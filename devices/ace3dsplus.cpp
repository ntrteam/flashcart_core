#ifdef TESTING

#include <cstring>

#include <ncgcpp/ntrcard.h>

#include "../device.h"
#include "../flash_util.h"

namespace flashcart_core {
using platform::logMessage;
using platform::showProgress;

class Ace3DSPlus : Flashcart {
    /// Gets the cart version (in the high halfword) and status (in the low byte).
    bool cmdVersionStatus(uint32_t *resp) {
        ncgc::Err r = m_card->sendCommand(0xB0, resp, 4, 0x180000);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdVersionStatus failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    /// Sets some SD-related register on the card (?)
    bool cmdSdRegister(uint8_t param) {
        ncgc::Err r = m_card->sendCommand(0xC2ull | (((uint64_t) param) << 32), NULL, 0, 0x180000);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdSdRegister failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    bool cmdSdRaw(uint8_t x, uint32_t y, uint8_t z) {
        union {
            uint8_t u8[8];
            uint64_t u64;
        } cmd;
        cmd.u8[0] = 0xC0;
        cmd.u8[1] = x;
        cmd.u8[2] = (y & 0xFF000000) >> 24;
        cmd.u8[3] = (y & 0xFF0000) >> 16;
        cmd.u8[4] = (y & 0xFF00) >> 8;
        cmd.u8[5] = y & 0xFF;
        cmd.u8[6] = z;
        cmd.u8[7] = 0;

        ncgc::Err r = m_card->sendCommand(cmd.u64, nullptr, 0, 0x180000);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdSdRaw failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    /// Sends a raw SD command, then polls for the result.
    bool cmdSd(uint8_t x, uint32_t y, uint8_t z, void *resp) {
        uint32_t tr, prev_tr, timeout = 5;

        do {
            if (!cmdSdRaw(x, y, z)) { return false; }

            prev_tr = 4;
            while (1) {
                if (!cmdVersionStatus(&tr)) { return false; }
                if (!(tr & 4) && tr == prev_tr) { break; }
                prev_tr = tr;
            }

            if (!(z & 8)) {
                return true;
            }

            prev_tr = 8;
            while (1) {
                if (!cmdVersionStatus(&tr)) { return false; }
                if (!(tr & 8) && tr == prev_tr) { break; }
                prev_tr = tr;
            }

            if (!(prev_tr & 0x20)) {
                if (!cmdReadSdBufferPlain(resp)) { return false; }
                return true;
            }
        } while (--timeout);

        logMessage(LOG_ERR, "Ace3DSPlus: cmdSd failed");
        return false;
    }

    /// Reads a sector off the card's SD into (probably) some memory on the card,
    /// which we'll call the card SD buffer.
    bool cmdSdReadSector() {
        // this is supposed to take an address, but it doesn't matter for us
        ncgc::Err r;
        uint32_t resp = 1;
        do {
            if ((r = m_card->sendCommand(0xB9, &resp, 4, 0x180000))) {
                logMessage(LOG_ERR, "Ace3DSPlus: cmdSdReadSector failed: %d", r.errNo());
                return false;
            }
        } while (resp);
        return true;
    }


    /// Read the card's SD buffer.
    ///
    /// We don't care about the result.
    bool cmdReadSdBufferPlain(void *resp = nullptr) {
        ncgc::Err r = m_card->sendCommand(0xBA, resp, 0x200, 0x180000);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdReadSdBufferPlain failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    /// Read the card's SD buffer. The card does R4-style crypto on the buffer
    /// before it is transferred over the NTR parallel port
    ///
    /// We don't care about the result.
    bool cmdReadSdBufferCrypted() {
        ncgc::Err r = m_card->sendCommand(0xBF, nullptr, 0x200, 0x180000);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdReadSdBufferCrypted failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    bool cmdEnableFlash() {
        uint32_t bufu32[0x200/4];
        uint8_t *buf = reinterpret_cast<uint8_t *>(bufu32);
        ncgc::Err r = m_card->sendCommand(0xC6, buf, 0x200, 0x180000);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdEnableFlash 0xC6 failed: %d", r.errNo());
            return false;
        }

        /*
            result = 0;
            v3 = v1 + 512;
            do {
                v4 = *v1++ & 1;
                result = 2 * result | v4;
            } while ( v1 != v3 );
        */
        // they loop over the whole 512 bytes but..
        // an int's only 32 bits????????
        // TODO verify this
        uint32_t weird_sum = 0;
        for (int i = 0; i < 32; ++i) {
            if (buf[i + (0x200 - 32)] & 1) {
                weird_sum |= (1 << (31 - i));
            }
        }

        for (int i = 0; i < 0x200/4; ++i) {
            if (i & 1) {
                bufu32[i] <<= 1;
            } else {
                bufu32[i] >>= 1;
            }
        }

        for (int i = 0; i < 0x200; ++i) {
            uint32_t sum_lsl1 = weird_sum << 1;
            if ((sum_lsl1 ^ weird_sum) & 0x8000) {
                sum_lsl1 |= 1;
            }
            buf[i] = (sum_lsl1 & 0x8000) ? (buf[i] | 1) : (buf[i] & ~1);
            weird_sum = sum_lsl1;
        }

        if((r = m_card->sendWriteCommand(0xC3FF3CA5AA555AC7, buf, 0x200, 0))) {
            logMessage(LOG_ERR, "Ace3DSPlus: cmdEnableFlash 0xC7 failed: %d", r.errNo());
            return false;
        }

        return true;
    }

    bool spiRdid(uint32_t *rdid) {
        static const uint8_t cmd[] = { 0x9F };
        ncgc::Err r = m_card->sendSpi(cmd, 1, reinterpret_cast<uint8_t *>(rdid), 3);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: spiRdid failed: %d", r.errNo());
            return false;
        }
        *rdid &= 0xFFFFFF;
        logMessage(LOG_INFO, "Ace3DSPlus: spiRdid: %06lX", *rdid);
        return true;
    }

    bool spiRead(const uint32_t address, const uint32_t size, void *const buf) {
        uint8_t cmd[] = { 3, 0, 0, 0 };
        cmd[1] = (address & 0xFF0000) >> 16;
        cmd[2] = (address & 0xFF00) >> 8;
        cmd[3] = address & 0xFF;

        ncgc::Err r = m_card->sendSpi(cmd, 4, reinterpret_cast<uint8_t *>(buf), size);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: spiRead failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    bool spiWriteEnable() {
        static const uint8_t cmd[] = { 0x6 };
        ncgc::Err r = m_card->sendSpi(cmd, 1, nullptr, 0);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: spiWriteEnable failed: %d", r.errNo());
            return false;
        }
        return true;
    }

    bool spiWaitWrite() {
        static const uint8_t rdsr[] = { 0x5 };
        uint8_t sr = 1;
        do {
            ncgc::Err r = m_card->sendSpi(rdsr, 1, &sr, 1);
            if (r) {
                logMessage(LOG_ERR, "Ace3DSPlus: spiWaitWrite failed: %d", r.errNo());
                return false;
            }
        } while (sr & 1);

        return true;
    }

    bool spiSectorErase(uint32_t address) {
        uint8_t cmd[] = { 0x20, 0, 0, 0 };
        cmd[1] = (address & 0xFF0000) >> 16;
        cmd[2] = (address & 0xFF00) >> 8;
        cmd[3] = address & 0xFF;

        ncgc::Err r = m_card->sendSpi(cmd, 4, nullptr, 0);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: spiSectorErase failed: %d", r.errNo());
            return false;
        }

        return true;
    }

    bool spiPageProgram(uint32_t address, const void *src) {
        uint8_t cmd[256 + 4] = { 0 };
        cmd[0] = 2;
        cmd[1] = (address & 0xFF0000) >> 16;
        cmd[2] = (address & 0xFF00) >> 8;
        cmd[3] = address & 0xFF;
        std::memcpy(cmd + 4, src, 256);

        ncgc::Err r = m_card->sendSpi(cmd, sizeof(cmd), nullptr, 0);
        if (r) {
            logMessage(LOG_ERR, "Ace3DSPlus: spiPageProgram failed: %d", r.errNo());
            return false;
        }

        return true;
    }

    bool flashUtilErase(std::uint32_t addr) {
        return spiWriteEnable()
            && spiSectorErase(addr)
            && spiWaitWrite();
    }

    bool flashUtilPageProgram(std::uint32_t addr, const void *src) {
        return spiWriteEnable()
            && spiPageProgram(addr, src)
            && spiWaitWrite();
    }

    bool tryBlowfishKey(BlowfishKey key) {
        ncgc::Err err = m_card->init();
        if (err && !err.unsupported()) {
            logMessage(LOG_ERR, "Ace3DSPlus: tryBlowfishKey: ntrcard init failed");
            return false;
        } else if (m_card->state() != ncgc::NTRState::Raw) {
            logMessage(LOG_ERR, "Ace3DSPlus: tryBlowfishKey: status (%d) not RAW and cannot reset",
                static_cast<uint32_t>(m_card->state()));
            return false;
        }

        ncgc::c::ncgc_ncard_t& state = m_card->rawState();
        state.hdr.key1_romcnt = state.key1.romcnt = 0x1808F8;
        state.hdr.key2_romcnt = state.key2.romcnt = 0x416017;
        state.key2.seed_byte = 0;
        m_card->setBlowfishState(platform::getBlowfishKey(key), key != BlowfishKey::NTR);

        if ((err = m_card->beginKey1())) {
            logMessage(LOG_ERR, "Ace3DSPlus: tryBlowfishKey: init key1 (key = %d) failed: %d", static_cast<int>(key), err.errNo());
            return false;
        }
        if ((err = m_card->beginKey2())) {
            logMessage(LOG_ERR, "Ace3DSPlus: tryBlowfishKey: init key2 failed: %d", err.errNo());
            return false;
        }

        return true;
    }

    bool cartSdInit() {
        uint8_t buf[0x200];
        if (!cmdSdRegister(0)
            || !cmdSd(0, 0, 4, buf)) {
            return false;
        }

        bool sdhc = cmdSd(8, 0x1C8, 0x1C, buf);
        uint32_t sd_thing = sdhc ? 0x40300000 : 0x300000;
        if (sdhc && (buf[3] != 1 || buf[4] != 0xC8)) {
            logMessage(LOG_ERR, "Ace3DSPlus: cartSdInit: unexpected response from SDHC");
            return false;
        }

        int timeout = 0x200;
        do {
            if (!cmdSd(0x37, 0, 0x1C, buf)) {
                return false;
            }
            cmdSd(0x29, sd_thing, 0x1C, buf);

            if (!(buf[1] & 0x80)) {
                continue;
            }

            bool flag = !!(buf[1] & 0x40);
            cmdSd(2, 0, 0x3C, buf);
            cmdSd(3, 0, 0x1C, buf);
            cmdSdRegister(flag ? 3 : 1);

            sd_thing = (((uint32_t) buf[2]) << 16) | (((uint32_t) buf[1]) << 24);
            cmdSd(9, sd_thing, 0x3C, buf);
            if ((buf[6] & 0xF) <= 8) {
                // TODO FIXME: what does this mean? (small issue)
                logMessage(LOG_ERR, "Ace3DSPlus: cartSdInit: buf[6] & 0xF <= 8");
                return false;
            }

            cmdSd(7, sd_thing, 0x1C, buf);
            cmdSd(16, 0x200, 0x1C, buf);

            uint32_t resp, prev_resp = 0x40;
            int timeout2 = 0x100;
            do {
                if (!cmdVersionStatus(&resp)) { return false; }
                if (!(resp & 0x40) && resp == prev_resp) { break; }
                prev_resp = resp;
            } while (--timeout2);

            cmdSd(0x37, sd_thing, 0x1C, buf);
            cmdSd(6, 2, 0x1C, buf);
            logMessage(LOG_INFO, "Ace3DSPlus: cartSdInit: success");
            return true;
        } while (--timeout);

        logMessage(LOG_ERR, "Ace3DSPlus: SD init timed out");
        return false;
    }

    bool tryPollVersion() {
        int timeout = 0x10;
        uint32_t resp;
        do {
            if (!cmdVersionStatus(&resp)) {
                return false;
            }
        } while ((resp == 0 || resp == 0xFFFFFFFF) && --timeout);

        if (resp == 0 || resp == 0xFFFFFFFF) {
            logMessage(LOG_INFO, "Ace3DSPlus: 0xB0 still %08lX after timeout", resp);
            return false;
        }

        return true;
    }

    void aapReadData(uint32_t addr) {
        ncgc::Err err;
        if ((err = m_card->readData(addr, nullptr, 0x200))) {
            logMessage(LOG_INFO, "Ace3DSPlus: readData failed: %d", err.errNo());
        }
    }

    bool passAntiAntiPiracy() {
        // Deep Labyrinth flash
        aapReadData(0x10FE00);
        aapReadData(0x167400);
        if (tryPollVersion()) {
            return true;
        }

        // Spongebob flash
        aapReadData(0x18DE00);
        aapReadData(0x198C00);
        aapReadData(0x1A0C00);
        if (tryPollVersion()) {
            return true;
        }

        // Alex Rider flash
        aapReadData(0x16D400);
        aapReadData(0x6B200);
        if (tryPollVersion()) {
            return true;
        }

        // Metroid Prime Hunters flash
        aapReadData(0x1159400); // wtf
        aapReadData(0xB7400);
        if (tryPollVersion()) {
            return true;
        }

        logMessage(LOG_INFO, "Ace3DSPlus: known AAP sequences failed");
        // last ditch attempt (sweep the first 2M and see if it works)
        // (this works for the Deep Labyrinth flash)
        ncgc::Err err;
        if ((err = m_card->readData(0x8000, nullptr, 0x200000 - 0x8000))
            || (err = m_card->readData(0x8000, nullptr, 0x200000 - 0x8000))) {
            logMessage(LOG_INFO, "Ace3DSPlus: readData failed: %d", err.errNo());
        }
        return tryPollVersion();
    }

    using Util = FlashUtil<Ace3DSPlus, 0, &Ace3DSPlus::spiRead, 12, &Ace3DSPlus::flashUtilErase, 8, &Ace3DSPlus::flashUtilPageProgram>;

public:
    Ace3DSPlus() : Flashcart("Ace3DS Plus", 0x200000) { }

    const char* getAuthor() {
        return "ntrteam, et al.";
    }

    const char* getDescription() {
        return
            "Ace3DS Plus family. Includes:\n"
            " * Ace3DS Plus (ace3ds.com)\n"
            " * r4isdhc.com.cn\n"
            " * Certain Gateway Blue cards";
    }

    bool initialize() {
        uint32_t resp;
        ncgc::Err err;
        bool initFromRaw = m_card->state() != ncgc::NTRState::Key2;

        if (initFromRaw
            && !tryBlowfishKey(BlowfishKey::NTR)
            && !tryBlowfishKey(BlowfishKey::B9Retail)
            && !tryBlowfishKey(BlowfishKey::B9Dev)) {
            logMessage(LOG_ERR, "Ace3DSPlus: init from RAW fail");
            return false;
        }

        if (!cmdVersionStatus(&resp)) {
            return false;
        }

        if (resp == 0 || resp == 0xFFFFFFFF || initFromRaw) {
            if (!passAntiAntiPiracy()) {
                logMessage(LOG_ERR, "Failed to pass Ace3DS anti-antipiracy");
                return false;
            }

            if (!cmdVersionStatus(&resp)) {
                return false;
            }

            if (resp == 0 || resp == 0xFFFFFFFF) {
                logMessage(LOG_ERR, "0xB0 still returns %08lX", resp);
                return false;
            }
        }

        logMessage(LOG_INFO, "Ace3DSPlus version: %08lX", resp);

        uint32_t rdid;
        if (!spiRdid(&rdid)) {
            return false;
        }

        if (rdid == 0xFFFFFF || initFromRaw) {
            if (!cartSdInit()
                || !cmdSdReadSector() || !cmdReadSdBufferPlain()
                || !cmdSdReadSector() || !cmdReadSdBufferCrypted()
                || !spiRdid(&rdid)) {
                return false;
            }

            if (rdid == 0xFFFFFF) {
                logMessage(LOG_ERR, "Ace3DSPlus: RDID still %06lX after init", rdid);
                return false;
            }
        }

        if (!cmdEnableFlash() || !spiRdid(&rdid)) {
            return false;
        }

        switch ((rdid & 0xFF0000) >> 16) {
            case 0x14:
            case 0x15:
            case 0x16:
                break;
            default:
                logMessage(LOG_ERR, "Ace3DSPlus: unexpected flash capacity in RDID: %06lX", rdid);
                return false;
        }

        logMessage(LOG_INFO, "Ace3DSPlus RDID: %06lX", rdid);

        return true;
    }

    void shutdown() {}

    bool readFlash(uint32_t address, uint32_t length, uint8_t *buffer) {
        return Util::read(this, address, length, buffer, true);
    }

    bool writeFlash(uint32_t address, uint32_t length, const uint8_t *buffer) {
        return Util::write(this, address, length, buffer, true);
    }

    bool injectNtrBoot(uint8_t *blowfish_key, uint8_t *firm, uint32_t firm_size) {
        if (firm_size > 0x1F5200 /* 0x200000 - 0xAE00 */) {
            logMessage(LOG_NOTICE, "FIRM too big; maximum size is 2052608 bytes");
            return false;
        }

        void *configPage = std::malloc(0x9100);
        if (!configPage) {
            logMessage(LOG_ERR, "malloc failed");
            return false;
        }

        // map = struct.unpack("<8192H", flash[0:0x4000]) # python
        // 0x4000:0x8000 is the map for pre-"anti-anti-piracy" (AAP)
        // nor_address(rom_address) = (map[rom_address >> 12] << 12) + (rom_address & 0xFFF)
        uint16_t *configMap = static_cast<uint16_t *>(configPage);
        for (int i = 0; i < 8; ++i) {
            configMap[i] = 0xA;
        }
        for (int i = 0; i < (0x4000/2) - 8; ++i) {
            configMap[i+8] = 0xB+i;
        }
        std::memcpy(configMap + 0x2000, configMap, 0x2000*2);

        // grab the flash version info/hw rev/fw rev stuff off the flash
        if (!spiRead(0x9050, 0x40, static_cast<uint8_t *>(configPage) + 0x9050)) {
            logMessage(LOG_ERR, "Flash read failed");
            return false;
        }

        // 0x90C0:0x9100 contains some sort of configuration
        // it appears the two ints at 0x90C0 and 0x90C4 determine the B7 reads
        // that must occur before flashcart commands are enabled
        // it's not a raw address though, seems to be a bitfield with the address
        // in the middle
        uint32_t *configAAP = static_cast<uint32_t *>(configPage) + 0x90C0/4;
        for (int i = 0; i < 16; ++i) {
            // taken from Ace3DS X in ntrboot mode
            // this appears to disable the AAP
            configAAP[i] = 0xFFFFFFF0u;
        }

        uint8_t *configBfKey = static_cast<uint8_t *>(configPage) + 0x8000;
        std::memcpy(configBfKey, blowfish_key + 0x48, 0x1000);
        for (int i = 0; i < 0x12; ++i) {
            std::memcpy(configBfKey + 0x1000 + (0x11 - i)*4, blowfish_key + i*4, 4);
        }

        bool result = Util::write(this, 0, 0x9100, configPage, true, "Writing configuration")
            && Util::write(this, 0xAE00, firm_size, firm, true, "Writing FIRM");
        std::free(configPage);
        return result;
    }
};

Ace3DSPlus ace3DSplus;
}

#endif