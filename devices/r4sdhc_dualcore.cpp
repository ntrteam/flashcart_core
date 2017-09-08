#include "device.h"

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
        if (enc & BIT(0)) dec |= BIT(5);
        if (enc & BIT(1)) dec |= BIT(4);
        if (enc & BIT(2)) dec |= BIT(1);
        if (enc & BIT(3)) dec |= BIT(3);
        if (enc & BIT(4)) dec |= BIT(6);
        if (enc & BIT(5)) dec |= BIT(7);
        if (enc & BIT(6)) dec |= BIT(0);
        if (enc & BIT(7)) dec |= BIT(2);
        dec ^= 0x98;
        return dec;
    }

    void erase_cmd(uint32_t address) {
        uint8_t cmdbuf[8];
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0, nullptr); // TODO: find IDB and get the latencies.
    }

    void write_cmd(uint32_t address, uint8_t value) {
        uint8_t cmdbuf[8];
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

        sendCommand(cmdUnkD0AA, 4, dummy);
        sendCommand(cmdUnkD0, 0, nullptr);
        sendCommand(cmdUnkD0AA, 4, dummy);
        sendCommand(cmdUnkD0AA, 4, dummy);

        return true; // We have no way of checking...
    }
    void shutdown() { }

    // We don't have a read command...
    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) { return false; }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
        for (uint32_t addr=0; addr < length; addr+=0x10000)
            erase_cmd(address + addr);

        for (uint32_t i=0; i < length; i++) {
            write_cmd(address + i, buffer[i]);
            showProgress(i,length, "Writing");
        }
    }

    // Need to find offsets first.
    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { return false; }
};

const uint8_t R4SDHC_DualCore::cmdUnkD0AA[8] = {0xD0, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdUnkD0[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t R4SDHC_DualCore::cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};

// R4SDHC_DualCore r4sdhc_dualcore;
