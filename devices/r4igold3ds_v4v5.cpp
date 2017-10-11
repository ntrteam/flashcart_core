#include "device.h"

#include <cstring>
#include <algorithm>

#define BIT(n) (1 << (n))

namespace flashcart_core {
using ntrcard::sendCommand;
using platform::logMessage;
using platform::showProgress;

class R4i_Gold_3DS_v4v5 : Flashcart {
private:
    static uint8_t encrypt(uint8_t dec, uint32_t offset)
    {
        uint8_t xorpad_byte;
		xorpad_byte = (offset % 256) + 9;
		return xorpad_byte ^ dec;
    }

	/* not needed - xorpad crypto is symmetrical
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
	*/

    static void encrypt_memcpy(uint8_t *dst, uint8_t *src, uint32_t length)
    {
        for(int i = 0; i < (int)length; ++i)
            dst[i] = encrypt(src[i], i);
    }

    void r4i_read(uint8_t *outbuf, uint32_t address) {
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4iGold: read(0x%08x)", address);
        memcpy(cmdbuf, cmdReadFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 0x200, outbuf, 32);
        r4i_wait_flash_busy();
    }

    void r4i_erase(uint32_t address)
    {
        uint32_t status;
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4iGold: erase(0x%08x)", address);
        memcpy(cmdbuf, cmdEraseFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;

        sendCommand(cmdbuf, 4, (uint8_t*)&status, 32);
        r4i_wait_flash_busy();
    }

    void r4i_writebyte(uint32_t address, uint8_t value)
    {
        uint32_t status;
        uint8_t cmdbuf[8];
        logMessage(LOG_DEBUG, "R4iGold: write(0x%08x) = 0x%02x", address, value);
        memcpy(cmdbuf, cmdWriteByteFlash, 8);
        cmdbuf[1] = (address >> 16) & 0xFF;
        cmdbuf[2] = (address >>  8) & 0xFF;
        cmdbuf[3] = (address >>  0) & 0xFF;
        cmdbuf[4] = value;

        sendCommand(cmdbuf, 4, (uint8_t*)&status, 32);
        r4i_wait_flash_busy();
    }

    void r4i_wait_flash_busy() {
        uint32_t state;
        do {
            sendCommand(cmdWaitFlashBusy, 4, (uint8_t *)&state, 32);
            logMessage(LOG_DEBUG, "R4iGold: waitFlashBusy = 0x%08x", state);
        } while ((state & 1) != 0);
    }

protected:
    static const uint8_t cmdGetHWRevision[8];
    static const uint8_t cmdReadFlash[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdWaitFlashBusy[8];
public:
    R4i_Gold_3DS_v4v5() : Flashcart("R4i Gold 3DS v4v5", 0x200000) { }

    const char *getAuthor() { return "Kitlith"; }
    const char *getDescription() { return "Works with many R4i Gold 3DS variants:\n * R4i Gold 3DS (rev4/rev5 (r4ids.cn)\n * R4i Gold 3DS Starter (r4ids.cn)\n * R4 3D Revolution (r4idsn.com)\n * Infinity 3 R4i (r4infinity.com)"; }

    bool initialize()
    {
        logMessage(LOG_INFO, "R4iGold: Init");
        uint32_t hw_revision;
        sendCommand(cmdGetHWRevision, 4, (uint8_t*)&hw_revision, 0);
        logMessage(LOG_NOTICE, "R4iGold: HW Revision = %08x", hw_revision);
        if (hw_revision == 0) return true;

        return false;
    }

    void shutdown() {
        logMessage(LOG_INFO, "R4iGold: Shutdown");
    }

    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer)
    {
        logMessage(LOG_INFO, "R4iGold: readFlash(addr=0x%08x, size=0x%x)", address, length);
        for (uint32_t curpos=0; curpos < length; curpos+=0x200) {
            r4i_read(buffer + curpos, address + curpos);
            showProgress(curpos+1,length, "Reading");
        }

        return true;
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer)
    {
        logMessage(LOG_INFO, "R4iGold: writeFlash(addr=0x%08x, size=0x%x)", address, length);
        for (uint32_t addr=0; addr < length; addr+=0x10000)
            r4i_erase(address + addr);

        for (uint32_t i=0; i < length; i++) {
            r4i_writebyte(address + i, buffer[i]);
            showProgress(i+1,length, "Writing");
        }

        return true;
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
    {
        const uint32_t blowfish_adr = 0x0;
	    const uint32_t firm_chunk_adr = 0x80000;
        const uint32_t firm_adr = 0x2200;     //overall bootloader addr 0x82200* (*writing directly to this addr was destroying bootloader data)
		const uint32_t firm_hdr_chunk_adr = 0x1F0000;
        const uint32_t firm_hdr_adr = 0xFE00; //this is overall bootloader address 0x1FFE00
		
        logMessage(LOG_INFO, "R4iGold: Injecting ntrboot");
        uint8_t *chunk0 = (uint8_t *)malloc(0x10000);
		
        readFlash(blowfish_adr, 0x10000, chunk0);
        memcpy(chunk0, blowfish_key, 0x1048);
        writeFlash(blowfish_adr, 0x10000, chunk0);
		
		//readFlash(firm_chunk_adr, 0x10000, chunk0);
		//encrypt_memcpy(chunk0 + firm_adr, firm + 0x200, firm_size);
        //writeFlash(firm_chunk_adr, 0x10000, chunk0);
		
	    readFlash(firm_hdr_chunk_adr, 0x10000, chunk0);
		memcpy(chunk0 + firm_hdr_adr, firm, 0x200);
        writeFlash(firm_hdr_chunk_adr, 0x10000, chunk0);
		
        free(chunk0);
        
        uint32_t buf_size = PAGE_ROUND_UP(firm_size - 0x200 + firm_adr, 0x10000);
        uint8_t *firm_chunk = (uint8_t *)malloc(buf_size);
        readFlash(firm_chunk_adr, buf_size, firm_chunk);
        encrypt_memcpy(firm_chunk + firm_adr, firm + 0x200, firm_size);
        writeFlash(firm_chunk_adr, buf_size, firm_chunk);

        free(firm_chunk);
		

        return true;
    }
};

const uint8_t R4i_Gold_3DS_v4v5::cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4i_Gold_3DS_v4v5::cmdReadFlash[8] = {0xA5, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS_v4v5::cmdEraseFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x00, 0x00};
const uint8_t R4i_Gold_3DS_v4v5::cmdWriteByteFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS_v4v5::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

R4i_Gold_3DS_v4v5 r4i_gold_3ds_v4v5;
}
