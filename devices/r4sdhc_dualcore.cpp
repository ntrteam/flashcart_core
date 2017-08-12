#include "../device.h"

#include <cstring>

class R4SDHC_DualCore : Flashcart {
    protected:
        static const uint8_t cmdReadFlash[8];
        static const uint8_t cmdEraseFlash[8];
        static const uint8_t cmdWriteByteFlash[8];

        static const uint8_t cmdUnkD0AA[8];
        static const uint8_t cmdUnkD0[8];

        virtual size_t formatReadCommand(uint8_t *cmdbuf, uint32_t address) {
            // TODO
            // memcpy(cmdbuf, cmdReadFlash, 8);
            // cmdbuf[1] = (address >> 16) & 0xFF;
            // cmdbuf[2] = (address >>  8) & 0xFF;
            // cmdbuf[3] = (address >>  0) & 0xFF;
            return 0x200; // Data
        }

        virtual size_t formatEraseCommand(uint8_t *cmdbuf, uint32_t address) {
            memcpy(cmdbuf, cmdEraseFlash, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            return 0; // No status.
        }

        virtual size_t formatWriteCommand(uint8_t *cmdbuf, uint32_t address, uint8_t value) {
            memcpy(cmdbuf, cmdWriteByteFlash, 8);
            cmdbuf[1] = (address >> 16) & 0xFF;
            cmdbuf[2] = (address >>  8) & 0xFF;
            cmdbuf[3] = (address >>  0) & 0xFF;
            cmdbuf[4] = value;
            return 0; // No status.
        }

        virtual unsigned getMaxLength() { return 0x400000; } // TODO: Replace.
        virtual const char* getDescription() { return "R4 SDHC Dual Core"; }

    public:
        virtual bool setup() {
            reset(); // Known good state.

            uint32_t dummy;
            // Shoot, the updater doesn't really have any real checks...
            // TODO: How do we check this? SWVer != 0/0xFF?

            sendCommand(cmdUnkD0AA, 4, (uint8_t *)&dummy);
            sendCommand(cmdUnkD0, 0, nullptr);
            sendCommand(cmdUnkD0AA, 4, (uint8_t *)&dummy);
            sendCommand(cmdUnkD0AA, 4, (uint8_t *)&dummy);
            // Doesn't use any locking or unlocking functions?
            return true;
        }

        virtual void cleanup() {
            // Nothing needed?
        };

        // TODO:
        virtual void writeBlowfishAndFirm(uint8_t *template_data, uint32_t template_size) = 0;
};

const uint8_t R4SDHC_DualCore::cmdReadFlash[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};

const uint8_t R4SDHC_DualCore::cmdUnkD0AA[8] = {0xD0, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4SDHC_DualCore::cmdUnkD0[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
