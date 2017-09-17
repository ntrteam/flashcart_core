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

// Allow platforms to not provide these.
__attribute__((weak)) void Flashcart::showProgress(uint32_t current, uint32_t total, const char* status_string) { ; }

__attribute__((weak)) int Flashcart::logMessage(const char *fmt, ...) { return 0; }
