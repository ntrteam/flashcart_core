#include "r4igold3ds.h"

const uint8_t R4i_Gold_3DS::cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdReadFlash[8] = {0xA5, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdEraseFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x00, 0x00};
const uint8_t R4i_Gold_3DS::cmdWriteByteFlash[8] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00};

void R4i_Gold_3DS::waitFlashBusy() {
    uint32_t state;
    do {
        // TODO: Can we not use delay here, and actually figure out what's going on with this?
        // I think we're probably not checking the right bit, or something like that.
        ioDelay( 16 * 10 );
        sendCommand(ak2i_cmdWaitFlashBusy, 4, (uint8_t *)&state);
    } while ((state & 1) != 0);
}


uint8_t R4i_Gold_3DS::encrypt(uint8_t dec)
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

static void R4i_Gold_3DS::encrypt_memcpy(uint8_t *dst, uint8_t *src, uint32_t length)
{
    for(int i = 0; i < (int)length; ++i)
        dst[i] = encrypt(src[i]);
}

// Included for completion's sake.
// uint8_t decrypt(uint8_t enc)
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


size_t R4i_Gold_3DS::sendReadCommand(uint8_t *outbuf, uint32_t address) {
    uint8_t cmdbuf[8];
    memcpy(cmdbuf, cmdReadFlash, 8);
    cmdbuf[1] = (address >> 16) & 0xFF;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 0x200, outbuf);

    return 0x200; // Data
}

size_t R4i_Gold_3DS::sendEraseCommand(uint32_t address) {
    uint8_t cmdbuf[8];
    uint32_t status;
    memcpy(cmdbuf, cmdEraseFlash, 8);
    cmdbuf[1] = (address >> 16) & 0xFF;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;

    // TODO: Add actual flags.
    sendCommand(cmdbuf, 4, (uint8_t *)&status);

    waitFlashBusy();
    return 0x10000; // Erase cluster size.
}

size_t R4i_Gold_3DS::sendWriteCommand(uint32_t address, uint8_t *inbuf) {
    uint8_t cmdbuf[8];
    uint32_t status;
    memcpy(cmdbuf, cmdWriteByteFlash, 8);
    cmdbuf[1] = (address >> 16) & 0xFF;
    cmdbuf[2] = (address >>  8) & 0xFF;
    cmdbuf[3] = (address >>  0) & 0xFF;
    cmdbuf[4] = *inbuf;

    sendCommand(cmdbuf, 4, (uint8_t *)&status);
    waitFlashBusy();

    return 1;
}

R4i_Gold_3DS::R4i_Gold_3DS() : Flashcart("R4i Gold 3DS", 0x400000, 0x10000) {
}

bool R4i_Gold_3DS::setup() {
    reset(); // Known good state.

    uint32_t hw_revision;
    sendCommand(cmdGetHWRevision, 4, (uint8_t*)&hw_revision); // Get HW Revision
    if (hw_revision != 0xA7A7A7A7) return false;

    // Doesn't use any locking or unlocking functions?
    return true;
}

void R4i_Gold_3DS::cleanup() { // Nothing needed?
}

void R4i_Gold_3DS::writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size)
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
