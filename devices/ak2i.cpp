#include "device.h"
#include "delay.h"

#include <stdlib.h>
#include <cstring>

class AK2i : Flashcart {
protected:
    static const uint8_t ak2i_cmdWaitFlashBusy[8];
    static const uint8_t ak2i_cmdGetHWRevision[8];
    static const uint8_t ak2i_cmdSetMapTableAddress[8];
    static const uint8_t ak2i_cmdActiveFatMap[8];
    static const uint8_t ak2i_cmdUnlockFlash[8];
    static const uint8_t ak2i_cmdLockFlash[8];
    static const uint8_t ak2i_cmdUnlockASIC[8];
    static const uint8_t ak2i_cmdReadFlash[8];
    static const uint8_t ak2i_cmdEraseFlash[8];
    static const uint8_t ak2i_cmdWriteByteFlash[8];
    static const uint8_t ak2i_cmdSetFlash1681_81[8];
    static const uint8_t ak2i_cmdEraseFlash81[8];
    static const uint8_t ak2i_cmdWriteByteFlash81[8];

    static const uint32_t page_size = 0x10000;

    uint32_t m_ak2i_hwrevision;

    void a2ki_wait_flash_busy() {
        uint32_t state;
        do {
            ioDelay( 16 * 10 );
            sendCommand(ak2i_cmdWaitFlashBusy, 4, (uint8_t *)&state, 4);
        } while ((state & 1) != 0);
    }

    void a2ki_read(uint8_t *outbuf, uint32_t address) {
        uint8_t cmdbuf[8];
        memcpy(cmdbuf, ak2i_cmdReadFlash, 8);
        cmdbuf[1] = (address >> 24) & 0xFF;
        cmdbuf[2] = (address >> 16) & 0xFF;
        cmdbuf[3] = (address >>  8) & 0xFF;
        cmdbuf[4] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0x200, outbuf, 2);
        a2ki_wait_flash_busy();
    }

    void a2ki_erase(uint32_t address) {
        uint8_t cmdbuf[8];

        if (m_ak2i_hwrevision == 0x44444444)
        {
            memcpy(cmdbuf, ak2i_cmdEraseFlash, 8);
            cmdbuf[1] = (address >> 16) & 0x1F;
        }
        else if (m_ak2i_hwrevision == 0x81818181)
        {
            memcpy(cmdbuf, ak2i_cmdEraseFlash81, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
        }

        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0, nullptr, (m_ak2i_hwrevision == 0x81818181) ? 20 : 0 );
        a2ki_wait_flash_busy();
    }

    void a2ki_writebyte(uint32_t address, uint8_t value) {
        uint8_t cmdbuf[8];

        if (m_ak2i_hwrevision == 0x44444444)
        {
            memcpy(cmdbuf, ak2i_cmdWriteByteFlash, 8);
            cmdbuf[1] = (address >> 16) & 0x1F;
        }
        else if (m_ak2i_hwrevision == 0x81818181)
        {
            memcpy(cmdbuf, ak2i_cmdWriteByteFlash81, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
        }

        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;
        cmdbuf[4] = value;

        sendCommand(cmdbuf, 0, nullptr, 20);
        a2ki_wait_flash_busy();
    }

public:
    AK2i() : Flashcart("Acekard 2i", 0x200000) { }

    const char *getAuthor() { return "kitling + normmatt"; }
    const char *getDescription() { return "Works with the following carts:\n * Acekard 2i HW-44\n * Acekard 2i HW-81\n * R4i Ultra (r4ultra.com)"; }

    const size_t getMaxLength()
    {
        if (m_ak2i_hwrevision == 0x44444444) return 0x200000;
        if (m_ak2i_hwrevision == 0x81818181) return 0x1000000;
        return 0x0;
    }

    bool initialize()
    {
        sendCommand(ak2i_cmdGetHWRevision, 4, (uint8_t*)&m_ak2i_hwrevision, 0);
        if (m_ak2i_hwrevision != 0x44444444 && m_ak2i_hwrevision != 0x81818181) return false;

        uint32_t garbage;
        if (m_ak2i_hwrevision == 0x44444444)
        {
            sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr, 0);
            sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t*)&garbage, 0);
            sendCommand(ak2i_cmdUnlockASIC, 0, nullptr, 0);
        }
        else if (m_ak2i_hwrevision == 0x81818181)
        {
            sendCommand(ak2i_cmdSetFlash1681_81, 0, nullptr, 20);
            sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t*)&garbage, 0);
            sendCommand(ak2i_cmdUnlockFlash, 0, nullptr, 0);
            sendCommand(ak2i_cmdUnlockASIC, 0, nullptr, 0);
            sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr, 0);
        }

        return true;
    }

    void shutdown()
    {
        uint32_t garbage;
        sendCommand(ak2i_cmdLockFlash, 0, nullptr, 0);
        sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr, 0);
        sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t *)&garbage, 4);
    }

    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer)
    {
        sendCommand(ak2i_cmdLockFlash, 0, nullptr, 0);

        if (m_ak2i_hwrevision == 0x81818181) sendCommand(ak2i_cmdSetFlash1681_81, 0, nullptr, 20);
        sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr, 0);

        for (uint32_t curpos=0; curpos < length; curpos+=0x200) {
            a2ki_read(buffer + curpos, address + curpos);
            showProgress(curpos,length, "Reading");
        }

        return true;
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer)
    {
        sendCommand(ak2i_cmdUnlockFlash, 0, nullptr, 0);
        sendCommand(ak2i_cmdUnlockASIC, 0, nullptr, 0);

        if (m_ak2i_hwrevision == 0x81818181) sendCommand(ak2i_cmdSetFlash1681_81, 0, nullptr, 20);
        sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr, 0);

        for (uint32_t addr=0; addr < length; addr+=page_size)
        {
            a2ki_erase(address + addr);

            for (uint32_t i=0; i < page_size; i++) {
                a2ki_writebyte(address + addr + i, buffer[addr + i]);
                showProgress(addr+i,length, "Writing");
            }
        }

        return true;
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
    {
        // This function follows a read-modify-write cycle:
        //  - Read from flash to prevent accidental erasure of things not overwritten
        //  - Modify the data read, mostly by memcpying data in, perhaps 'encrypting' it first.
        //  - Write the data back to flash, now that we have made our modifications.
        const uint32_t blowfish_adr = 0x80000;
        const uint32_t firm_offset = 0x9E00;
        const uint32_t chipid_offset = 0x1FC0;

        uint32_t buf_size = PAGE_ROUND_UP(firm_offset + firm_size, page_size);
        uint8_t *buf = (uint8_t *)calloc(buf_size, sizeof(uint8_t));

        // readFlash(blowfish_adr, buf_size, buf); // Read in data that shouldn't be changed
        memcpy(buf, blowfish_key, 0x1048);
        memcpy(buf + firm_offset, firm, firm_size);

        uint8_t chipid_and_length[8] = {0x00, 0x00, 0x0F, 0xC2, 0x00, 0xB4, 0x17, 0x00};
        memcpy(buf + chipid_offset, chipid_and_length, 8);

        writeFlash(blowfish_adr, buf_size, buf);

        free(buf);
    }
};

const uint8_t AK2i::ak2i_cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdSetMapTableAddress[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdActiveFatMap[8] = {0xC2, 0x55, 0xAA, 0x55, 0xAA, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdUnlockFlash[8] = {0xC2, 0xAA, 0x55, 0xAA, 0x55, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdLockFlash[8] = {0xC2, 0xAA, 0xAA, 0x55, 0x55, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdUnlockASIC[8] = {0xC2, 0xAA, 0x55, 0x55, 0xAA, 0x00, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdReadFlash[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
const uint8_t AK2i::ak2i_cmdSetFlash1681_81[8] = {0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x06};
const uint8_t AK2i::ak2i_cmdEraseFlash81[8] = {0xD4, 0x00, 0x00, 0x00, 0x30, 0x80, 0x00, 0x35};
const uint8_t AK2i::ak2i_cmdWriteByteFlash81[8] = {0xD4, 0x00, 0x00, 0x00, 0x30, 0xa0, 0x00, 0x63};

AK2i ak2i;
