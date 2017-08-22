#pragma once

#include <cstdint>
#include <cstddef>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

enum Flashcart_mode {
    FLASH_READ,
    FLASH_WRITE
};

// Utility -- s must be a power of two and have no side effects.
#define PAGE_ROUND_UP(x, s) ( ((x) + (s)-1)  & (~((s)-1)) )

class Flashcart {
    public:
        Flashcart(const char *_description, size_t _max_length, uint32_t _page_size);
        virtual bool setup() = 0;
        const char *getDescription() { return description; }
        size_t getMaxLength() { return max_length; }
        virtual void readFlash(uint32_t address, uint32_t length, uint8_t *buf);
        virtual void writeFlash(uint32_t address, uint32_t length, const uint8_t *buf);
        virtual void cleanup() = 0;

        virtual void writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) = 0;

        static uint32_t getChipID();
        static void reset();

    protected:
        virtual void eraseFlash(uint32_t address, uint32_t length);

        virtual uint32_t getLength(uint32_t length, uint32_t offset);
        
        virtual size_t sendReadCommand(uint8_t *outbuf, uint32_t address) = 0;
        virtual size_t sendEraseCommand(uint32_t address) = 0;
        virtual size_t sendWriteCommand(uint32_t address, const uint8_t *inbuf) = 0;

        // platform specific! Find a better way to handle these.
        static void platformInit();
        static void sendCommand(const uint8_t *cmdbuf, uint16_t response_len, uint8_t *resp, uint32_t flags=32);
        static void showProgress(uint32_t current, uint32_t total);

        //const size_t read_granularity, write_granularity, erase_granularity; 
        const size_t max_length;
        const uint32_t page_size;
        const char *const description;

        static const uint8_t NTR_cmdDummy[8];
        static const uint8_t NTR_cmdChipid[8];
};
