#include "device.h"

#include <stdlib.h>
#include <cstring>

namespace flashcart_core {
using ntrcard::BlowfishKey;
using ntrcard::sendCommand;
using platform::logMessage;
using platform::showProgress;
using platform::ioDelay;
using platform::startSPITransfer;
using platform::writeSPI;
using platform::readSPI;
using platform::waitSPIBusy;
using platform::endSPITransfer;

typedef struct {
    uint32_t hardware;
    uint32_t firmware;
} Ace3DSPlusVersion;

class Ace3DSPlus : Flashcart {
    private:
        uint32_t spi_read_jedec_id() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - JEDEC_ID");
            uint8_t opcode[4] = {0x9F, 0x00, 0x00, 0x00};
            uint32_t ret = 0;
            uint8_t data;
            ioDelay(500);
            startSPITransfer();
            ioDelay(400);
            for (int i = 0; i < 4; i++) {
                ioDelay(100);
                writeSPI(opcode[i]);
                data = readSPI();
                ret <<= 8;
                ret |= data;
            }
            ioDelay(100);
            endSPITransfer();
            logMessage(LOG_DEBUG, "Ace3DS+: eeprom id (0x%08x)", ret);
            // manufacture/mem type/capacity
            return ret & 0xFFFFFF;
        }

        void spi_powerdown() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - POWER_DOWN");
            ioDelay(500);
            startSPITransfer();
            ioDelay(100);
            writeSPI(0xB9);
            ioDelay(100);
            endSPITransfer();
            ioDelay(500);
        }

        void spi_write_enable() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - WRITE_ENABLE");
            ioDelay(500);
            startSPITransfer();
            ioDelay(100);
            writeSPI(0x06);
            ioDelay(100);
            endSPITransfer();
            ioDelay(500);
        }

        void spi_write_disable() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - WRITE_DISABLE");
            startSPITransfer();
            ioDelay(200);
            writeSPI(0x04);
            endSPITransfer();
        }

        void spi_block_0_erase() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - BLOCK_ERASE(64KB) (0x00000000)");
            uint8_t opcode[4] = {0xD8, 0x00, 0x00, 0x00};
            ioDelay(500);
            startSPITransfer();
            for (int i = 0; i < 4; i++) {
                ioDelay(100);
                writeSPI(opcode[i]);
            }
            ioDelay(100);
            endSPITransfer();
        }

        void spi_chip_erase() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - CHIP_ERASE");
            ioDelay(500);
            startSPITransfer();
            ioDelay(100);
            writeSPI(0xC7);
            ioDelay(100);
            endSPITransfer();
        }

        uint8_t spi_read_status_register1() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - READ_STATUS_REGISTER_1");
            uint8_t ret;
            ioDelay(2000);
            startSPITransfer();
            ioDelay(200);
            writeSPI(0x05);
            writeSPI(0x00);
            ret = readSPI();
            endSPITransfer();
            return ret;
        }

        uint8_t spi_() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - READ_STATUS_REGISTER_1");
            uint8_t ret;
            ioDelay(2000);
            startSPITransfer();
            ioDelay(200);
            writeSPI(0x05);
            writeSPI(0x00);
            ret = readSPI();
            endSPITransfer();
            return ret;
        }

        uint8_t spi_read_status_register2() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - READ_STATUS_REGISTER_2");
            uint8_t ret;
            ioDelay(2000);
            startSPITransfer();
            ioDelay(200);
            writeSPI(0x35);
            writeSPI(0x00);
            ret = readSPI();
            endSPITransfer();
            return ret;
        }

        uint8_t spi_write_status_register() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - WRITE_STATUS_REGISTER");
            uint8_t ret;
            ioDelay(2000);
            startSPITransfer();
            ioDelay(200);
            writeSPI(0x01);
            writeSPI(0x00);
            ret = readSPI();
            endSPITransfer();
            return ret;
        }

        void spi_block_erase(uint32_t addr) {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - BLOCK_ERASE(64KB) (0x%08X)", addr);
            // remove 64KB (0x10000 byte)
            ioDelay(500);
            spi_write_enable();
            ioDelay(500);
            startSPITransfer();
            ioDelay(200);
            writeSPI(0xD8);
            writeSPI((addr >> 16) & 0xFF);
            writeSPI((addr >> 8) & 0xFF);
            writeSPI(addr & 0xFF);
            endSPITransfer();
            while (spi_read_status_register1() & 1);
            ioDelay(2000);
            spi_write_disable();
        }

        void spi_data_read(uint32_t addr, uint8_t *buf, uint32_t length) {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - READ_DATA (0x%08X : %0x%08X)", addr, length);
            ioDelay(500);
            startSPITransfer();
            ioDelay(200);
            writeSPI(0x03);
            writeSPI((addr >> 16) & 0xFF);
            writeSPI((addr >> 8) & 0xFF);
            writeSPI(addr & 0xFF);
            while (length) {
                writeSPI(0x00);
                length--;
                *buf++ = readSPI();
            }
            waitSPIBusy();
            ioDelay(200);
            endSPITransfer();
        }

        // write 0x100 bytes
        void spi_page_program(uint32_t addr, uint8_t *buf) {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - PAGE_PROGRAM (0x%08X)", addr);
            uint8_t *end = buf + 0x100;
            uint8_t *p = buf;

            spi_write_enable();
            ioDelay(800);

            startSPITransfer();
            ioDelay(200);
            writeSPI(0x02);
            writeSPI((addr >> 16) & 0xFF);
            writeSPI((addr >> 8) & 0xFF);
            writeSPI(addr & 0xFF);

            do {
                writeSPI(*p++);
            } while (p != end);
            endSPITransfer();

            ioDelay(800);
            while (spi_read_status_register1() & 1);
            ioDelay(500);
            spi_write_disable();
        }

        void spi_cont_read_mode_reset() {
            logMessage(LOG_DEBUG, "Ace3DS+: SPI - CONTINUOUS_READ_MODE_RESET");
            startSPITransfer();
            ioDelay(200);
            writeSPI(0xFF);
            endSPITransfer();
        }

        uint8_t ace3ds_reset() {
            // maybe not support cart
            if (version.hardware == 0x32) {
                return 0;
            }
            if (version.hardware != 0x33 && version.hardware <= 0x9F) {
                return 0;
            }
            uint32_t chip_id = spi_read_jedec_id();
            if (chip_id == 0x208013) {
                logMessage(LOG_DEBUG, "Ace3DS+: type 1");
                spi_powerdown();
                return 1;
            } else {
                logMessage(LOG_DEBUG, "Ace3DS+: type 2");
                for (uint8_t i = 0; i < 5; i++) {
                    logMessage(LOG_DEBUG, "Ace3DS+: spi write enable");
                    spi_write_enable();
                }
                logMessage(LOG_DEBUG, "Ace3DS+: spi block 0 clear");
                spi_block_0_erase();
                logMessage(LOG_DEBUG, "Ace3DS+: spi send chip erase");
                spi_chip_erase();
                ioDelay(1000);
                return 2;
            }
        }

        void ace3ds_load_version() {
            uint32_t value;

            sendCommand(cmdGetHWVersion, 4, (uint8_t*)&value, 0x1FFF);
            //uint32_t buf[0x80];
            //sendCommand(cmdGetHWVersion, 512, (uint8_t*)buf, 0x3F1FFF);
            //for (uint32_t i = 0; i < 0x80; i++) {
            //    logMessage(LOG_INFO, "Ace3DS+: versions - 0%08X: 0x%08x", i, buf[i]);
            //}
            //value = buf[0];

            version.hardware = (value >> 16) & 0xFF;
            version.firmware = value & 0xFFFF;

            logMessage(LOG_INFO, "Ace3DS+: versions - 0x%08x 0x%08X", version.hardware, version.firmware);
        }

        uint32_t ace3ds_chip_id() {
            uint32_t idx;
            ace3ds_reset();

            if (version.hardware <= 0x2F) {
                logMessage(LOG_DEBUG, "Ace3DS+: wrong hw version");
                return version.hardware;
            }

            uint8_t buf[0x200];
            // flag: 0xA1586000 under bf enable environ
            sendCommand(cmdC6, 0x200, buf, 0x180000);
            uint32_t checksum = 0;
            for (idx = 0; idx < 0x200; idx++) {
                checksum = (checksum * 2) | (buf[idx] & 1);
            }

            logMessage(LOG_DEBUG, "Ace3DS+: C6 (0x%08x)", checksum);

            uint32_t *p = (uint32_t*)buf;
            for (idx = 0; idx < 0x80; idx++) {
                uint32_t t = buf[idx];
                buf[idx] = (idx % 2) ? 2 * t : t >> 1;
            }

            for (idx = 0; idx < 0x200; idx++) {
                uint32_t t = checksum * 2;
                if ((2 * checksum ^ checksum) & 0x8000) {
                    t |= 1;
                }
                buf[idx] = t & 0x8000 ? buf[idx] | 1 : buf[idx] & 0xFE;
                checksum = t;
            }

            // flag: 0xE1586000 under bf enable environ
            sendCommand(cmdC7, 0x200, buf, 0x40180000);
            logMessage(LOG_DEBUG, "Ace3DS+: C7");

            ioDelay(10000);
            spi_cont_read_mode_reset();
            ioDelay(10000000);
            uint32_t jedec = spi_read_jedec_id();
            logMessage(LOG_INFO, "Ace3DS+: chip_id (0x%08x)", jedec);
            return jedec;
        }

        /*
        void ace3ds_unk_b4_0(uint32_t addr, uint8_t value) {
            uint8_t cmdbuf[8];
            logMessage(LOG_DEBUG, "Ace3DS+: unk B4_0(0x%08x : 0x%02x)", addr, value);
            memcpy(cmdbuf, cmdB4, 8);
            cmdbuf[1] = (addr >> 24) & 0xFF;
            cmdbuf[2] = (addr >> 16) & 0xFF;
            // cmdbuf[3] = 0;
            cmdbuf[4] = value;
            // cmdbuf[5] = 0;
            cmdbuf[6] = (addr >>  8) & 0xFF;
            cmdbuf[7] = (addr >>  0) & 0xFF;

            uint32_t check;
            do {
                sendCommand(cmdbuf, 4, (uint8_t*)&check, 0x180000);
            } while (check != 0);
        }

        void ace3ds_unk_b4_1(uint32_t addr, uint8_t value) {
            uint8_t cmdbuf[8];
            logMessage(LOG_DEBUG, "Ace3DS+: unk B4_1(0x%08x : 0x%02x)", addr, value);
            memcpy(cmdbuf, cmdB4, 8);
            cmdbuf[1] = (addr >> 24) & 0xFF;
            cmdbuf[2] = (addr >> 16) & 0xFF;
            cmdbuf[3] = (addr >>  8) & 0xFF;
            cmdbuf[4] = (addr >>  0) & 0xFF;
            cmdbuf[5] = value;

            uint32_t check;
            do {
                sendCommand(cmdbuf, 4, (uint8_t*)&check, 0x180000);
            } while (check != 0);
        }
        */

        void ace3ds_read_sector(uint32_t sector, uint8_t *buf, uint8_t type) {
            uint8_t cmdbuf[8];
            logMessage(LOG_DEBUG, "Ace3DS+: read sector(0x%08x)", sector);

            memcpy(cmdbuf, cmdReadStatus, 8);
            cmdbuf[1] = (sector >> 24) & 0xFF;
            cmdbuf[2] = (sector >> 16) & 0xFF;
            cmdbuf[3] = (sector >>  8) & 0xFF;
            cmdbuf[4] = (sector >>  0) & 0xFF;
            cmdbuf[5] = type;

            uint32_t check;
            do {
                sendCommand(cmdbuf, 4, (uint8_t*)&check, 0x180000);
            } while (check != 0);

            memcpy(cmdbuf, cmdReadSector, 8);
            cmdbuf[1] = (sector >> 24) & 0xFF;
            cmdbuf[2] = (sector >> 16) & 0xFF;
            cmdbuf[3] = (sector >>  8) & 0xFF;
            cmdbuf[4] = (sector >>  0) & 0xFF;
            cmdbuf[5] = type;

            sendCommand(cmdbuf, 0x200, buf, 0x180000);
        }

        void ace3ds_write_sector(uint32_t sector, const uint8_t *buf) {
        }

    protected:
        static const uint8_t cmdGetHWVersion[8];
        //static const uint8_t cmdB4[8];
        static const uint8_t cmdReadStatus[8];
        static const uint8_t cmdReadSector[8];
        static const uint8_t cmdWriteSector[8];
        static const uint8_t cmdWriteStatus[8];
        static const uint8_t cmdC6[8];
        static const uint8_t cmdC7[8];

        uint32_t versions;
        Ace3DSPlusVersion version;
        uint32_t chip_id;

    public:
        Ace3DSPlus() : Flashcart("Ace3DS+", 0x10000) { }

        const char *getAuthor() { return "d3m3vilurr"; }
        const char *getDescription() {
            return "Works with Ace3DS+ variants:\n"
                "untested yet";
        }

        size_t getMaxLength()
        {
            return 0x10000;
        }

        bool initialize() {
            logMessage(LOG_INFO, "Ace3DS+: initialize");

            /*
            if (platform::CAN_RESET) {
                if (!ntrcard::init()) {
                    logMessage(LOG_ERR, "Ace3DS+: ntrcard::init failed");
                    return false;
                }

                ntrcard::state.hdr_key1_romcnt = ntrcard::state.key1_romcnt = 0x81808F8;
                ntrcard::state.hdr_key2_romcnt = ntrcard::state.key2_romcnt = 0x416657;
                ntrcard::state.key2_seed = 0;
                if (!ntrcard::initKey1(BlowfishKey::NTR)) {
                    logMessage(LOG_ERR, "Ace3DS+: init key1 (key=NTR) failed");
                    return false;
                }
                if (!ntrcard::initKey2()) {
                    logMessage(LOG_ERR, "Ace3DS+: init key2 failed");
                    return false;
                }
            }

            if (ntrcard::state.status != ntrcard::Status::KEY2) {
                logMessage(LOG_ERR, "Ace3DS+: wrong state (%d)", ntrcard::state.status);
                return false;
            }
            */

            ace3ds_load_version();
            chip_id = ace3ds_chip_id();

            /*
            if (chip_id != 0xEF3015 &&
                    chip_id != 0xEF4015 &&
                    chip_id != 0xC84015 &&
                    chip_id != 0xEF4016) {
                // return false
            }
            // 2.xx
            if (version.hardware == 0x32) {
                // return false
            }
            */

            if (version.hardware == 0 && version.firmware == 0) {
                return false;
            }
            logMessage(LOG_DEBUG, "Ace3DS+: supported cart");
            return true;
        }

        void shutdown() { }

        bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
            logMessage(LOG_INFO, "Ace3DS+: readFlash(addr=0x%08x, size=0x%x)", address, length);
            for (uint32_t curpos=0; curpos < length; curpos += 0x100) {
                spi_data_read(address + curpos, buffer + curpos, 0x100);
                showProgress(curpos+1,length, "Reading");
            }
            /*
            for (uint32_t curpos=0; curpos < length; curpos += 0x200) {
                uint8_t type = (curpos & 0xFF0000) >> 16;
                uint32_t pos = curpos & 0xFFFF;
                ace3ds_read_sector(address + pos, buffer + curpos, type);
                showProgress(curpos+1,length, "Reading");
            }*/

            return true;
        }

        bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) { return true; }
        bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { return true; }
};

const uint8_t Ace3DSPlus::cmdGetHWVersion[8] = {0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//const uint8_t Ace3DSPlus::cmdB4[8] = {0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Ace3DSPlus::cmdC6[8] = {0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Ace3DSPlus::cmdC7[8] = {0xC7, 0x5A, 0x55, 0xAA, 0xA5, 0x3C, 0xFF, 0xC3};

// dldi things
const uint8_t Ace3DSPlus::cmdReadStatus[8] = {0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Ace3DSPlus::cmdReadSector[8] = {0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Ace3DSPlus::cmdWriteSector[8] = {0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Ace3DSPlus::cmdWriteStatus[8] = {0xBC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Ace3DSPlus ace3dsplus;
}
