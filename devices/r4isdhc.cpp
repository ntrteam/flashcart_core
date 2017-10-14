#include <cstring>
#include <algorithm>

#include "device.h"

namespace flashcart_core {
using ntrcard::sendCommand;
using platform::logMessage;
using platform::ioDelay;
using platform::showProgress;

namespace {
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

    uint32_t norRead(const uint32_t address) {
        uint32_t ret = 0;
        sendCommand(norCmd(2, 5, 0x3B, address), 4, reinterpret_cast<uint8_t *>(&ret), 0x180000);
        logMessage(LOG_DEBUG, "R4ISDHC: NOR read at %X returned %X", address, ret);
        return ret;
    }

    void norWriteEnable() {
        sendCommand(norCmd(0, 1, 6, 0), 4, nullptr, 0x180000);
        ioDelay(0x60000);
    }

    void norErase4k(const uint32_t address) {
        norWriteEnable();
        sendCommand(norCmd(0, 4, 0x20, address), 4, nullptr, 0x180000);
        ioDelay(41000000);
    }

    void norWrite256(const uint32_t address, const uint8_t *bytes) {
        norWriteEnable();
        sendCommand(norCmd(0, 6, 2, address, bytes[0], bytes[1]), 4, nullptr, 0x180000);
        for (uint32_t cur = 2; cur < 0x100; cur += 2) {
            sendCommand(norRaw(bytes[cur], bytes[cur+1]), 4, nullptr, 0x180000);
        }
        sendCommand(norRaw(bytes[0], bytes[1], 0xF0), 4, nullptr, 0x180000);
        ioDelay(0x60000);
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

    bool writeNor(const uint32_t dest_address, const uint32_t length, const uint8_t *const src, bool progress = false) {
        const uint32_t real_start = dest_address & ~0xFFF;
        const uint32_t first_page_offset = dest_address & 0xFFF;
        const uint32_t real_length = ((length + first_page_offset) + 0xFFF) & ~0xFFF;
        uint32_t cur = 0;
        uint8_t buf[0x1000];

        if (progress) {
                showProgress(cur, real_length, "Writing NOR");
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
                    ioDelay(41000000);
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
                showProgress(cur, real_length, "Writing NOR");
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
}

class R4iSDHC : Flashcart {
public:
    // Name & Size of Flash Memory
    R4iSDHC() : Flashcart("R4iSDHC family", 0x200000) { }

    const char* getAuthor() { return ""; }
    const char* getDescription() { return ""; }

    bool initialize() {
        sendCommand(0x68, 4, nullptr, 0x180000);
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
        return false;
    }
};

R4iSDHC r4isdhcrtsl;
}
