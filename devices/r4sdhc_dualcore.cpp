#include "device.h"

class R4SDHC_DualCore : Flashcart {
public:
    R4SDHC_DualCore() : Flashcart("R4 SDHC Dual Core", 0x400000) { }

    bool initialize() { return false; }
    void shutdown() { }
    
    void readFlash(uint32_t address, uint32_t length, uint8_t *buffer) { }
    void writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) { }
    void injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { }
};

// R4SDHC_DualCore r4sdhc_dualcore;