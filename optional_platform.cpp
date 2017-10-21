// This file is so named to avoid build-time object file conflicts with platform.cpp in ntrboot_flasher

#include <cstdint>

#include "platform.h"

namespace flashcart_core {
namespace platform {
// Allow platforms to not provide these.
__attribute__((weak)) void showProgress(std::uint32_t current, std::uint32_t total, const char* status_string) { ; }

__attribute__((weak)) int logMessage(log_priority priority, const char *fmt, ...) { return 0; }

__attribute__((weak)) extern const bool HAS_HW_KEY2 = false;

__attribute__((weak)) extern const bool BYPASS_CART_INIT = false;

__attribute__((weak)) void initKey2Seed(std::uint64_t x, std::uint64_t y) {}

__attribute__((weak)) void enableSecureFlagOverride() {}

__attribute__((weak)) void disableSecureFlagOverride() {}
}
}
