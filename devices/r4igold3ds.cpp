#include "../device.h"

#include <cstring>
#include <algorithm>

#define BIT(n) (1 << (n))

namespace flashcart_core {
using platform::logMessage;
using platform::showProgress;

struct r4i_flash_setting {
    uint32_t blowfish_chunk_adr;
    uint32_t blowfish_offset;
    uint32_t firm_hdr_chunk_adr;
    uint32_t firm_hdr_offset;
    uint32_t firm_chunk_adr;
    uint32_t firm_offset;
    bool encrypt_header;
};

class R4i_Gold_3DS : Flashcart {
private:
    uint8_t encrypt(uint8_t dec, uint32_t offset)
    {
        uint8_t enc = 0;
        switch (m_r4i_type) {
            case 1: //rev9-D
            case 3: //rev6-7 maybe 8
                if (dec & BIT(0)) enc |= BIT(4);
                if (dec & BIT(1)) enc |= BIT(3);
                if (dec & BIT(2)) enc |= BIT(7);
                if (dec & BIT(3)) enc |= BIT(6);
                if (dec & BIT(4)) enc |= BIT(1);
                if (dec & BIT(5)) enc |= BIT(0);
                if (dec & BIT(6)) enc |= BIT(2);
                if (dec & BIT(7)) enc |= BIT(5);
                return enc;
            case 2: //rev4-5
                enc = (offset % 256) + 9;
                return enc ^ dec;
        }
        // FIXME throw error
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

    void encrypt_memcpy(uint8_t *dst, uint8_t *src, uint32_t length)
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

        m_card->sendCommand(cmdbuf, outbuf, 0x200, 32);
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

        m_card->sendCommand(cmdbuf, &status, 4, 32);
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

        m_card->sendCommand(cmdbuf, &status, 4, 32);
        r4i_wait_flash_busy();
    }

    void r4i_wait_flash_busy() {
        uint32_t state;
        do {
            m_card->sendCommand(cmdWaitFlashBusy, &state, 4, 32);
            logMessage(LOG_DEBUG, "R4iGold: waitFlashBusy = 0x%08x", state);
        } while ((state & 1) != 0);
    }

    void injectFlash(uint32_t chunk_addr, uint32_t chunk_length, uint32_t offset, uint8_t *src, uint32_t src_length, bool encrypt) {
        uint8_t *chunk = (uint8_t *)malloc(chunk_length);
        readFlash(chunk_addr, chunk_length, chunk);
        if (encrypt) {
            encrypt_memcpy(chunk + offset, src, src_length);
        } else {
            memcpy(chunk + offset, src, src_length);
        }
        writeFlash(chunk_addr, chunk_length, chunk);
        free(chunk);
    }

protected:
    static const uint8_t cmdGetHWRevision[8];
    static const uint8_t cmdReadFlash[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdWaitFlashBusy[8];
    static const uint8_t cmdCardType[8];
    static const r4i_flash_setting flashSettings[3];

    uint8_t m_r4i_type;

public:
    R4i_Gold_3DS() : Flashcart("R4i Gold 3DS", 0x400000) { }

    const char *getAuthor() { return "Kitlith"; }
    const char *getDescription() {
        return "Works with many R4i Gold 3DS variants:\n"
               " * R4i Gold 3DS (RTS, rev A5/A6/A7) (r4ids.cn)\n"
               " * R4i Gold 3DS (rev 4/5/6/7/8?) (r4ids.cn)\n"
               " * R4i Gold 3DS Starter (r4ids.cn)\n"
               " * R4 3D Revolution (r4idsn.com)\n"
               " * Infinity 3 R4i (r4infinity.com)";
    }

    size_t getMaxLength()
    {
        switch (m_r4i_type) {
            case 1:
                return 0x400000;
            case 2:
            case 3:
                return 0x200000;
        }
        return 0x0;
    }

    bool initialize()
    {
        logMessage(LOG_INFO, "R4iGold: Init");
        uint32_t hw_revision;
        uint32_t hw_type;
        m_card->sendCommand(cmdGetHWRevision, (uint8_t*)&hw_revision, 4, 0);
        m_card->sendCommand(cmdCardType, (uint8_t*)&hw_type, 4, 0);
        logMessage(LOG_NOTICE, "R4iGold: HW Revision = %08x", hw_revision);
        logMessage(LOG_NOTICE, "R4iGold: HW Type = %08x", hw_type);

        switch (hw_revision) {
            // rev9-D
            case 0xA5A5A5A5:
            case 0xA6A6A6A6:
            case 0xA7A7A7A7:
                m_r4i_type = 1;
                return true;
            case 0:
                break;
            default:
                return false;
        }
        switch (hw_type) {
            // rev4-5
            case 0xA79BCA95:
            case 0x95A79BCA:
            case 0xCA95A79B:
            case 0x9BCA95A7:
                m_r4i_type = 2;
                return true;
            // rev6-7 maybe 8
            case 0xB7DB5BB5:
            case 0xDB5BB5B7:
            case 0x5BB5B7DB:
            case 0xB5B7DB5B:
                m_r4i_type = 3;
                return true;
        }
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

        const r4i_flash_setting *set;

        switch (m_r4i_type) {
            case 1:
            case 2:
            case 3:
                set = &flashSettings[m_r4i_type - 1];
                break;
            default:
                return false;
        }

        logMessage(LOG_INFO, "R4iGold: Injecting ntrboot");
        injectFlash(set->blowfish_chunk_adr, 0x10000, set->blowfish_offset, blowfish_key, 0x1048, set->encrypt_header);
        injectFlash(set->firm_hdr_chunk_adr, 0x10000, set->firm_hdr_offset, firm, 0x200, set->encrypt_header);

        uint32_t buf_size = PAGE_ROUND_UP(firm_size - 0x200 + set->firm_offset, 0x10000);
        injectFlash(set->firm_chunk_adr, buf_size, set->firm_offset, firm + 0x200, firm_size, true);
        return true;
    }
};

const uint8_t R4i_Gold_3DS::cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdReadFlash[8] = {0xA5, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdEraseFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdWriteByteFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdCardType[8] = {0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const r4i_flash_setting R4i_Gold_3DS::flashSettings[3] = {
    {
        /* .blowfish_chunk_adr = */ 0x000000,
        /* .blowfish_offset    = */ 0x000000,
        /* .firm_hdr_chunk_adr = */ 0x000000,
        /* .firm_hdr_offset    = */ 0x00EE00,
        /* .firm_chunk_adr     = */ 0x080000,
        /* .firm_offset        = */ 0x000000,
        /* .encrypt_header     = */ true
    },
    {
        /* .blowfish_chunk_adr = */ 0x000000,
        /* .blowfish_offset    = */ 0x000000,
        //this is overall bootloader address 0x1FFE00
        /* .firm_hdr_chunk_adr = */ 0x1F0000,
        /* .firm_hdr_offset    = */ 0x00FE00,
        // overall bootloader addr 0x82200*
        // (*writing directly to this addr was destroying bootloader data)
        /* .firm_chunk_adr     = */ 0x080000,
        /* .firm_offset        = */ 0x002200,
        /* .encrypt_header     = */ false,
    },
    {
        /* .blowfish_chunk_adr = */ 0x1F0000,
        /* .blowfish_offset    = */ 0x00A000,
        /* .firm_hdr_chunk_adr = */ 0x1F0000,
        /* .firm_hdr_offset    = */ 0x00FE00,
        /* .firm_chunk_adr     = */ 0x080000,
        /* .firm_offset        = */ 0x000000,
        /* .encrypt_header     = */ true,
    },
};

R4i_Gold_3DS r4i_gold_3ds;
}
