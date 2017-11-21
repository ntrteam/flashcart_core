#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include <ncgcpp/ntrcard.h>

#include "platform.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

// Utility -- s must be a power of two and have no side effects.
#define PAGE_ROUND_UP(x, s) ( ((x) + (s)-1)  & (~((s)-1)) )
#define PAGE_ROUND_DOWN(x, s) ( (x) & (~((s)-1)) )

#define BIT(n) (1 << (n))
namespace flashcart_core {
class Flashcart {
public:
    Flashcart(const char* name, const size_t max_length);

    inline bool initialize(ncgc::NTRCard *card) {
        m_card = card;
        return initialize();
    }
    virtual void shutdown() = 0;

    virtual bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) = 0;
    virtual bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) = 0;
    virtual bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) = 0;

    const char *getName() { return m_name; }
    virtual const char *getAuthor() { return "unknown"; }
    virtual const char *getDescription() { return ""; }
    virtual size_t getMaxLength() { return m_max_length; }

protected:
    const char* m_name;
    const size_t m_max_length;
    ncgc::NTRCard *m_card;

    virtual bool initialize() = 0;
};

extern std::vector<Flashcart*> *flashcart_list;
}
