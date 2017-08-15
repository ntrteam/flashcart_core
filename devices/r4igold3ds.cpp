#include <cstring>
#include <algorithm>
using std::memcpy;

#include "../device.h"

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

        // Included for completion's sake.
        // static uint8_t decrypt(uint8_t enc)
        // {
        //     uint8_t dec = 0;
        //     if (enc & BIT(0)) dec |= BIT(5);
        //     if (enc & BIT(1)) dec |= BIT(4);
        //     if (enc & BIT(2)) dec |= BIT(6);
        //     if (enc & BIT(3)) dec |= BIT(1);
        //     if (enc & BIT(4)) dec |= BIT(0);
        //     if (enc & BIT(5)) dec |= BIT(7);
        //     if (enc & BIT(6)) dec |= BIT(3);
        //     if (enc & BIT(6)) dec |= BIT(2);
        //     return dec;
        // }

        static void encrypt_memcpy(uint8_t *dst, uint8_t *src, uint32_t length)
        {
            for(int i=0; i<length; i++)
            {
                dst[i] = encrypt(src[i]);
                //dst[i] = (src[i]);
            }
        }

    protected:
        static const uint8_t cmdReadFlash[8];
        static const uint8_t cmdEraseFlash[8];
        static const uint8_t cmdWriteByteFlash[8];

        virtual size_t formatReadCommand(uint8_t *cmdbuf, uint32_t address) {
            memcpy(cmdbuf, cmdReadFlash, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            return 0x200; // Data
        }

        virtual size_t formatEraseCommand(uint8_t *cmdbuf, uint32_t address) {
            memcpy(cmdbuf, cmdEraseFlash, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            return 4; // Status
        }

        virtual size_t formatWriteCommand(uint8_t *cmdbuf, uint32_t address, uint8_t value) {
            memcpy(cmdbuf, cmdWriteByteFlash, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            cmdbuf[4] = value;
            return 4; // Status
        }

    public:
        R4i_Gold_3DS() : Flashcart() {
            description = "R4i Gold 3DS";
            max_length = 0x400000;
        }

        virtual bool setup() {
            reset(); // Known good state.

            uint32_t hw_revision;
            sendCommand(ak2i_cmdGetHWRevision, 4, (uint8_t*)&hw_revision); // Get HW Revision
            if (hw_revision != 0xA7A7A7A7) return false;

            // Doesn't use any locking or unlocking functions?
            return true;
        }

        virtual void cleanup() {} // Nothing needed?

        virtual void writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
        {
            // This function follows a read-modify-write cycle:
            //  - Read from flash to prevent accidental erasure of things not overwritten
            //  - Modify the data read, mostly by memcpying data in, perhaps 'encrypting' it first.
            //  - Write the data back to flash, now that we have made our modifications.
            const uint32_t blowfish_adr = 0x0;
            const uint32_t firm_hdr_adr = 0xEE00;
            const uint32_t firm_adr = 0x80000;
            // const uint32_t firm_adr2 = 0x370000;
            // const uint32_t firm_adr3 = 0x368000;

            uint8_t *chunk0 = (uint8_t *)malloc(page_size);
            readFlash(blowfish_adr, page_size, chunk0);
            encrypt_memcpy(chunk0, blowfish_key, 0x1048);
            encrypt_memcpy(chunk0 + firm_hdr_adr, firm, 0x200);
            writeFlash(blowfish_adr, page_size, chunk0);
            free(chunk0);

            uint32_t buf_size = PAGE_ROUND_UP(firm_size - 0x200, page_size);
            uint8_t *firm_chunk = (uint8_t *)malloc(buf_size);
            readFlash(firm_adr, buf_size, firm_chunk);
            encrypt_memcpy(firm_chunk, firm + 0x200, firm_size);
            writeFlash(firm_adr, buf_size, firm_chunk);
            // writeFlash(firm_adr2, buf_size, firm_chunk);

            free(firm_chunk);
        }
};

const uint8_t R4i_Gold_3DS::cmdReadFlash[8] = {0xA5, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdEraseFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdWriteByteFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};

R4i_Gold_3DS r4igold3ds;
