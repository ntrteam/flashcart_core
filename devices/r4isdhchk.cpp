#include <cstring>
#include <algorithm>

#include "../device.h"

#define BIT(n) (1 << (n))

namespace flashcart_core {
using platform::logMessage;
using platform::showProgress;

class R4iSDHCHK : Flashcart {
private:
    static const uint8_t cmdGetSWRev[8];
    static const uint8_t cmdReadFlash506[8];
    static const uint8_t cmdReadFlash700[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdWaitFlashBusy[8];

    static const uint8_t cmdGetCartUniqueKey[8];
    static const uint8_t cmdUnkD0AA[8];
    static const uint8_t cmdGetChipID[8];

    static uint32_t sw_rev;

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

    void encrypt_memcpy(uint8_t * dst, uint8_t * src, uint32_t length) {
        for(uint32_t i = 0; i < length; ++i) {
            dst[i] = encrypt(src[i]);
        }
    }

    void read_cmd(uint32_t address, uint8_t *resp) {
        uint8_t cmdbuf[8];

        switch (sw_rev) {
            case 0x00000505:
                /*placeholder if going to be supported in the future. There are no reports that this revision currently exists.*/
                return;
            case 0x00000605:
                address = address + 0x610000;
                memcpy(cmdbuf, cmdReadFlash506, 8);
                break;
            case 0x00000007:
                case 0x00000707:
                memcpy(cmdbuf, cmdReadFlash700, 8);
                break;
            default:
                return;
        }

      cmdbuf[2] = (address >> 16) & 0x1F;
      cmdbuf[3] = (address >>  8) & 0xFF;
      cmdbuf[4] = (address >>  0) & 0xFF;

      m_card->sendCommand(cmdbuf, resp, 0x200, 80);
    }

    void wait_flash_busy(void) {
      uint8_t cmdbuf[8];
      uint32_t resp = 0;
      memcpy(cmdbuf, cmdWaitFlashBusy, 8);

      do {
          m_card->sendCommand(cmdbuf, (uint8_t *)&resp, 4, 80);
      } while(resp);
    }

    void erase_cmd(uint32_t address) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "r4isdhc.hk: erase(0x%08x)", address);
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        m_card->sendCommand(cmdbuf, nullptr, 0, 80);
        wait_flash_busy();
    }

    void write_cmd(uint32_t address, uint8_t value) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "r4isdhc.hk: write(0x%08x) = 0x%02x", address, value);
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
            logMessage(LOG_ERR, "r4isdhc.hk: trySecureInit: ntrcard::init failed");
            return false;
        } else if (m_card->state() != ncgc::NTRState::Raw) {
            logMessage(LOG_ERR, "r4isdhc.hk: trySecureInit: status (%d) not RAW and cannot reset",
                static_cast<uint32_t>(m_card->state()));
            return false;
        }

        ncgc::c::ncgc_ncard_t& state = m_card->rawState();
        state.hdr.key1_romcnt = state.key1.romcnt = 0x1808F8;
        state.hdr.key2_romcnt = state.key2.romcnt = 0x416017;
        state.key2.seed_byte = 0;
        m_card->setBlowfishState(platform::getBlowfishKey(key), key != BlowfishKey::NTR);
        if ((err = m_card->beginKey1())) {
            logMessage(LOG_ERR, "r4isdhc.hk: trySecureInit: init key1 (key = %d) failed: %d", static_cast<int>(key), err.errNo());
            return false;
        }
        if ((err = m_card->beginKey2())) {
            logMessage(LOG_ERR, "r4isdhc.hk: trySecureInit: init key2 failed: %d", err.errNo());
            return false;
        }

        return true;
    }

    void injectFlash(uint32_t chunk_addr, uint32_t chunk_length, uint32_t offset, uint8_t *src, uint32_t src_length, bool encryption) {
        uint8_t *chunk = (uint8_t *)malloc(chunk_length);
        readFlash(chunk_addr, chunk_length, chunk);
        if (encryption) {
            encrypt_memcpy(chunk + offset, src, src_length);
        } else {
            memcpy(chunk + offset, src, src_length);
        }
        writeFlash(chunk_addr, chunk_length, chunk);
        free(chunk);
    }

public:
    R4iSDHCHK() : Flashcart("R4 SDHC Dual-Core", "R4iSDHC.hk", 0x200000) { }

    const char * getAuthor() {
        return
                    "Normmatt, Kitlith, stuckpixel,\n"
                    "       angelsl, EleventhSign, et al.\n";
    }
    const char * getDescription() {
        return "\n"
               "Works with several carts similar to the r4isdhc.hk\n"
               " * R4 SDHC Dual-Core (r4isdhc.hk)\n"
               " * R4i Gold (r4igold.cc)\n"
               " * R4iTT 3DS (r4itt.net)\n";
    }

    bool initialize() {
        logMessage(LOG_INFO, "r4isdhc.hk: Init");

        if (!trySecureInit(BlowfishKey::NTR) && !trySecureInit(BlowfishKey::B9Retail) && !trySecureInit(BlowfishKey::B9Dev))
        {
          logMessage(LOG_ERR, "r4isdhc.hk: Secure init failed!");
          return false;
        }

        uint32_t resp1[0x200/4];
        uint32_t resp2[0x200/4];

        //this is how the updater does it. Not sure exactly what it's for
        do {
          m_card->sendCommand(cmdGetCartUniqueKey, resp1, 0x200, 80);
          m_card->sendCommand(cmdGetCartUniqueKey, resp2, 0x200, 80);
          logMessage(LOG_DEBUG, "resp1: 0x%08x, resp2: 0x%08x", *resp1, *resp2);
        } while(std::memcmp(resp1, resp2, 0x200));

        m_card->sendCommand(cmdGetSWRev, &sw_rev, 4, 80);

        logMessage(LOG_INFO, "r4isdhc.hk: Current Software Revision: %08x", sw_rev);

        m_card->sendCommand(cmdUnkD0AA, nullptr, 4, 80);
        m_card->sendCommand(cmdUnkD0AA, nullptr, 4, 80);
        m_card->sendCommand(cmdGetChipID, nullptr, 0, 80);
        m_card->sendCommand(cmdUnkD0AA, nullptr, 4, 80);

        do {
          m_card->sendCommand(cmdGetCartUniqueKey, resp1, 0x200, 80);
          m_card->sendCommand(cmdGetCartUniqueKey, resp2, 0x200, 80);
        } while(std::memcmp(resp1, resp2, 0x200));
        
        switch (sw_rev) {
            case 0x00000505:
                logMessage(LOG_ERR, "r4isdhc.hk: Anything below 0x00000605 is not supported.");
                return false;
            case 0x00000605:
            case 0x00000007:
            case 0x00000707:
                break;
            default:
                logMessage(LOG_ERR, "r4isdhc.hk: 0x%08x is not a recognized version and is therefore not supported.", sw_rev);
                return false;
        }

        logMessage(LOG_NOTICE, "r4isdhc.hk: SW Revision = %08x", sw_rev);
        return true;
    }
 
    void shutdown() {
        logMessage(LOG_INFO, "r4isdhc.hk: Shutdown");
    }

    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
        logMessage(LOG_INFO, "r4isdhc.hk: readFlash(addr=0x%08x, size=0x%x)", address, length);
        for(uint32_t addr = 0; addr < length; addr += 0x200)
        {
            read_cmd(addr + address, buffer + addr);
            showProgress(addr, length, "Reading");
            for(int i = 0; i < 0x200; i++)
                /*the read command decrypts the raw flash contents before returning it you*/
                /*so to get the raw flash contents, encrypt the returned values*/
                *(buffer + addr + i) = encrypt(*(buffer + addr + i));
        }
        return true;
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
        logMessage(LOG_INFO, "r4isdhc.hk: writeFlash(addr=0x%08x, size=0x%x)", address, length);
        for (uint32_t addr=0; addr < length; addr+=0x10000) {
           erase_cmd(address + addr);
           showProgress(addr, length, "Erasing");
        }

        for (uint32_t i=0; i < length; i++) {
            /*the write command encrypts whatever you send it before actually writing to flash*/
            /*so we decrypt whatever we send to be written*/
            uint8_t byte = decrypt(buffer[i]);
            write_cmd(address + i, byte);
            showProgress(i,length, "Writing");
        }

        return true;
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) {
        logMessage(LOG_INFO, "r4isdhc.hk: Injecting ntrboot");
        uint8_t *block_0 = (uint8_t *)malloc(0x10000);
        uint32_t buf_size = PAGE_ROUND_UP(firm_size - 0x200 + 0x000000, 0x10000);
        uint8_t gameHeader[0x200];

        logMessage(LOG_INFO, "r4isdhc.hk: Patch firmware (header)");
        readFlash(0, 0x10000, block_0);

        switch (sw_rev) {
            case 0x00000505:
                /*placeholder if going to be supported in the future. There are no reports that this revision currently exists.*/
                return false;
            case 0x00000605: {
                /*Modify the PicoBlaze 3 instruction (aka cart header) to remap the following in flash:*/
                /*game header from 0x11100, move to 0x1000 (len = 200h)*/
                block_0[0x1B0] = 0;                 block_0[0x1B9] = 0x10;

                /*blowfish key from 0x11100, move to 0x1600 (len = 1048h)*/
                block_0[0x1F8] = 0;                 block_0[0x1FE] = 0x16;
                block_0[0x25B] = 0;                 block_0[0x261] = 0x16;
                block_0[0x2AC] = 0;                 block_0[0x2B2] = 0x16;
                block_0[0x309] = 0;                 block_0[0x30F] = 0x16;
                block_0[0x35D] = 0;                 block_0[0x363] = 0x16;
                block_0[0x3D2] = 0;                 block_0[0x3D8] = 0x16;
                block_0[0x41A] = 0;                 block_0[0x420] = 0x16;

                /*secure area (7k) from 0x14700, move to 0x3000 (len = 1100h)*/
                block_0[0x4EF] = 0;                 block_0[0x4F2] = 0x30;

                /*main data area (8k) from 0x30000, move to 0x5000 (len = 7600h)*/
                block_0[0x645] = 0x30;              block_0[0x646] = 0xC0;
                block_0[0x648] = 0;                 block_0[0x649] = 0xE1;

                break;
                }
            case 0x00000007:
            case 0x00000707: {
                /*Modify the PicoBlaze 3 instruction (aka cart header) to remap the following in flash:*/
                /*game header from 0x11100, move to 0x1000 (len = 200h)*/
                block_0[0x189] = 0;                 block_0[0x192] = 0x10;

                /*blowfish key from 0x11100, move to 0x1600 (len = 1048h)*/
                block_0[0x1D1] = 0;                 block_0[0x1D7] = 0x16;
                block_0[0x234] = 0;                 block_0[0x23A] = 0x16;
                block_0[0x285] = 0;                 block_0[0x28B] = 0x16;
                block_0[0x2E2] = 0;                 block_0[0x2E8] = 0x16;
                block_0[0x336] = 0;                 block_0[0x33C] = 0x16;
                block_0[0x3AB] = 0;                 block_0[0x3B1] = 0x16;
                block_0[0x3F3] = 0;                 block_0[0x3F9] = 0x16;

                /*secure area (7k) from 0x14700, move to 0x3000 (len = 1100h)*/
                block_0[0x4C8] = 0;                 block_0[0x4CB] = 0x30;

                /*main data area (8k) from 0x30000, move to 0x5000 (len = 7600h)*/
                block_0[0x627] = 0x30;              block_0[0x628] = 0xC0;
                block_0[0x62A] = 0;                 block_0[0x62B] = 0xE1;
                
                /*Below enables us to finally have ntrboot on header 7.0x so we don't need*/
                /*to borrow header 5.06. Not sure what the original instruction is for*/
                /*(loop until the 7th bit of InputPort 0x17 is 1) on rev 7.0x but we are changing it to*/
                /*how the cart does it with a 5.06 header (loop until the 8th bit of InputPort 0x11 is 1)*/
                block_0[0x5A] = 0x11;
                block_0[0x5D] = 0x80;

                break;
            }
            default:
                logMessage(LOG_ERR, "r4isdhc.hk: 0x%08x is not a recognized version and therefore is not supported.", sw_rev);
                return false;
        }

        readFlash(0x11100, 0x200, gameHeader);
        memcpy(block_0 + 0x1000, gameHeader, 0x200);
        memcpy(block_0 + 0x1600, blowfish_key, 0x1048);
        memcpy(block_0 + 0x3EA8, firm, 0x200);
        memcpy(block_0 + 0x5000, firm + 0x200, firm_size - 0x200);
        encrypt_memcpy(block_0 + 0x1200, block_0 + 0x1200, 0xEE00);
        injectFlash(0, 0x10000, 0, block_0, 0x10000, false);
        
        free(block_0);
        return true;
    }
};

const uint8_t R4iSDHCHK::cmdGetCartUniqueKey[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00};     //reads flash at offset 0x2FE00
const uint8_t R4iSDHCHK::cmdUnkD0AA[8] = {0xD0, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4iSDHCHK::cmdGetChipID[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};            //returns as 0xFC2

const uint8_t R4iSDHCHK::cmdGetSWRev[8] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4iSDHCHK::cmdReadFlash700[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00};
const uint8_t R4iSDHCHK::cmdReadFlash506[8] = {0xB7, 0x01, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4iSDHCHK::cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t R4iSDHCHK::cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
const uint8_t R4iSDHCHK::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint32_t R4iSDHCHK::sw_rev = 0;

R4iSDHCHK r4isdhchk;

}
