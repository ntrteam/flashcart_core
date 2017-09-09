#include <cstdio>
#include <cstdlib>

#include "device.h"

std::vector<Flashcart*> *flashcart_list = nullptr;

Flashcart::Flashcart(const char* name, const size_t max_length)
    : m_name(name), m_max_length(max_length) {
    if (flashcart_list == nullptr) {
        flashcart_list = new std::vector<Flashcart*>();
    }
    flashcart_list->push_back(this);
}

uint32_t Flashcart::readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
    if (length <= 0) return 0;
    setWriteState(false);

    uint32_t curpos;
    for (curpos = 0; curpos < length;) {
        uint32_t read_sz = rawRead(address + curpos, length - curpos, buffer + curpos);
        if (!read_sz) break;

        curpos += read_sz;
        showProgress(curpos, length, "Reading");
    }

    return curpos;
}

uint32_t Flashcart::writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
    if (length <= 0) return 0;
    setWriteState(true);

    uint32_t curpos;
    for (curpos = 0; address < length;) {
        uint32_t erase_sz = rawErase(address + curpos);
        if (!erase_sz) break;

        for (uint32_t end = curpos + erase_sz; curpos < end;) {
            uint32_t write_sz = rawWrite(address + curpos, length - curpos, buffer + curpos);
            if (!write_sz) break;

            curpos += write_sz;
            showProgress(curpos, length, "Writing");
        }
    }

    return curpos;
}

template <class T, void (T::*fn)(uint32_t, uint8_t *), uint32_t alignment>
uint32_t Flashcart::read_wrapper(uint32_t address, uint32_t length, uint8_t *buffer) {
    uint8_t *buf = buffer;
    uint32_t round_addr = PAGE_ROUND_DOWN(address, alignment);
    uint32_t offset = address - round_addr;
    length = std::min<uint32_t>(length, alignment - offset);
    if (length < alignment || offset) {
        buf = (uint8_t*)malloc(alignment);
    }

    (this->*(fn))(round_addr, buf);

    if (buf != buffer) {
        memcpy(buffer, buf + offset, length);
        free(buf);
    }

    return length;
}

// Infinite recursion if write alignment and erase alignment do not line up.
// writeFlash -> erase_wrapper -> writeFlash...
// It is also not a good idea to use this if alignment for writing is not 1.
template <class T, void (T::*fn)(uint32_t), uint32_t alignment>
uint32_t Flashcart::erase_wrapper(uint32_t address) {
    uint32_t round_addr = PAGE_ROUND_DOWN(address, alignment);
    uint32_t offset = address - round_addr;
    uint8_t *extra = nullptr;
    if (offset) {
        extra = (uint8_t*)malloc(offset);
        readFlash(round_addr, offset, extra);
    }

    (this->*(fn))(round_addr);

    if (extra != nullptr) {
        writeFlash(round_addr, offset, extra);
        free(extra);
    }

    return alignment - offset;
}

// This should only be used for carts w/ multi-byte writes.
template <class T, void (T::*fn)(uint32_t, const uint8_t *), uint32_t alignment>
uint32_t Flashcart::write_wrapper(uint32_t address, uint32_t length, const uint8_t *buffer) {
    uint8_t *buf = buffer;
    uint32_t round_addr = PAGE_ROUND_DOWN(address, alignment);
    uint32_t offset = address - round_addr;
    length = std::min<uint32_t>(length, alignment - offset);

    if (length < alignment || offset) {
        buf = (uint8_t*)malloc(alignment);
        readFlash(round_addr, alignment, buf);
        memcpy(buf + offset, buffer, length);
    }

    (this->*(fn))(round_addr, buf);

    if (buf != buffer) {
        free(buf);
    }

    return length;
}

// Default do-nothing implementation, override when necessary.
void Flashcart::setWriteState(bool state) { (void)state; }
