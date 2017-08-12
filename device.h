#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

using std::uint8_t;
using std::uint32_t;

enum Flashcart_mode {
    FLASH_READ,
    FLASH_WRITE
};

// Utility -- s must be a power of two and have no side effects.
#define PAGE_ROUND_UP(x, s) ( ((x) + (s)-1)  & (~((s)-1)) )

class Flashcart {
    public:
        Flashcart();
        virtual bool setup() = 0;
        const char *getDescription() { return description; }
        size_t getMaxLength() { return max_length; }
        virtual void readFlash(uint32_t address, uint32_t length, uint8_t *buf);
        virtual void writeFlash(uint32_t address, uint32_t length, const uint8_t *buf);
        virtual void cleanup() = 0;

        virtual void writeBlowfishAndFirm(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) = 0;

        static uint32_t getHardwareVersion();
        static uint32_t getChipID();
        static Flashcart *detectCart();

    protected:
        static void reset();
        static void waitFlashBusy();
        virtual void eraseFlash(uint32_t address, uint32_t length);

        // Default is do-nothing.
        virtual void switchMode(Flashcart_mode mode) { (void)mode; };

        virtual size_t formatReadCommand(uint8_t *cmdbuf, uint32_t address) = 0;
        virtual size_t formatEraseCommand(uint8_t *cmdbuf, uint32_t address) = 0;
        virtual size_t formatWriteCommand(uint8_t *cmdbuf, uint32_t address, uint8_t value) = 0;
        virtual uint32_t getLength(uint32_t length, uint32_t offset);

        // platform specific! Find a better way to handle these.
        static void platformInit();
        static void sendCommand(const uint8_t *cmdbuf, uint16_t response_len, uint8_t *resp);
        static void showProgress(uint32_t current, uint32_t total);

        size_t max_length;
        uint32_t page_size;
        const char *description;

        static const uint8_t NTR_cmdDummy[8];
        static const uint8_t NTR_cmdChipid[8];
        static const uint8_t ak2i_cmdGetHWRevision[8];
        static const uint8_t ak2i_cmdWaitFlashBusy[8];

    private:
        static Flashcart *detected_cache;
};

extern std::vector<Flashcart*> flashcart_list;
