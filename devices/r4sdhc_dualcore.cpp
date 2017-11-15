#include <cstring>
#include <algorithm>

#include "../device.h"

#define BIT(n) (1 << (n))

namespace flashcart_core {
using ntrcard::sendCommand;
using ntrcard::BlowfishKey;
using platform::logMessage;
using platform::showProgress;

class R4SDHC_DualCore : Flashcart {
private:
    static const uint8_t cmdGetSWRev[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdWaitFlashBusy[8];

    static const uint8_t cmdUnkB7[8];
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

    void wait_flash_busy(void)
    {
      uint8_t cmdbuf[8];
      uint32_t resp = 0;
      memcpy(cmdbuf, cmdWaitFlashBusy, 8);

      do {
        sendCommand(cmdbuf, 4, (uint8_t *)&resp, 80);
      } while(resp);
    }

    void erase_cmd(uint32_t address) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4SDHC: erase(0x%08x)", address);
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0, nullptr, 80); // TODO: find IDB and get the latencies.
        wait_flash_busy();
    }

    void write_cmd(uint32_t address, uint8_t value) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4SDHC: write(0x%08x) = 0x%02x", address, value);
        memcpy(cmdbuf, cmdWriteByteFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;
        cmdbuf[4] = value;

        sendCommand(cmdbuf, 0, nullptr, 80);
    }

public:
    R4SDHC_DualCore() : Flashcart("R4 SDHC Dual Core", 0x200000) { }

    bool initialize() {
        logMessage(LOG_INFO, "R4SDHC: Init");

        if(!ntrcard::init())
        {
          logMessage(LOG_ERR, "R4SDHC: Init failed!");
          return false;
        }

        if(!ntrcard::initKey1(BlowfishKey::NTR))
        {
          logMessage(LOG_ERR, "R4SDHC: Key1 init failed!");
          return false;
        }

        if(!ntrcard::initKey2())
        {
          logMessage(LOG_ERR, "R4SDHC: Key2 init failed!");
          return false;
        }

        uint8_t resp1[0x200];
        uint8_t resp2[0x200];


        //this is how the updater does it. Not sure exactly what it's for
        do {
          sendCommand(cmdUnkB7, 0x200, resp1, 80);
          sendCommand(cmdUnkB7, 0x200, resp2, 80);
          logMessage(LOG_DEBUG, "resp1: 0x%08x, resp2: 0x%08x", *(uint32_t *)resp1, *(uint32_t *)resp2);
        } while(std::memcmp(resp1, resp2, 0x200));

        uint8_t sw_rev[4];

        sendCommand(cmdGetSWRev, 4, sw_rev, 80);

        logMessage(LOG_INFO, "R4SDHC: Current Software Revsion: %08x", *(uint32_t *)sw_rev);

        uint8_t dummy[4];

        sendCommand(cmdUnkD0AA, 4, dummy, 80);
        sendCommand(cmdUnkD0AA, 4, dummy, 80);
        sendCommand(cmdUnkD0, 0, nullptr, 80);
        sendCommand(cmdUnkD0AA, 4, dummy, 80);

        do {
          sendCommand(cmdUnkB7, 0x200, resp1, 80);
          sendCommand(cmdUnkB7, 0x200, resp2, 80);
        } while(std::memcmp(resp1, resp2, 0x200));

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

const uint8_t R4SDHC_DualCore::cmdUnkB7[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00}; //possibly read flash?
const uint8_t R4SDHC_DualCore::cmdUnkD0AA[8] = {0xD0, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdUnkD0[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t R4SDHC_DualCore::cmdGetSWRev[8] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

R4SDHC_DualCore r4sdhc_dualcore;
}
