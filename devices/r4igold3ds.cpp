#include "device.h"
#include "delay.h"

#include <cstring>
#include <algorithm>

#define BIT(n) (1 << (n))

class R4i_Gold_3DS : Flashcart {
private:
    static uint8_t encrypt(uint8_t dec)
    {
        uint8_t enc = 0;
        if (dec & BIT(0)) enc |= BIT(4);
        if (dec & BIT(1)) enc |= BIT(3);
        if (dec & BIT(2)) enc |= BIT(7);
        if (dec & BIT(3)) enc |= BIT(6);
        if (dec & BIT(4)) enc |= BIT(1);
        if (dec & BIT(5)) enc |= BIT(0);
        if (dec & BIT(6)) enc |= BIT(2);
        if (dec & BIT(7)) enc |= BIT(5);
        return enc;
    }

    static uint8_t decrypt(uint8_t enc)
    {
         uint8_t dec = 0;
         if (enc & BIT(0)) dec |= BIT(5);
         if (enc & BIT(1)) dec |= BIT(4);
         if (enc & BIT(2)) dec |= BIT(6);
         if (enc & BIT(3)) dec |= BIT(1);
         if (enc & BIT(4)) dec |= BIT(0);
         if (enc & BIT(5)) dec |= BIT(7);
         if (enc & BIT(6)) dec |= BIT(3);
         if (enc & BIT(6)) dec |= BIT(2);
         return dec;
    }

    static void encrypt_memcpy(uint8_t *dst, uint8_t *src, uint32_t length)
    {
        for(int i = 0; i < (int)length; ++i)
            dst[i] = encrypt(src[i]);
    }

    void r4i_read(uint8_t *outbuf, uint32_t address) {
        uint8_t cmdbuf[8];
        memcpy(cmdbuf, cmdReadFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0x200, outbuf); // TODO: Add actual flags.
        r4i_wait_flash_busy();
    }

    uint32_t r4i_erase(uint32_t address)
    {
        uint32_t status;
        uint8_t cmdbuf[8];
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 4, (uint8_t*)&status); // TODO: Add actual flags.
        r4i_wait_flash_busy();
    }

    void r4i_writebyte(uint32_t address, uint8_t value)
    {
        uint32_t status;
        uint8_t cmdbuf[8];
        memcpy(cmdbuf, cmdWriteByteFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;
        cmdbuf[4] = value;

        sendCommand(cmdbuf, 4, (uint8_t*)&status); // TODO: Add actual flags.
        r4i_wait_flash_busy();
    }

    void r4i_wait_flash_busy() {
        uint32_t state;
        do {
            ioDelay( 16 * 10 );
            sendCommand(cmdWaitFlashBusy, 4, (uint8_t *)&state);
        } while ((state & 1) != 0);
    }

protected:
    uint32_t m_hwrevision;

    static const uint8_t cmdGetHWRevision[8];
    static const uint8_t cmdReadFlash[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdWaitFlashBusy[8];
public:
    R4i_Gold_3DS() : Flashcart("R4i Gold 3DS", 0x400000) { }

    const char *getAuthor() { return "kitling"; }
    const char *getDescription() { return "Works with many R4i Gold 3DS variants:\n * R4i Gold 3DS (RTS, rev A5/A6/A7) (r4ids.cn)\n * R4i Gold 3DS Starter (r4ids.cn)\n * R4 3D Revolution (r4idsn.com)\n * Infinity 3 R4i (r4infinity.com)"; }

    bool initialize()
    {
        sendCommand(cmdGetHWRevision, 4, (uint8_t*)&m_hwrevision);
        if (m_hwrevision != 0xA7A7A7A7 && m_hwrevision != 0xA6A6A6A6 && m_hwrevision != 0xA5A5A5A5) return false;

        return true;
    }
    
    void shutdown() { }
    
    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer)
    {
        for (uint32_t curpos=0; curpos < length; curpos+=0x200) {
            r4i_read(buffer + curpos, address + curpos);
            showProgress(curpos,length, "Reading");
        }

        return true;
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer)
    {
        for (uint32_t addr=0; addr < length; addr+=0x10000)
            r4i_erase(address + addr);

        for (uint32_t i=0; i < length; i++) {
            r4i_writebyte(address + i, buffer[i]);
            showProgress(i,length, "Writing");
        }

        return true;
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
    {
        const uint32_t blowfish_adr = 0x0;
        const uint32_t firm_hdr_adr = 0xEE00;
        const uint32_t firm_adr = 0x80000;

        uint8_t *chunk0 = (uint8_t *)malloc(0x10000);
        readFlash(blowfish_adr, 0x10000, chunk0);
        encrypt_memcpy(chunk0, blowfish_key, 0x1048);
        encrypt_memcpy(chunk0 + firm_hdr_adr, firm, 0x200);
        writeFlash(blowfish_adr, 0x10000, chunk0);
        free(chunk0);

        uint32_t buf_size = PAGE_ROUND_UP(firm_size - 0x200, 0x10000);
        uint8_t *firm_chunk = (uint8_t *)malloc(buf_size);
        readFlash(firm_adr, buf_size, firm_chunk);
        encrypt_memcpy(firm_chunk, firm + 0x200, firm_size);
        writeFlash(firm_adr, buf_size, firm_chunk);

        free(firm_chunk);

        return true;
    }
};

const uint8_t R4i_Gold_3DS::cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdReadFlash[8] = {0xA5, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdEraseFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdWriteByteFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

R4i_Gold_3DS r4i_gold_3ds;
