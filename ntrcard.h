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
};

struct OpFlags {
public:
    /// The delay before the response to a KEY1 command (KEY1 gap1)
    std::uint16_t pre_delay;
    /// The delay after the response to a KEY1 command (KEY1 gap2)
    std::uint16_t post_delay;
    /// True if the secure area can be read 0x1000 bytes at a time
    bool large_secure_area_read;
    /// True if the command is KEY2-encrypted
    bool key2_command;
    /// True if the command is KEY1-encrypted
    bool key2_response;
    /// True if the slower CLK rate should be used (usually for raw commands)
    bool slow_clock;

    static OpFlags parse(const std::uint32_t romcnt);
};

extern State state;

bool sendCommand(const std::uint8_t *cmdbuf, std::uint16_t resplen, std::uint8_t *resp, std::uint32_t flags = 32);
bool sendCommand(const std::uint64_t cmd, std::uint16_t resplen, std::uint8_t *resp, std::uint32_t flags = 32);
bool init();
bool initKey1(BlowfishKey key = BlowfishKey::NTR);
bool initKey2();
}
}
