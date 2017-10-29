#pragma once

#include <cstdint>

#include "ntrcard.h"

namespace flashcart_core {
enum log_priority {
    LOG_DEBUG = 0, // Will probably spam logs, only use when debugging.
    LOG_INFO, // Reccomended default logfile level
    LOG_NOTICE, // Reccomended default display level, if implemented.
    LOG_WARN,
    LOG_ERR,
    LOG_PRIORITY_MAX
};

// override these in platform.cpp
namespace platform {
/// Whether the platform has hardware KEY2 support.
/// If false, flashcart_core will do KEY2 in software. (TODO IMPL)
/// If unset, defaults to false.
extern const bool HAS_HW_KEY2;

/// Whether the platform can reset a cart, whether electronically or by asking
/// the user to physically reinsert the card.
/// If unset, defaults to false.
extern const bool CAN_RESET;

/// The initial encryption status when flashcart_core first gains control.
/// If unset, defaults to RAW.
extern const ntrcard::Status INITIAL_ENCRYPTION;

std::int32_t resetCard();
bool sendCommand(const std::uint8_t *cmdbuf, std::uint16_t response_len, std::uint8_t *resp, ntrcard::OpFlags flags);
void ioDelay(std::uint32_t us);
void initBlowfishPS(std::uint32_t (&ps)[ntrcard::BLOWFISH_PS_N], ntrcard::BlowfishKey key = ntrcard::BlowfishKey::NTR);
void initKey2Seed(std::uint64_t x, std::uint64_t y);

void startSPITransfer();
void writeSPI(uint8_t op);
uint8_t readSPI();
void waitSPIBusy();
void endSPITransfer();

void showProgress(std::uint32_t current, std::uint32_t total, const char* status_string);
int logMessage(log_priority priority, const char *fmt, ...);
}
}
