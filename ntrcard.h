#pragma once

#include <cstdint>

namespace flashcart_core {
namespace ntrcard {
const uint32_t BLOWFISH_PS_N = 0x412;
const uint32_t BLOWFISH_P_N = 0x12;

enum class BlowfishKey {
    // TODO impl TWL
    NTR, B9RETAIL, B9DEV
};

enum class Status {
    RAW, KEY1, KEY2, UNKNOWN
};

struct State {
public:
    /// The chip ID, stored in `ntrcard_init`
    std::uint32_t chipid;
    /// The game code, from the header
    std::uint32_t game_code;
    /// The KEY1 ROMCNT settings, as in the header
    std::uint32_t hdr_key1_romcnt;
    /// The KEY2 ROMCNT settings, as in the header
    std::uint32_t hdr_key2_romcnt;
    /// The chip ID gotten in KEY1 mode
    std::uint32_t key1_chipid;
    /// The chip ID gotten in KEY2 mode
    std::uint32_t key2_chipid;


    /// The KEY1 ROMCNT settings, as used
    std::uint32_t key1_romcnt;
    /// KEY1 nonce iiijjj
    std::uint32_t key1_ij;
    /// KEY1 counter
    std::uint32_t key1_k;
    /// KEY1 nonce llll
    std::uint16_t key1_l;
    /// KEY1 Blowfish P array and S boxes
    std::uint32_t key1_ps[BLOWFISH_PS_N];
    /// KEY1 Blowfish key
    std::uint32_t key1_key[3];

    /// The KEY2 ROMCNT settings, as used
    std::uint32_t key2_romcnt;
    /// The KEY2 seed byte, as in the header
    std::uint8_t key2_seed;
    /// The KEY2 seed, set in `ntrcard_begin_key1`
    std::uint32_t key2_mn;
    /// The KEY2 X register
    std::uint64_t key2_x;
    /// The KEY2 Y register
    std::uint64_t key2_y;

    /// The current status
    Status status;
};

class OpFlags {
    /// The raw ROMCNT value.
    std::uint32_t romcnt;
public:
    constexpr bool bit(uint32_t bit) const { return !!(romcnt & (1 << bit)); }
    constexpr OpFlags bit(uint32_t bit, bool set) { return (romcnt & ~(1 << bit)) | (set ? (1 << bit) : 0); }

    /// Returns the delay before the response to a KEY1 command (KEY1 gap1)
    constexpr std::uint16_t pre_delay() const { return static_cast<uint16_t>(romcnt & 0x1FFF); }
    /// Returns the delay after the response to a KEY1 command (KEY1 gap2)
    constexpr std::uint16_t post_delay() const { return static_cast<uint16_t>((romcnt >> 16) & 0x3F); }
    /// Returns true if the secure area can be read 0x1000 bytes at a time
    constexpr bool large_secure_area_read() const { return bit(28); }
    /// Returns true if the command is KEY2-encrypted
    constexpr bool key2_command() const { return bit(22) && bit(14); }
    /// Returns true if the command is KEY2-encrypted
    constexpr bool key2_response() const { return bit(13) && bit(14); }
    /// Returns true if the slower CLK rate should be used (usually for raw commands)
    constexpr bool slow_clock() const { return bit(27); }

    /// Sets the the delay before the response to a KEY1 command (KEY1 gap1)
    constexpr OpFlags pre_delay(std::uint16_t value) { return (romcnt & ~0x1FFF) | (value & 0x1FFF); }
    /// Sets the delay after the response to a KEY1 command (KEY1 gap2)
    constexpr OpFlags post_delay(std::uint16_t value) { return (romcnt & ~(0x3F << 16)) | ((value & 0x3F) << 16); }
    /// Set if the secure area can be read 0x1000 bytes at a time
    constexpr OpFlags large_secure_area_read(bool value) { return bit(28, value); }
    /// Set if the command is KEY2-encrypted
    constexpr OpFlags key2_command(bool value) { return bit(22, value).bit(14, value || bit(13)); }
    /// Set if the command is KEY2-encrypted
    constexpr OpFlags key2_response(bool value) { return bit(13, value).bit(14, value || bit(22)); }
    /// Set if the slower CLK rate should be used (usually for raw commands)
    constexpr OpFlags slow_clock(bool value) { return bit(27, value); }

    constexpr operator std::uint32_t() const { return romcnt; }
    constexpr OpFlags(const std::uint32_t& from) : romcnt(from) {}
};

extern State state;

bool sendCommand(const std::uint8_t *cmdbuf, std::uint16_t resplen, std::uint8_t *resp, OpFlags flags = OpFlags(32));
bool sendCommand(const std::uint64_t cmd, std::uint16_t resplen, std::uint8_t *resp, OpFlags flags = OpFlags(32));
bool init();
bool initKey1(BlowfishKey key = BlowfishKey::NTR);
bool initKey2();
}
}
