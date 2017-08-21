#pragma once

#include "../device.h"

class Genuine_ak2i_44 : public Flashcart {
protected:
    static const uint8_t ak2i_cmdGetHWRevision[8];
    static const uint8_t ak2i_cmdWaitFlashBusy[8];
    static const uint8_t ak2i_cmdSetMapTableAddress[8];
    static const uint8_t ak2i_cmdActiveFatMap[8];
    static const uint8_t ak2i_cmdUnlockFlash[8];
    static const uint8_t ak2i_cmdLockFlash[8];
    static const uint8_t ak2i_cmdUnlockASIC[8];
    static const uint8_t ak2i_cmdWaitFlashBusy[8];

    static const uint8_t ak2i_cmdReadFlash[8];
    static const uint8_t ak2i_cmdEraseFlash[8];
    static const uint8_t ak2i_cmdWriteByteFlash[8];

    Flashcart_mode curmode;
    virtual void switchMode(Flashcart_mode mode);
    virtual size_t sendReadCommand(uint8_t *outbuf, uint32_t address);
    virtual size_t sendEraseCommand(uint32_t address);
    virtual size_t sendWriteCommand(uint32_t address, const uint8_t *inbuf);

    void waitFlashBusy();

    Genuine_ak2i_44(const char *_description, size_t _max_length, uint32_t _page_size);
public:
    Genuine_ak2i_44();

    virtual bool setup();
    virtual void cleanup();

    // This function follows a read-modify-write cycle:
    //  - Read from flash to prevent accidental erasure of things not overwritten
    //  - Modify the data read, mostly by memcpying data in, perhaps 'encrypting' it first.
    //  - Write the data back to flash, now that we have made our modifications.
    virtual void writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size);
};

class Genuine_ak2i_81 : public Genuine_ak2i_44 {
protected:
    static const uint8_t ak2i_cmdSetFlash1681_81[8];
    static const uint8_t ak2i_cmdEraseFlash81[8];
    static const uint8_t ak2i_cmdWriteByteFlash81[8];

    virtual size_t sendEraseCommand(uint32_t address);
    virtual size_t sendWriteCommand(uint32_t address, const uint8_t *inbuf);
    virtual void switchMode(Flashcart_mode mode);

public:
    Genuine_ak2i_81();
    virtual bool setup();
};
