// This file is so named to avoid build-time object file conflicts with platform.cpp in ntrboot_flasher

#include <cstdint>

#include "platform.h"

// Allow platforms to not provide these.
__attribute__((weak)) void platform::showProgress(std::uint32_t current, std::uint32_t total, const char* status_string) { ; }

__attribute__((weak)) int platform::logMessage(log_priority priority, const char *fmt, ...) { return 0; }

__attribute__((weak)) extern const bool platform::HAS_HW_KEY2 = false;

__attribute__((weak)) void platform::initKey2Seed(std::uint64_t x, std::uint64_t y) {}
