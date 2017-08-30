#include "device.h"

class Genuine_ak2i_44 : Flashcart {
public:
    Genuine_ak2i_44() : Flashcart("Acekard 2i HW-44", 0x200000) { }

    bool initialize() { return false; }
    void shutdown() { }
    
    void readFlash(uint32_t address, uint32_t length, uint8_t *buffer) { }
    void writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) { }
    void injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { }
};

class Genuine_ak2i_81 : Flashcart {
public:
    Genuine_ak2i_81() : Flashcart("Acekard 2i HW-81", 0x200000) { }

    bool initialize() { return false; }
    void shutdown() { }

    void readFlash(uint32_t address, uint32_t length, uint8_t *buffer) { }
    void writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) { }
    void injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { }
};

Genuine_ak2i_44 ak2i_44;
Genuine_ak2i_81 ak2i_81;