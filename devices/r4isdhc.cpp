#include <cstring>
#include <algorithm>
#include <ncgcpp/ntrcard.h>

#include "../device.h"

namespace flashcart_core {
using platform::logMessage;
using platform::showProgress;

namespace {
union CmdBuf4 {
        uint32_t u32;
        uint8_t u8[4];
};
static_assert(sizeof(CmdBuf4) == 4, "Wrong union size");

constexpr uint64_t norCmd(const uint8_t outlen, const uint8_t inlen, const uint8_t cmd, const uint32_t addr, const uint8_t d1 = 0, const uint8_t d2 = 0) {
    // DD DD AA AA AA CC XY 99
    return 0x99ull | ((outlen & 0xFull) << 12) | ((inlen & 0xFull) << 8) |
        (cmd << 16) |
        ((addr & 0xFF0000ull) << 8) | ((addr & 0xFF00ull) << 24) | ((addr & 0xFFull) << 40) |
        (static_cast<uint64_t>(d1) << 48) | (static_cast<uint64_t>(d2) << 56);
}
static_assert(norCmd(2, 5, 0x3B, 0xABCDEF, 0x12, 0x34) == 0x3412EFCDAB3B2599, "norCmd result is wrong");

// i have no idea what else to name this...
constexpr uint64_t norRaw(const uint8_t d1, const uint8_t d2, const uint8_t fpgaParam = 0) {
    // 00 00 00 00 D2 D1 XY 99
    return 0x99ull | (static_cast<uint64_t>(fpgaParam) << 8) |
        (static_cast<uint64_t>(d1) << 16) | (static_cast<uint64_t>(d2) << 24);
}
static_assert(norRaw(0x34, 0x56, 0x12) == 0x56341299, "norRaw result is wrong");
}

class R4iSDHC : Flashcart {
    uint32_t norRead(const uint32_t address) {
        CmdBuf4 buf;
        m_card->sendCommand(norCmd(2, 5, 0x3B, address), buf.u8, 4, 0x180000);
        logMessage(LOG_DEBUG, "R4ISDHC: NOR read at %X returned %X", address, buf.u32);
        return buf.u32;
    }

    void norWriteEnable() {
        m_card->sendCommand(norCmd(0, 1, 6, 0), nullptr, 4, 0x180000);
        ncgc::delay(0x60000);
    }

    void norErase4k(const uint32_t address) {
        norWriteEnable();
        m_card->sendCommand(norCmd(0, 4, 0x20, address), nullptr, 4, 0x180000);
        ncgc::delay(41000000);
    }

    void norWrite256(const uint32_t address, const uint8_t *bytes) {
        norWriteEnable();
        m_card->sendCommand(norCmd(0, 6, 2, address, bytes[0], bytes[1]), nullptr, 4, 0x180000);
        for (uint32_t cur = 2; cur < 0x100; cur += 2) {
            m_card->sendCommand(norRaw(bytes[cur], bytes[cur+1]), nullptr, 4, 0x180000);
        }
        m_card->sendCommand(norRaw(bytes[0], bytes[1], 0xF0), nullptr, 4, 0x180000);
        ncgc::delay(0x60000);
    }

    void norWrite4k(const uint32_t address, const uint8_t *bytes) {
        uint32_t cur = 0;
        while (cur < 0x1000) {
            norWrite256(address + cur, bytes + cur);
            cur += 0x100;
        }
    }

    bool readNor(const uint32_t address, const uint32_t length, uint8_t *const buffer, bool progress = false) {
        uint32_t cur = 0;

        while (cur < length) {
            uint32_t rd = norRead(address + cur);
            std::memcpy(buffer + cur, &rd, std::min<uint32_t>(length - cur, sizeof(rd)));
            cur += 4;
            if (progress && cur % 0x4000) {
                showProgress(cur, length, "Reading NOR");
            }
        }

        return true;
    }

    bool writeNor(const uint32_t dest_address, const uint32_t length, const uint8_t *const src, bool progress = false,
                    const char *const progress_str = "Writing NOR") {
        const uint32_t real_start = dest_address & ~0xFFF;
        const uint32_t first_page_offset = dest_address & 0xFFF;
        const uint32_t real_length = ((length + first_page_offset) + 0xFFF) & ~0xFFF;
        uint32_t cur = 0;
        uint8_t buf[0x1000];

        if (progress) {
                showProgress(cur, real_length, progress_str);
        }

        while (cur < real_length) {
            const uint32_t cur_addr = real_start + cur;
            const uint32_t buf_ofs = ((dest_address > cur_addr) ? (dest_address - cur_addr) : 0);
            const uint32_t src_ofs = ((cur > first_page_offset) ? (cur - first_page_offset) : 0);
            const uint32_t len = std::min<uint32_t>(0x1000 - buf_ofs, std::min<uint32_t>(length - src_ofs, 0x1000));

            if (!readNor(cur_addr, 0x1000, buf)) {
                logMessage(LOG_ERR, "writeNor: failed to read");
                return false;
            }

            // don't write if they're already identical
            if (std::memcmp(buf + buf_ofs, src + src_ofs, len)) {
                norErase4k(cur_addr);
                // now ideally if i could read the NOR status register, i'd do the memcpy here
                // while the NOR does the sector erase, then just wait on it at the end. BUT NOPE!
                // (and the datasheet doesn't say this chip can read while writing, so that's not an option)

                bool success = false;
                uint32_t retry = 0;
                while (retry < 10) {
                    // some sanity checks..
                    success = norRead(cur_addr) == 0xFFFFFFFF &&
                        norRead(cur_addr + 0x1000 - 4) == 0xFFFFFFFF;
                    if (success) {
                        break;
                    }

                    showProgress(cur, real_length, "Waiting for NOR erase to finish");
                    ++retry;
                    logMessage(LOG_WARN, "writeNor: start or end isn't FF");
                    ncgc::delay(41000000);
                }

                if (!success) {
                    if (progress) {
                        showProgress(0, 1, "NOR erase sanity check failed");
                    }
                    return false;
                }

                std::memcpy(buf + buf_ofs, src + src_ofs, len);

                norWrite4k(cur_addr, buf);
            }

            cur += 0x1000;
            if (progress) {
                showProgress(cur, real_length, progress_str);
            }
        }

        uint32_t t = norRead(dest_address);
        if (std::memcmp(&t, src, std::min<uint32_t>(length, 4))) {
            if (progress) {
                showProgress(0, 1, "NOR write start verification failed");
            }
            logMessage(LOG_ERR, "writeNor: start mismatch after write");
            return false;
        }

        if (length > 4) {
            t = norRead(dest_address + length - 4);
            if (std::memcmp(&t, src + length - 4, 4)) {
                if (progress) {
                    showProgress(0, 1, "NOR write end verification failed");
                }
                logMessage(LOG_ERR, "writeNor: end mismatch after write");
                return false;
            }
        }
        return true;
    }

    bool checkCartType1() {
        CmdBuf4 buf;
        // this is actually the NOR write disable command
        // the r4isdhc will respond to cart commands with 0xFFFFFFFF if
        // the "magic" command hasn't been sent, so we check for that
        m_card->sendCommand(0x40199, buf.u8, 4, 0x180000);
        if (m_card->state() == ncgc::NTRState::Raw) {
            if (buf.u32 != 0xFFFFFFFF) {
                logMessage(LOG_ERR, "r4isdhc: checkCartType1: pre-test returned 0x%08X", buf.u32);
                return false;
            }
        }

        ncgc::Err err = m_card->init();
        if (err && !err.unsupported()) {
            logMessage(LOG_ERR, "r4isdhc: checkCartType1: ntrcard::init failed");
            return false;
        }

        // only type 1 carts support 0x68 command
        m_card->sendCommand(0x68, nullptr, 4, 0x180000, true);

        // now it will return zeroes
        m_card->sendCommand(0x40199, buf.u8, 4, 0x180000, true);
        if (buf.u32 == 0) {
            m_card->state(ncgc::NTRState::Raw);
            return true;
        }

        logMessage(LOG_ERR, "r4isdhc: checkCartType1: post-test returned 0x%08X", buf.u32);
        return false;
    }

    bool checkCartType2() {
        // this check only work on the activated BF key2
        if (m_card->state() != ncgc::NTRState::Key2) {
            logMessage(LOG_ERR, "r4isdhc: checkCartType2: status (%d) not KEY2",
                static_cast<uint32_t>(m_card->state()));
            return false;
        }

        CmdBuf4 buf;
        m_card->sendCommand(0x66, nullptr, 4, 0x586000, true);
        m_card->sendCommand(0x40199, buf.u8, 4, 0x180000, true);

        // FIXME this is a really poor check
        // a non-r4isdhc cart will stay in KEY2 and likely return something that isn't all-FF
        if (buf.u32 != 0xFFFFFFFF) {
            m_card->state(ncgc::NTRState::Raw);
            return true;
        }

        logMessage(LOG_ERR, "r4isdhc: checkCartType2: post-test returned 0x%08X", buf.u32);
        // i'm not sure if there's any point to this
        // the encryption state's probably desynced if the previous command returned 0xFFFFFFFF
        return false;
    }

    bool trySecureInit(BlowfishKey key) {
        ncgc::Err err = m_card->init();
        if (err && !err.unsupported()) {
            logMessage(LOG_ERR, "r4isdhc: trySecureInit: ntrcard::init failed");
            return false;
        } else if (m_card->state() != ncgc::NTRState::Raw) {
            logMessage(LOG_ERR, "r4isdhc: trySecureInit: status (%d) not RAW and cannot reset",
                static_cast<uint32_t>(m_card->state()));
            return false;
        }

        ncgc::c::ncgc_ncard_t& state = m_card->rawState();
        state.hdr.key1_romcnt = state.key1.romcnt = 0x81808F8;
        state.hdr.key2_romcnt = state.key2.romcnt = 0x416657;
        state.key2.seed_byte = 0;
        m_card->setBlowfishState(platform::getBlowfishKey(key), key != BlowfishKey::NTR);

        if ((err = m_card->beginKey1())) {
            logMessage(LOG_ERR, "r4isdhc: trySecureInit: init key1 (key = %d) failed: %d", static_cast<int>(key), err.errNo());
            return false;
        }
        if ((err = m_card->beginKey2())) {
            logMessage(LOG_ERR, "r4isdhc: trySecureInit: init key2 failed: %d", err.errNo());
            return false;
        }

        return checkCartType2();
    }

    uint8_t cart_type;

public:
    // Name & Size of Flash Memory
    R4iSDHC() : Flashcart("R4iSDHC family", 0x200000), cart_type(1) { }

    const char* getAuthor() {
        return
                    "handsomematt, Rai-chan, Kitlith,\n"
            "        stuckpixel, angelsl, et al.";
    }

    const char* getDescription() {
        return
            "\n"
            "A family of DSTT clones. Tested with:\n"
            " * R4iSDHC RTS Lite (r4isdhc.com)\n"
            " * R4i-SDHC 3DS RTS (r4i-sdhc.com)\n"
            " * R4i-SDHC B9S (r4i-sdhc.com)";
    }

    bool initialize() {
        if (checkCartType1()) {
            cart_type = 1;
        } else {
            switch (m_card->state()) {
                case ncgc::NTRState::Raw:
                    if (!trySecureInit(BlowfishKey::NTR)
                        && !trySecureInit(BlowfishKey::B9Retail)
                        && !trySecureInit(BlowfishKey::B9Dev)) {
                        logMessage(LOG_DEBUG, "r4isdhc: type 2 init from RAW fail");
                        return false;
                    }
                    break;
                case ncgc::NTRState::Key2:
                    if (!checkCartType2()) {
                        logMessage(LOG_DEBUG, "r4isdhc: type 2 init from KEY2 fail");
                        return false;
                    }
                    break;
                default:
                    logMessage(LOG_ERR, "r4isdhc: Unexpected encryption status %d", m_card->state());
                    return false;
            }
            cart_type = 2;
        }

        uint32_t read1 = norRead(0), read2 = norRead(0);
        if (read1 != read2) {
            logMessage(LOG_ERR, "r4isdhc: two reads from flash @ 0 returned 0x%08lX and 0x%08lX", read1, read2);
            return false;
        }

        logMessage(LOG_ERR, "r4isdhc: found type %d cart", cart_type);
        return true;
    }

    void shutdown() { }

    bool readFlash(const uint32_t address, const uint32_t length, uint8_t *const buffer) override {
        return readNor(address, length, buffer, true);
    }

    bool writeFlash(const uint32_t address, const uint32_t length, const uint8_t *const buffer) override {
        return writeNor(address, length, buffer, true);
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) override {
        // FIRM is written at 0x7E00; blowfish key at 0x1F1000
        // N.B. this doesn't necessarily mean that the cart's ROM => NOR mapping will
        // allow a FIRM of this size (i.e. old carts), it's just so we don't overwrite
        // the blowfish key
        if (firm_size > (0x1F1000 - 0x7E00)) {
            showProgress(0, 1, "FIRM too big (max 2003456 bytes)");
            return false;
        }

        uint8_t map[0x100] = {0};
        // set the 2nd ROM map to some high value (0x7FFFFFFF in big-endian)
        map[4] = 0x7F; map[5] = 0xFF; map[6] = 0xFF; map[7] = 0xFF;
        return
            // 1:1 map the ROM <=> NOR (unless it's an "old" cart - those don't seem to have
            // a mapping in the NOR)
            writeNor(0x1000, 0x48, blowfish_key, true, "Writing Blowfish key (1)") && // blowfish P array
            writeNor(0x2000, 0x1000, blowfish_key+0x48, true, "Writing Blowfish key (2)") && // blowfish S boxes
            (
                (cart_type == 1 && writeNor(0x40, 0x100, map, true, "Writing ROM <=> NOR map")) ||
                (cart_type == 2 && true) // type 2 not need ROM-NOR map
            ) &&
            writeNor(0x1F1000, 0x48, blowfish_key, true, "Writing Blowfish key (3)") && // blowfish P array
            writeNor(0x1F2000, 0x1000, blowfish_key+0x48, true, "Writing Blowfish key (4)") && // blowfish S boxes
            writeNor(0x7E00, firm_size, firm, true, "Writing FIRM (1)") && // FIRM
            // type2 carts read 0x8000-0x10000 from 0x1F8000-0x200000 instead of from 0x8000
            writeNor(0x1F7E00, std::min<uint32_t>(firm_size, (cart_type == 1 ? 0x200 : 0x8200)), firm, true,
                "Writing FIRM (2)"); // FIRM header
    }
};

R4iSDHC r4isdhc;
}
