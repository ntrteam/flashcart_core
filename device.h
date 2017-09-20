#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

// Utility -- s must be a power of two and have no side effects.
#define PAGE_ROUND_UP(x, s) ( ((x) + (s)-1)  & (~((s)-1)) )
#define PAGE_ROUND_DOWN(x, s) ( (x) & (~((s)-1)) )

#define BIT(n) (1 << (n))

enum log_priority {
    LOG_DEBUG = 0, // Will probably spam logs, only use when debugging.
    LOG_INFO, // Reccomended default logfile level
    LOG_NOTICE, // Reccomended default display level, if implemented.
    LOG_WARN,
    LOG_ERR
};

class Flashcart {
public:
    Flashcart(const char* name, const size_t max_length);

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) = 0;
    virtual bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) = 0;
    virtual bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) = 0;

    const char *getName() { return m_name; }
    virtual const char *getAuthor() { return "unknown"; }
    virtual const char *getDescription() { return ""; }
    virtual const size_t getMaxLength() { return m_max_length; }

protected:
    const char* m_name;
    const size_t m_max_length;

    // override these in platform.cpp
    static void sendCommand(const uint8_t *cmdbuff, uint16_t response_len, uint8_t *resp, uint32_t latency=32);
    static void showProgress(uint32_t current, uint32_t total, const char* status_string);
    static int logMessage(log_priority priority, const char *fmt, ...);
};

extern std::vector<Flashcart*> *flashcart_list;
