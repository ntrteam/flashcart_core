#include "device.h"

#include <cstring>
#include <algorithm>

#define BIT(n) (1 << (n))

namespace flashcart_core {
using ntrcard::sendCommand;
using platform::logMessage;
using platform::showProgress;

class R4SDHC_DualCore : Flashcart {
private:
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];

    static const uint8_t cmdUnkD0AA[8];
    static const uint8_t cmdUnkD0[8];

    uint8_t decrypt(uint8_t enc) {
        uint8_t dec = 0;
        enc ^= 0x98;
        if (enc & BIT(0)) dec |= BIT(6);
        if (enc & BIT(1)) dec |= BIT(2);
        if (enc & BIT(2)) dec |= BIT(7);
        if (enc & BIT(3)) dec |= BIT(3);
        if (enc & BIT(4)) dec |= BIT(1);
        if (enc & BIT(5)) dec |= BIT(0);
        if (enc & BIT(6)) dec |= BIT(4);
        if (enc & BIT(7)) dec |= BIT(5);
        return dec;
    }

    uint8_t encrypt(uint8_t dec) {
        uint8_t enc = 0;
        if (dec & BIT(0)) enc |= BIT(5);
        if (dec & BIT(1)) enc |= BIT(4);
        if (dec & BIT(2)) enc |= BIT(1);
        if (dec & BIT(3)) enc |= BIT(3);
        if (dec & BIT(4)) enc |= BIT(6);
        if (dec & BIT(5)) enc |= BIT(7);
        if (dec & BIT(6)) enc |= BIT(0);
        if (dec & BIT(7)) enc |= BIT(2);
        enc ^= 0x98;
        return enc;
    }

    void erase_cmd(uint32_t address) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4SDHC: erase(0x%08x)", address);
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0, nullptr); // TODO: find IDB and get the latencies.
    }

    void write_cmd(uint32_t address, uint8_t value) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4SDHC: write(0x%08x) = 0x%02x", address, value);
        memcpy(cmdbuf, cmdWriteByteFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;
        cmdbuf[4] = value;

        sendCommand(cmdbuf, 0, nullptr);
    }

public:
    R4SDHC_DualCore() : Flashcart("R4 SDHC Dual Core", 0x400000) { }

    bool initialize() {
        uint8_t dummy[4];

        logMessage(LOG_INFO, "R4SDHC: Init");
        sendCommand(cmdUnkD0AA, 4, dummy);
        sendCommand(cmdUnkD0, 0, nullptr);
        sendCommand(cmdUnkD0AA, 4, dummy);
        sendCommand(cmdUnkD0AA, 4, dummy);

        logMessage(LOG_WARN, "R4SDHC: We have no way of detecting cart!");
        return true; // We have no way of checking yet.
    }
    void shutdown() {
        logMessage(LOG_INFO, "R4SDHC: Shutdown");
    }

    // We don't have a read command...
    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
        logMessage(LOG_ERR, "R4SDHC: readFlash not implemented!");
        return false;
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
        logMessage(LOG_INFO, "R4SDHC: writeFlash(addr=0x%08x, size=0x%x)", address, length);
        for (uint32_t addr=0; addr < length; addr+=0x10000)
            erase_cmd(address + addr);

        for (uint32_t i=0; i < length; i++) {
            write_cmd(address + i, buffer[i]);
            showProgress(i,length, "Writing");
        }

        return true;
    }

    // Need to find offsets first.
    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) {
        logMessage(LOG_ERR, "R4SDHC: ntrboot injection not implemented!");
        return false;
    }
};

const uint8_t R4SDHC_DualCore::cmdUnkD0AA[8] = {0xD0, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdUnkD0[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t R4SDHC_DualCore::cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};

// R4SDHC_DualCore r4sdhc_dualcore;
}
