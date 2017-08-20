#include <cstdio>
#include <cstdlib>

#include "device.h"
#include "delay.h" // TODO: platform specific! Remove this!

std::vector<Flashcart*> *flashcart_list = nullptr;

const uint8_t Flashcart::NTR_cmdDummy[8] = {0x9F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Flashcart::NTR_cmdChipid[8] = {0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Flashcart::ak2i_cmdWaitFlashBusy[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Flashcart::ak2i_cmdGetHWRevision[8] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Flashcart *Flashcart::detected_cache = nullptr; // No way to detect removal.

Flashcart::Flashcart() {
    if (flashcart_list == nullptr) {
        flashcart_list = new std::vector<Flashcart*>();
    }
    flashcart_list->push_back(this);
    page_size = 0x10000; // Default until further notice.
}

void Flashcart::reset() {
    uint8_t *garbage = new uint8_t[0x2000];
    if (!garbage) {
        puts("Memory allocation failure!");
        exit(EXIT_FAILURE);
    }

    platformInit();
    sendCommand(NTR_cmdDummy, 0x2000, garbage);
    delete[] garbage;  // TODO: prevent thrashing?
}

void Flashcart::waitFlashBusy() {
    uint32_t state;
    do {
        // TODO: Can we not use delay here, and actually figure out what's going on with this?
        // I think we're probably not checking the right bit, or something like that.
        ioDelay( 16 * 10 );
        sendCommand(ak2i_cmdWaitFlashBusy, 4, (uint8_t *)&state);
    } while ((state & 1) != 0);
}

void Flashcart::readFlash(uint32_t address, uint32_t length, uint8_t *buf) {
    uint8_t cmdbuf[8];

    switchMode(FLASH_READ);

    // Read less if less is requested, otherwise read the maximum that we can.
    length = getLength(length, address);
    uint32_t endaddr = address + length; // TODO: make sure that length is aligned?
    uint8_t *curpos = buf;

    for (uint32_t addr=address; addr < endaddr;) {
        size_t step = sendReadCommand(curpos, addr);
        addr += step;
        curpos += step;
        showProgress(addr-address,length);
    }
}

// TODO: Should we bother having this as a seperate function anymore?
//       perhaps we should just merge this into writeFlash.
void Flashcart::eraseFlash(uint32_t address, uint32_t length) {
    // TODO: make sure length is aligned?
    uint8_t cmdbuf[8];

    switchMode(FLASH_WRITE);

    length = getLength(length, address);
    uint32_t endaddr = address + length;

    for (uint32_t addr=address; addr < endaddr;) {
        size_t step = sendEraseCommand(addr);
        addr += step;
        curpos += step;
        waitFlashBusy(); // TODO: move inside the send*Command functions?
        showProgress(addr-address,length);
    }
    free(status);
}

void Flashcart::writeFlash(uint32_t address, uint32_t length, const uint8_t *buf) {
    uint8_t cmdbuf[8];

    switchMode(FLASH_WRITE);

    length = getLength(length, address);
    uint32_t endaddr = address + length;
    const uint8_t *curpos = buf;

    for (uint32_t page=address; page < endaddr; page+=page_size) {
        eraseFlash(page, page_size); // Erase a page before writing.

        for (uint32_t addr=page; addr < page + page_size; ++addr, ++curpos) {
            sendWriteByteCommand(cmdbuf, addr, *curpos);
            addr += step;
            curpos += step;
            waitFlashBusy(); // TODO: move inside the send*Command functions?
            showProgress(addr-address,length);
        }
    }
}

uint32_t Flashcart::getChipID() {
    uint32_t chipid;
    reset(); // Known good state.
    sendCommand(NTR_cmdChipid, 4, (uint8_t*)&chipid);
    return chipid;
}

uint32_t Flashcart::getLength(uint32_t length, uint32_t offset) {
    uint32_t curmax = max_length - offset;
    if (length == 0) return curmax;

    return std::min<uint32_t>(length, curmax);
}

uint32_t Flashcart::getHardwareVersion() {
    uint32_t hw_revision;
    reset(); // Known good state.
    sendCommand(ak2i_cmdGetHWRevision, 4, (uint8_t*)&hw_revision); // Get HW Revision
    return hw_revision;
}

Flashcart *Flashcart::detectCart() {
    // if (detected_cache) return detected_cache; // Uncomment once we have a way to detect cart removal.
    for (std::vector<Flashcart*>::iterator it = flashcart_list->begin() ; it != flashcart_list->end(); ++it) {
        if((*it)->setup()) {
            // detected_cache = *it;
            // (*it)->cleanup(); // We leave the cart setup.
            return *it;
        }
    }
    return nullptr;
}
