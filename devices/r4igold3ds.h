#pragma once

#include <cstring>
#include <algorithm>
using std::memcpy;

#include "../device.h"
#define BIT(n) (1 << (n))

class R4i_Gold_3DS : Flashcart {
private:
    static uint8_t encrypt(uint8_t dec);
    static void encrypt_memcpy(uint8_t *dst, uint8_t *src, uint32_t length);

protected:
    static const uint8_t cmdReadFlash[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];
    static const uint8_t cmdGetHWRevision[8];
    static const uint8_t cmdWaitFlashBusy[8];

    void waitFlashBusy();

    virtual size_t sendReadCommand(uint8_t *outbuf, uint32_t address);
    virtual size_t sendEraseCommand(uint32_t address);
    virtual size_t sendWriteCommand(uint32_t address, uint8_t *inbuf);

public:
    R4i_Gold_3DS();

    virtual bool setup();
    virtual void cleanup();

    virtual void writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size);
};
