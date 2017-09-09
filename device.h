#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <vector>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

// Utility -- s must be a power of two and have no side effects.
#define PAGE_ROUND_UP(x, s) ( ((x) + (s)-1)  & (~((s)-1)) )
#define PAGE_ROUND_DOWN(x, s) ((x) & (~((s)-1)))

#define BIT(n) (1 << (n))

class Flashcart {
public:
    Flashcart(const char* name, const size_t max_length);

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual uint32_t readFlash(uint32_t address, uint32_t length, uint8_t *buffer);
    virtual uint32_t writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer);
    virtual bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) = 0;

    const char *getName() { return m_name; }
    virtual const char *getAuthor() { return "unknown"; }
    virtual const char *getDescription() { return ""; }
    virtual const size_t getMaxLength() { return m_max_length; }

protected:
    const char* m_name;
    const size_t m_max_length;

    // For devices that need to unlock flash and the like,
    // and have issues reading when flash is unlocked.
    virtual void setWriteState(bool state); // Default 'do-nothing' implementation provided.

    // uint32_t (Flashcart::*rawRead)(uint32_t address, uint32_t length, uint8_t *buffer);
    virtual uint32_t rawRead(uint32_t address, uint32_t length, uint8_t *buffer) = 0;
    virtual uint32_t rawErase(uint32_t address) = 0;
    virtual uint32_t rawWrite(uint32_t address, uint32_t length, const uint8_t *buffer) = 0;

    template <class T, void (T::*fn)(uint32_t, uint8_t *), uint32_t alignment>
    uint32_t read_wrapper(uint32_t address, uint32_t length, uint8_t *buffer);
    template <class T, void (T::*fn)(uint32_t), uint32_t alignment>
    uint32_t erase_wrapper(uint32_t address);
    template <class T, void (T::*fn)(uint32_t, const uint8_t *), uint32_t alignment>
    uint32_t write_wrapper(uint32_t address, uint32_t length, const uint8_t *buffer);

    // override these in platform.cpp
    static void sendCommand(const uint8_t *cmdbuff, uint16_t response_len, uint8_t *resp, uint32_t latency=32);
    static void showProgress(uint32_t current, uint32_t total, const char* status_string);
};

extern std::vector<Flashcart*> *flashcart_list;
