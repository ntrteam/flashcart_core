#include "device.h"

class R4i_Gold_3DS : Flashcart {
public:
    R4i_Gold_3DS() : Flashcart("R4i Gold 3DS", 0x400000) { }

    bool initialize() { return false; }
    void shutdown() { }
    
    void readFlash(uint32_t address, uint32_t length, uint8_t *buffer) { }
    void writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) { }
    void injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { }
};

R4i_Gold_3DS r4i_gold_3ds;