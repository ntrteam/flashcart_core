#include <cstdio>
#include <cstdlib>

#include "device.h"

std::vector<flashcart_core::Flashcart*> *flashcart_core::flashcart_list = nullptr;

flashcart_core::Flashcart::Flashcart(const char* name, const size_t max_length)
    : m_name(name), m_max_length(max_length) {
    if (flashcart_list == nullptr) {
        flashcart_list = new std::vector<Flashcart*>();
    }
    flashcart_list->push_back(this);
}
