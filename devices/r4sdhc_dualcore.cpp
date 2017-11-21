#include <cstring>
#include <algorithm>

#include "../device.h"

#define BIT(n) (1 << (n))

namespace flashcart_core {
using platform::logMessage;
using platform::showProgress;

class R4SDHC_DualCore : Flashcart {
private:
    static const uint8_t cmdGetSWRev[8];
    static const uint8_t cmdReadFlash[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdWaitFlashBusy[8];

    static const uint8_t cmdUnkB7[8];
    static const uint8_t cmdUnkD0AA[8];
    static const uint8_t cmdUnkD0[8];

    uint8_t decrypt(uint8_t enc) {
        uint8_t dec = 0;
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

    uint8_t encrypt(uint8_t dec) {
        uint8_t enc = 0;
        dec ^= 0x98;
        if (dec & BIT(0)) enc |= BIT(6);
        if (dec & BIT(1)) enc |= BIT(2);
        if (dec & BIT(2)) enc |= BIT(7);
        if (dec & BIT(3)) enc |= BIT(3);
        if (dec & BIT(4)) enc |= BIT(1);
        if (dec & BIT(5)) enc |= BIT(0);
        if (dec & BIT(6)) enc |= BIT(4);
        if (dec & BIT(7)) enc |= BIT(5);
        return enc;
    }

    void read_cmd(uint32_t address, uint8_t *resp)
    {
      uint8_t cmdbuf[8];

      memcpy(cmdbuf, cmdReadFlash, 8);

      cmdbuf[2] = (address >> 16) & 0x1F;
      cmdbuf[3] = (address >>  8) & 0xFF;
      cmdbuf[4] = (address >>  0) & 0xFF;

      m_card->sendCommand(cmdbuf, resp, 0x200, 80);
    }

    void wait_flash_busy(void)
    {
      uint8_t cmdbuf[8];
      uint32_t resp = 0;
      memcpy(cmdbuf, cmdWaitFlashBusy, 8);

      do {
        m_card->sendCommand(cmdbuf, (uint8_t *)&resp, 4, 80);
      } while(resp);
    }

    void erase_cmd(uint32_t address) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4SDHC: erase(0x%08x)", address);
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[2] = (address >> 16) & 0xFF;
        cmdbuf[3] = (address >>  8) & 0xFF;
        cmdbuf[4] = (address >>  0) & 0xFF;

        m_card->sendCommand(cmdbuf, nullptr, 0, 80);
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

        m_card->sendCommand(cmdbuf, nullptr, 0, 80);
        wait_flash_busy();
    }

    bool trySecureInit(BlowfishKey key) {
        ncgc::Err err = m_card->init();
        if (err && !err.unsupported()) {
            logMessage(LOG_ERR, "R4SDHC: trySecureInit: ntrcard::init failed");
            return false;
        } else if (m_card->state() != ncgc::NTRState::Raw) {
            logMessage(LOG_ERR, "R4SDHC: trySecureInit: status (%d) not RAW and cannot reset",
                static_cast<uint32_t>(m_card->state()));
            return false;
        }

        ncgc::c::ncgc_ncard_t& state = m_card->rawState();
        state.hdr.key1_romcnt = state.key1.romcnt = 0x1808F8;
        state.hdr.key2_romcnt = state.key2.romcnt = 0x416017;
        state.key2.seed_byte = 0;
        m_card->setBlowfishState(platform::getBlowfishKey(key), key != BlowfishKey::NTR);
        if ((err = m_card->beginKey1())) {
            logMessage(LOG_ERR, "R4SDHC: trySecureInit: init key1 (key = %d) failed: %d", static_cast<int>(key), err.errNo());
            return false;
        }
        if ((err = m_card->beginKey2())) {
            logMessage(LOG_ERR, "R4SDHC: trySecureInit: init key2 failed: %d", err.errNo());
            return false;
        }

        return true;
    }

public:
    R4SDHC_DualCore() : Flashcart("R4 SDHC Dual Core", 0x200000) { }

    const char* getAuthor() {
        return
                    "handsomematt, Normmatt, Kitlith,\n"
            "        stuckpixel, angelsl, et al.";
    }

    bool initialize() {
        logMessage(LOG_INFO, "R4SDHC: Init");

        if (!trySecureInit(BlowfishKey::NTR) && !trySecureInit(BlowfishKey::B9Retail) && !trySecureInit(BlowfishKey::B9Dev))
        {
          logMessage(LOG_ERR, "R4SDHC: Secure init failed!");
          return false;
        }

        uint32_t resp1[0x200/4];
        uint32_t resp2[0x200/4];

        //this is how the updater does it. Not sure exactly what it's for
        do {
          m_card->sendCommand(cmdUnkB7, resp1, 0x200, 80);
          m_card->sendCommand(cmdUnkB7, resp2, 0x200, 80);
          logMessage(LOG_DEBUG, "resp1: 0x%08x, resp2: 0x%08x", *resp1, *resp2);
        } while(std::memcmp(resp1, resp2, 0x200));

        uint32_t sw_rev;

        m_card->sendCommand(cmdGetSWRev, &sw_rev, 4, 80);

        logMessage(LOG_INFO, "R4SDHC: Current Software Revsion: %08x", sw_rev);

        m_card->sendCommand(cmdUnkD0AA, nullptr, 4, 80);
        m_card->sendCommand(cmdUnkD0AA, nullptr, 4, 80);
        m_card->sendCommand(cmdUnkD0, nullptr, 0, 80);
        m_card->sendCommand(cmdUnkD0AA, nullptr, 4, 80);

        do {
          m_card->sendCommand(cmdUnkB7, resp1, 0x200, 80);
          m_card->sendCommand(cmdUnkB7, resp2, 0x200, 80);
        } while(std::memcmp(resp1, resp2, 0x200));

        logMessage(LOG_WARN, "R4SDHC: We have no way of detecting this cart!");

        return true; // We have no way of checking yet.
    }
    void shutdown() {
        logMessage(LOG_INFO, "R4SDHC: Shutdown");
    }

    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
        logMessage(LOG_INFO, "R4SDHC: readFlash(addr=0x%08x, size=0x%x)", address, length);
        for(uint32_t addr = 0; addr < length; addr += 0x200)
        {
          read_cmd(addr + address, buffer + addr);
          showProgress(addr, length, "Reading");
          for(int i = 0; i < 0x200; i++)
            /*the read command encrypts the raw flash contents before returning it you*/
            /*so to get the raw flash contents, decrypt the returned values*/
            *(buffer + addr + i) = decrypt(*(buffer + addr + i));
        }
        return true;
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
        logMessage(LOG_INFO, "R4SDHC: writeFlash(addr=0x%08x, size=0x%x)", address, length);
        for (uint32_t addr=0; addr < length; addr+=0x10000)
        {
            erase_cmd(address + addr);
            showProgress(addr, length, "Erasing");
        }

        for (uint32_t i=0; i < length; i++) {
            /*the write command decrypts whatever you send it before actually writing to flash*/
            /*so we encrypt whatever we send to be written*/
            uint8_t byte = encrypt(buffer[i]);
            write_cmd(address + i, byte);
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

const uint8_t R4SDHC_DualCore::cmdUnkB7[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00}; //reads flash at offset 0x40000
const uint8_t R4SDHC_DualCore::cmdUnkD0AA[8] = {0xD0, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdUnkD0[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t R4SDHC_DualCore::cmdGetSWRev[8] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdReadFlash[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

R4SDHC_DualCore r4sdhc_dualcore;
}
