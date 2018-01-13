#include <cstring>
#include <algorithm>
#include <ncgcpp/ntrcard.h>

#include "../device.h"
#include "../flash_util.h"

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

    bool norRead(uint32_t addr, uint32_t size, void *dest) {
        uint32_t res = norRead(addr);
        std::memcpy(dest, &res, std::min<uint32_t>(size, 4));

        return true;
    }

    void norWriteEnable() {
        m_card->sendCommand(norCmd(0, 1, 6, 0), nullptr, 4, 0x180000);
        ncgc::delay(0x60000);
    }

    bool norErase4k(const uint32_t address) {
        norWriteEnable();
        m_card->sendCommand(norCmd(0, 4, 0x20, address), nullptr, 4, 0x180000);
        ncgc::delay(41000000);

        // now ideally if i could read the NOR status register, i'd do the memcpy here
        // while the NOR does the sector erase, then just wait on it at the end. BUT NOPE!
        // (and the datasheet doesn't say this chip can read while writing, so that's not an option)

        bool success = false;
        uint32_t retry = 0;
        while (retry < 10) {
            // some sanity checks..
            success = norRead(address) == 0xFFFFFFFF &&
                norRead(address + 0x1000 - 4) == 0xFFFFFFFF;
            if (success) {
                break;
            }

            ++retry;
            logMessage(LOG_WARN, "r4isdhc: norErase4k: start or end isn't FF");
            ncgc::delay(41000000);
        }

        return success;
    }

    bool norWrite256(const uint32_t address, const void *src) {
        const uint8_t *bytes = static_cast<const uint8_t *>(src);
        norWriteEnable();
        m_card->sendCommand(norCmd(0, 6, 2, address, bytes[0], bytes[1]), nullptr, 4, 0x180000);
        for (uint32_t cur = 2; cur < 0x100; cur += 2) {
            m_card->sendCommand(norRaw(bytes[cur], bytes[cur+1]), nullptr, 4, 0x180000);
        }
        m_card->sendCommand(norRaw(bytes[0], bytes[1], 0xF0), nullptr, 4, 0x180000);
        ncgc::delay(0x60000);

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

    using Util = FlashUtil<R4iSDHC, 2, &R4iSDHC::norRead, 12, &R4iSDHC::norErase4k, 8, &R4iSDHC::norWrite256>;

public:
    // Name & Size of Flash Memory
    R4iSDHC() : Flashcart("R4iSDHC family", "r4isdhc", 0x200000), cart_type(1) { }

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
            " * R4i-SDHC B9S (r4i-sdhc.com)\n"
            "\n"
            "Does not include the Dual-Core 2013 varient.";
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
        return Util::read(this, address, length, buffer, true);
    }

    bool writeFlash(const uint32_t address, const uint32_t length, const uint8_t *const buffer) override {
        return Util::write(this, address, length, buffer, true);
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
            Util::write(this, 0x1000, 0x48, blowfish_key, true, "Writing Blowfish key (1)") && // blowfish P array
            Util::write(this, 0x2000, 0x1000, blowfish_key+0x48, true, "Writing Blowfish key (2)") && // blowfish S boxes
            (
                (cart_type == 1 && Util::write(this, 0x40, 0x100, map, true, "Writing ROM <=> NOR map")) ||
                (cart_type == 2 && true) // type 2 not need ROM-NOR map
            ) &&
            Util::write(this, 0x1F1000, 0x48, blowfish_key, true, "Writing Blowfish key (3)") && // blowfish P array
            Util::write(this, 0x1F2000, 0x1000, blowfish_key+0x48, true, "Writing Blowfish key (4)") && // blowfish S boxes
            Util::write(this, 0x7E00, firm_size, firm, true, "Writing FIRM (1)") && // FIRM
            // type2 carts read 0x8000-0x10000 from 0x1F8000-0x200000 instead of from 0x8000
            Util::write(this, 0x1F7E00, std::min<uint32_t>(firm_size, (cart_type == 1 ? 0x200 : 0x8200)), firm, true,
                "Writing FIRM (2)"); // FIRM header
    }
};

R4iSDHC r4isdhc;
}
