#include <cstring>
#include <algorithm>

#include "../device.h"

const uint8_t Genuine_ak2i_44::ak2i_cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdSetMapTableAddress[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdActiveFatMap[8] = {0xC2, 0x55, 0xAA, 0x55, 0xAA, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdUnlockFlash[8] = {0xC2, 0xAA, 0x55, 0xAA, 0x55, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdLockFlash[8] = {0xC2, 0xAA, 0xAA, 0x55, 0x55, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdUnlockASIC[8] = {0xC2, 0xAA, 0x55, 0x55, 0xAA, 0x00, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdReadFlash[8] = {0xB7, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdEraseFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
const uint8_t Genuine_ak2i_44::ak2i_cmdWriteByteFlash[8] = {0xD4, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};

void Genuine_ak2i_44::waitFlashBusy() {
    uint32_t state;
    do {
        // TODO: Can we not use delay here, and actually figure out what's going on with this?
        // I think we're probably not checking the right bit, or something like that.
        ioDelay( 16 * 10 );
        sendCommand(ak2i_cmdWaitFlashBusy, 4, (uint8_t *)&state);
    } while ((state & 1) != 0);
}

void Genuine_ak2i_44::switchMode(Flashcart_mode mode) {
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

size_t Genuine_ak2i_44::sendReadCommand(uint8_t *outbuf, uint32_t address) {
    uint8_t cmdbuf[8];
    memcpy(cmdbuf, ak2i_cmdReadFlash, 8);
    cmdbuf[1] = (address >> 24) & 0xFF;
    cmdbuf[2] = (address >> 16) & 0xFF;
    cmdbuf[3] = (address >>  8) & 0xFF;
    cmdbuf[4] = (address >>  0) & 0xFF;

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 0x200, outbuf);

    return 0x200; // Data
}

size_t Genuine_ak2i_44::sendEraseCommand(uint32_t address) {
    uint8_t cmdbuf[8];
    memcpy(cmdbuf, ak2i_cmdEraseFlash, 8);
    cmdbuf[1] = (address >> 16) & 0x1F;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 0, nullptr);

    waitFlashBusy();

    return 0x10000; // Erase cluster size.
}

size_t Genuine_ak2i_44::sendWriteCommand(uint32_t address, const uint8_t *inbuf) {
    uint8_t cmdbuf[8];
    memcpy(cmdbuf, ak2i_cmdWriteByteFlash, 8);
    cmdbuf[1] = (address >> 16) & 0x1F;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;
    cmdbuf[4] = *inbuf;

    waitFlashBusy();

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 0, nullptr);

    return 1;
}

Genuine_ak2i_44::Genuine_ak2i_44() : Flashcart("Acekard 2i HW-44", 0x200000, 0x10000) {
}

Genuine_ak2i_44(const char *_description, size_t _max_length, uint32_t _page_size) : Flashcart(_description, _max_length, _page_size) {
}

bool Genuine_ak2i_44::setup() {
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

void Genuine_ak2i_44::cleanup() {
    uint32_t garbage;
    switchMode(FLASH_READ); // Lock flash.
    sendCommand(ak2i_cmdActiveFatMap, 4, (uint8_t *)&garbage);
}

void Genuine_ak2i_44::writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
{
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

const uint8_t Genuine_ak2i_81::ak2i_cmdSetFlash1681_81[8] = {0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x06};
const uint8_t Genuine_ak2i_81::ak2i_cmdEraseFlash81[8] = {0xD4, 0x00, 0x00, 0x00, 0x30, 0x80, 0x00, 0x35};
const uint8_t Genuine_ak2i_81::ak2i_cmdWriteByteFlash81[8] = {0xD4, 0x00, 0x00, 0x00, 0x30, 0xa0, 0x00, 0x63};

size_t Genuine_ak2i_81::sendEraseCommand(uint32_t address) {
    uint8_t cmdbuf[8];
    memcpy(cmdbuf, ak2i_cmdEraseFlash81, 8);
    cmdbuf[1] = (address >> 16) & 0xFF;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 0, nullptr);
    waitFlashBusy();

    return 0x10000; // Erase cluster size.
}

size_t Genuine_ak2i_81::sendWriteCommand(uint32_t address, const uint8_t *inbuf) {
    uint8_t cmdbuf[8];
    memcpy(cmdbuf, ak2i_cmdWriteByteFlash81, 8);
    cmdbuf[1] = (address >> 16) & 0xFF;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;
    cmdbuf[4] = *inbuf;

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 0, nullptr);
    waitFlashBusy();

    return 1;
}

void Genuine_ak2i_81::switchMode(Flashcart_mode mode) {
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

Genuine_ak2i_81::Genuine_ak2i_81() : Genuine_ak2i_44("Acekard 2i HW-81", 0x1000000, 0x10000) {
}

bool Genuine_ak2i_81::setup() {
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
