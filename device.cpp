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
        showProgress(curpos, length - 1, "Reading");
        uint32_t read_sz = rawRead(address + curpos, length - curpos, buffer + curpos);
        if (!read_sz) break;

        curpos += read_sz;
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
            showProgress(curpos, length - 1, "Writing");
            uint32_t write_sz = rawWrite(address + curpos, length - curpos, buffer + curpos);
            if (!write_sz) break;

            curpos += write_sz;
        }
    }

    return curpos;
}

// Default do-nothing implementation, override when necessary.
void Flashcart::setWriteState(bool state) { (void)state; }
