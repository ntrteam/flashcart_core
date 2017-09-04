#ifdef DONT_INCLUDE_EXAMPLE_PLEASE

#include "device.h"

class Example : Flashcart {
    public:
        // Name & Size of Flash Memory
        Example() : Flashcart("Example Name", 0x400000) { }

        const char* getAuthor() { return "your name"; }
        const char* getDescription() { return "something helpful\nuse\newlines"; }
    

        /*
        called when the user selects this flashcart
        return false to error
        return true otherwise
        */
        bool initialize() {
            return true;
        }

        void shutdown() { }
        
        void readFlash(uint32_t address, uint32_t length, uint8_t *buffer) { }
        void writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) { }
        void injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) { }
};

// adds your cart to the list
Example example;

#endif