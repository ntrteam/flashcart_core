#pragma once

#include "../device.h"

class R4SDHC_DualCore : public Flashcart {
private:
    static uint8_t decrypt(uint8_t enc);

protected:
    static const uint8_t cmdReadFlash[8];
    static const uint8_t cmdEraseFlash[8];
    static const uint8_t cmdWriteByteFlash[8];

    static const uint8_t cmdUnkD0AA[8];
    static const uint8_t cmdUnkD0[8];

    virtual size_t sendReadCommand(uint8_t *outbuf, uint32_t address);
    virtual size_t sendEraseCommand(uint32_t address);
    virtual size_t sendWriteCommand(uint32_t address, const uint8_t *inbuf);

public:
    virtual bool setup();
    virtual void cleanup();

    // TODO:
    virtual void writeBlowfishAndFirm(uint8_t *template_data, uint32_t template_size) = 0;
};
