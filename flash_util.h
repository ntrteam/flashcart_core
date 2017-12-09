#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace flashcart_core {

template<
            typename FlashcartClass, 
            unsigned int readSizePower,
            /// Reads a page from the flash at address `addr`.
            ///
            /// `size` is guaranteed to always be `(1 << readSizePower)`, if `readSizePower`
            /// is not 0. There are no alignment guarantees for `addr`.
            bool (FlashcartClass::*readFn)(std::uint32_t addr, std::uint32_t size, void *dest),
            unsigned int eraseSizePower,
            /// Erases a `(1 << eraseSizePower)`-byte page at address `addr`.
            ///
            /// `addr` is guaranteed to be aligned to `(1 << eraseSizePower)` bytes.
            bool (FlashcartClass::*eraseFn)(std::uint32_t addr),
            unsigned int writeSizePower,
            /// Writes a `(1 << writeSizePower)`-byte page at address `addr`.
            ///
            /// `addr` is guaranteed to be aligned to `(1 << writeSizePower)` bytes.
            bool (FlashcartClass::*writeFn)(std::uint32_t addr, const void *src)
        >
class FlashUtil {
    static constexpr std::uint32_t readSize = (1 << readSizePower);
    static constexpr std::uint32_t eraseSize = (1 << eraseSizePower);
    static constexpr std::uint32_t eraseSizeM1 = eraseSize - 1;
    static constexpr std::uint32_t writeSize = (1 << writeSizePower);

    static_assert(eraseSizePower >= writeSizePower, "Erase page size must be at least write page size");

    /// Writes a `(1 << eraseSizePower)`-byte page at address `dest_address`.
    static bool writeHelper(FlashcartClass *const fc, const std::uint32_t dest_address, const std::uint8_t *const src) {
        std::uint32_t cur = 0;

        while (cur < eraseSize) {
            if (!(fc->*writeFn)(dest_address + cur, src + cur)) {
                return false;
            }

            // invariant: eraseSize % writeSize == 0
            cur += writeSize;
        }

        return cur == eraseSize;
    }

public:
    static bool read(FlashcartClass *const fc, 
                     const std::uint32_t start_address, const std::uint32_t length, void *const destVoid,
                     const bool progress = false, const char *const progress_str = "Reading flash") {
        constexpr bool freeReadSize = readSize == 1;
        constexpr std::uint32_t blockSize = freeReadSize ? 0x1000 : readSize;
        std::uint8_t *const dest = static_cast<std::uint8_t *>(destVoid);
        std::uint32_t cur = 0;

        // special case for small reads if we can read any size, and
        // and we're not showing the progress bar
        // just read the whole thing in one shot
        if (freeReadSize && !progress) {
            return (fc->*readFn)(start_address, length, dest);
        }
        
        if (progress) {
            platform::showProgress(cur, length, progress_str);
        }

        while (cur < length) {
            const std::uint32_t cur_blockSize = std::min<std::uint32_t>(blockSize, length - cur);
            const bool oddBlock = cur_blockSize != blockSize && !freeReadSize;

            std::uint8_t *const cur_dest = oddBlock ? static_cast<std::uint8_t *>(std::malloc(blockSize)) : dest + cur;
            if (!cur_dest) {
                platform::logMessage(LOG_ERR, "FlashUtil::read: malloc failed");
                return false;
            }

            if (!(fc->*readFn)(start_address + cur, freeReadSize ? cur_blockSize : blockSize, cur_dest)) {
                if (oddBlock) {
                    std::free(cur_dest);
                }
                return false;
            }

            if (oddBlock) {
                std::memcpy(dest + cur, cur_dest, cur_blockSize);
                std::free(cur_dest);
            }
            
            cur += cur_blockSize;

            if (progress) {
                platform::showProgress(cur, length, progress_str);
            }
        }

        return cur == length;
    }

    static bool write(FlashcartClass *const fc,
                      const std::uint32_t dest_address, const std::uint32_t length, const void *const srcVoid,
                      bool progress = false, const char *const progress_str = "Writing flash") {
        const std::uint32_t real_start = dest_address & ~eraseSizeM1;
        const std::uint32_t first_page_offset = dest_address & eraseSizeM1;
        const std::uint32_t real_length = ((length + first_page_offset) + eraseSizeM1) & ~eraseSizeM1;
        const std::uint8_t *const src = static_cast<const std::uint8_t *>(srcVoid);
        std::uint32_t cur = 0;
        std::uint8_t *buf = static_cast<std::uint8_t *>(std::malloc(eraseSize));
        if (!buf) {
            platform::logMessage(LOG_ERR, "FlashUtil::write: malloc failed");
            return false;
        }

        if (progress) {
            platform::showProgress(cur, real_length, progress_str);
        }

        while (cur < real_length) {
            const std::uint32_t cur_addr = real_start + cur;
            const std::uint32_t buf_ofs = ((dest_address > cur_addr) ? (dest_address - cur_addr) : 0);
            const std::uint32_t src_ofs = ((cur > first_page_offset) ? (cur - first_page_offset) : 0);
            const std::uint32_t len = std::min<std::uint32_t>(eraseSize - buf_ofs, std::min<std::uint32_t>(length - src_ofs, std::uint32_t(eraseSize)));

            if (!read(fc, cur_addr, eraseSize, buf)) {
                platform::logMessage(LOG_ERR, "FlashUtil::write: read failed");
                goto fail;
            }

            if (std::memcmp(buf + buf_ofs, src + src_ofs, len)) {
                if (!(fc->*eraseFn)(cur_addr)) {
                    platform::logMessage(LOG_ERR, "FlashUtil::write: erase failed");
                    goto fail;
                }

                std::memcpy(buf + buf_ofs, src + src_ofs, len);
                writeHelper(fc, cur_addr, buf);
            }

            cur += eraseSize;
            if (progress) {
                platform::showProgress(cur, real_length, progress_str);
            }
        }

        std::free(buf);
        buf = static_cast<uint8_t *>(std::malloc(length));
        if (buf) {
            if (!read(fc, dest_address, length, buf)
                || std::memcmp(buf, src, length)) {
                platform::logMessage(LOG_NOTICE, "Flash write verification failed");
                goto fail;
            }
        } else {
            std::uint32_t t;
            if (!read(fc, dest_address, 4, &t)
                || std::memcmp(&t, src, std::min<std::uint32_t>(length, 4))) {
                platform::logMessage(LOG_NOTICE, "Flash write start verification failed");
                goto fail;
            }

            if (length > 4) {
                if (!read(fc, dest_address + length - 4, 4, &t)
                    || std::memcmp(&t, src + length - 4, 4)) {
                    platform::logMessage(LOG_NOTICE, "Flash write start verification failed");
                    goto fail;
                }
            }
        }

        if (buf) {
            std::free(buf);
        }
        return true;
    fail:
        if (buf) {
            std::free(buf);
        }
        return false;
    }
};
}
