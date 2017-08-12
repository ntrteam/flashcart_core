#include <cstring>
#include <algorithm>

#include "../device.h"

class Genuine_ak2i_44 : protected Flashcart {
    protected:
        static const uint8_t ak2i_cmdSetMapTableAddress[8];
        static const uint8_t ak2i_cmdActiveFatMap[8];
        static const uint8_t ak2i_cmdUnlockFlash[8];
        static const uint8_t ak2i_cmdLockFlash[8];
        static const uint8_t ak2i_cmdUnlockASIC[8]; 
        static const uint8_t ak2i_cmdWaitFlashBusy[8];

        // 0xA5..0x5A on R4i Gold 3DS
        static const uint8_t ak2i_cmdReadFlash[8];
        // 0xDA..0xA5 on R4i Gold 3DS
        static const uint8_t ak2i_cmdEraseFlash[8];

        // 0xDA..0x5A on R4i Gold 3DS
        static const uint8_t ak2i_cmdWriteByteFlash[8];

        Flashcart_mode curmode;
        virtual void switchMode(Flashcart_mode mode) {
            if (curmode == mode) return;

            switch (mode) {
                case FLASH_READ:
                    sendCommand(ak2i_cmdLockFlash, 0, nullptr);
                    break;
                case FLASH_WRITE:
                    sendCommand(ak2i_cmdUnlockFlash, 0, nullptr);
                    sendCommand(ak2i_cmdUnlockASIC, 0, nullptr); 
                    break;
            }
            
            sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr);

            curmode = mode;
        }

        virtual size_t formatReadCommand(uint8_t *cmdbuf, uint32_t address) {
            memcpy(cmdbuf, ak2i_cmdReadFlash, 8);
            cmdbuf[1] = (address >> 24) & 0xFF;
            cmdbuf[2] = (address >> 16) & 0xFF;
            cmdbuf[3] = (address >>  8) & 0xFF;
            cmdbuf[4] = (address >>  0) & 0xFF;
            return 0x200; // Data
        }

        virtual size_t formatEraseCommand(uint8_t *cmdbuf, uint32_t address) {
            memcpy(cmdbuf, ak2i_cmdEraseFlash, 8);
            cmdbuf[1] = (address >> 16) & 0x1F;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            return 0; // No status.
        }

        virtual size_t formatWriteCommand(uint8_t *cmdbuf, uint32_t address, uint8_t value) {
            memcpy(cmdbuf, ak2i_cmdWriteByteFlash, 8);
            cmdbuf[1] = (address >> 16) & 0x1F;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            cmdbuf[4] = value;
            return 0; // No status.
        }

    public:
        Genuine_ak2i_44() : Flashcart() {
            description = "Acekard 2i HW-44";
            max_length = 0x200000;
        }

        virtual bool setup() {
            uint32_t hw_revision;
            uint32_t garbage;

            reset(); // Known good state.

            sendCommand(ak2i_cmdGetHWRevision, 4, (uint8_t*)&hw_revision); // Get HW Revision
            if (hw_revision != 0x44444444) return false;

            sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr); // TODO: Not all of this is needed when reading, is it?
            sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t*)&garbage);
            sendCommand(ak2i_cmdUnlockASIC, 0, nullptr); 

            curmode = FLASH_READ;
            return true;
        }

        virtual void cleanup() {
            uint32_t garbage;
            switchMode(FLASH_READ); // Lock flash.
            sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t *)&garbage);
        }

        virtual void writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
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
            // Forget R/M/W for now. Read is broken on hw44. Let's just make sure that what we're flashing is sane.
            memcpy(buf, blowfish_key, 0x1048);
            memcpy(buf + firm_offset, firm, firm_size);

            uint8_t chipid_and_length[8] = {0x00, 0x00, 0x0F, 0xC2, 0x00, 0xB4, 0x17, 0x00};
            memcpy(buf + chipid_offset, chipid_and_length, 8);

            writeFlash(blowfish_adr, buf_size, buf);

            free(buf);
        }
};

const uint8_t Genuine_ak2i_44::ak2i_cmdSetMapTableAddress[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdActiveFatMap[8] = {0xC2, 0x55, 0xAA, 0x55, 0xAA, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdUnlockFlash[8] = {0xC2, 0xAA, 0x55, 0xAA, 0x55, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdLockFlash[8] = {0xC2, 0xAA, 0xAA, 0x55, 0x55, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdUnlockASIC[8] = {0xC2, 0xAA, 0x55, 0x55, 0xAA, 0x00, 0x00, 0x00}; 
const uint8_t Genuine_ak2i_44::ak2i_cmdReadFlash[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};

class Genuine_ak2i_81 : protected Genuine_ak2i_44 {
    protected:
        static const uint8_t ak2i_cmdSetFlash1681_81[8];
        static const uint8_t ak2i_cmdEraseFlash81[8];
        static const uint8_t ak2i_cmdWriteByteFlash81[8];

        virtual size_t formatEraseCommand(uint8_t *cmdbuf, uint32_t address) {
            memcpy(cmdbuf, ak2i_cmdEraseFlash81, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            return 0; // No status.
        }

        virtual size_t formatWriteCommand(uint8_t *cmdbuf, uint32_t address, uint8_t value) {
            memcpy(cmdbuf, ak2i_cmdWriteByteFlash81, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            cmdbuf[4] = value;
            return 0; // No status.
        }
        
        virtual void switchMode(Flashcart_mode mode) {
            if (curmode == mode) return;

            switch (mode) {
                case FLASH_READ:
                    sendCommand(ak2i_cmdLockFlash, 0, nullptr);
                    break;
                case FLASH_WRITE:
                    sendCommand(ak2i_cmdUnlockFlash, 0, nullptr);
                    sendCommand(ak2i_cmdUnlockASIC, 0, nullptr); 
                    break;
            }
            
            sendCommand(ak2i_cmdSetFlash1681_81, 0, nullptr);
            sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr);

            curmode = mode;
        }

    public:
        Genuine_ak2i_81() : Genuine_ak2i_44() {
            description = "Acekard 2i HW-81";
            max_length = 0x1000000;
        }

        virtual bool setup() {
            uint32_t hw_revision;
            uint32_t garbage;

            reset(); // Known good state.

            sendCommand(ak2i_cmdGetHWRevision, 4, (uint8_t*)&hw_revision); // Get HW Revision
            if (hw_revision != 0x81818181) return false;

            sendCommand(ak2i_cmdSetFlash1681_81, 0, nullptr);
            sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t*)&garbage);
            sendCommand(ak2i_cmdUnlockFlash, 0, nullptr);
            sendCommand(ak2i_cmdUnlockASIC, 0, nullptr); 
            sendCommand(ak2i_cmdSetMapTableAddress, 0, nullptr);

            curmode = FLASH_READ;
            return true;
        }
};

const uint8_t Genuine_ak2i_81::ak2i_cmdSetFlash1681_81[8] = {0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x06};
const uint8_t Genuine_ak2i_81::ak2i_cmdEraseFlash81[8] = {0xD4, 0x00, 0x00, 0x00, 0x30, 0x80, 0x00, 0x35};
const uint8_t Genuine_ak2i_81::ak2i_cmdWriteByteFlash81[8] = {0xD4, 0x00, 0x00, 0x00, 0x30, 0xa0, 0x00, 0x63};

Genuine_ak2i_44 ak2i_44; // These are automatically added to the list of types to check.
Genuine_ak2i_81 ak2i_88;
