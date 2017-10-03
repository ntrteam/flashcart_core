#pragma once

#include <cstdint>

#include "ntrcard.h"

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
extern const bool HAS_HW_KEY2;

std::int32_t resetCard();
bool sendCommand(const std::uint8_t *cmdbuf, std::uint16_t response_len, std::uint8_t *resp, ntrcard::OpFlags flags);
void ioDelay(std::uint32_t us);
void initBlowfishPS(std::uint32_t (&ps)[ntrcard::BLOWFISH_PS_N]);
void initKey2Seed(std::uint64_t x, std::uint64_t y);

void showProgress(std::uint32_t current, std::uint32_t total, const char* status_string);
int logMessage(log_priority priority, const char *fmt, ...);
}
