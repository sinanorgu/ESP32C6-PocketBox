#pragma once

#include <cstddef>
#include <cstdint>

namespace gallery {
constexpr uint32_t MaxFileBytes = 2 * 1024 * 1024;
constexpr uint32_t MaxDimension = 1024;
constexpr uint32_t MaxPixels = 1024 * 1024;
constexpr size_t HeapReserve = 48 * 1024;
constexpr uint32_t DecodeTimeoutMs = 10000;

inline bool validDimensions(uint32_t width, uint32_t height) {
    return width > 0 && height > 0 && width <= MaxDimension &&
           height <= MaxDimension && uint64_t(width) * height <= MaxPixels;
}

struct Size { int width; int height; };

// Fit without stretching or enlarging; all arithmetic is bounded by the policy.
inline Size fit(int width, int height, int areaWidth, int areaHeight) {
    if (width <= 0 || height <= 0 || areaWidth <= 0 || areaHeight <= 0) return {0, 0};
    if (width <= areaWidth && height <= areaHeight) return {width, height};
    if (int64_t(width) * areaHeight > int64_t(height) * areaWidth) {
        int h = int(int64_t(height) * areaWidth / width);
        return {areaWidth, h > 0 ? h : 1};
    }
    int w = int(int64_t(width) * areaHeight / height);
    return {w > 0 ? w : 1, areaHeight};
}

// First destination pixel whose nearest-neighbour sample lies at/after source.
inline int boundary(int source, int sourceSize, int destinationSize) {
    return int((int64_t(source) * destinationSize + sourceSize - 1) / sourceSize);
}

inline uint16_t le16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

// Bound the decoder's pitch arithmetic before it parses an untrusted IHDR.
inline const char* checkPngHeader(const uint8_t* h, size_t size) {
    if (size < 33 || be32(h + 8) != 13 || be32(h + 12) != 0x49484452U)
        return "Invalid PNG header.";
    if (!validDimensions(be32(h + 16), be32(h + 20)))
        return "Image too large. Maximum: 1024 x 1024.";
    if (h[28] != 0) return "Interlaced PNG is not supported.";
    if (h[24] == 16) return "16-bit PNG is not supported. Use 8-bit channels.";
    const int depth = h[24], type = h[25];
    const bool packed = depth == 1 || depth == 2 || depth == 4 || depth == 8;
    const bool valid = ((type == 0 || type == 3) && packed) ||
                       ((type == 2 || type == 4 || type == 6) && depth == 8);
    if (!valid || h[26] != 0 || h[27] != 0) return "Unsupported PNG encoding.";
    return nullptr;
}

struct BmpInfo {
    uint32_t width, height, offset, stride;
    uint8_t bytesPerPixel;
    bool topDown;
};

// Only uncompressed Windows RGB BMP (24/32 bit); validate before any pixel I/O.
inline bool parseBmp(const uint8_t* h, size_t headerSize, uint32_t fileSize, BmpInfo& out) {
    if (headerSize < 54 || h[0] != 'B' || h[1] != 'M' || fileSize > MaxFileBytes) return false;
    uint32_t dib = le32(h + 14), width = le32(h + 18), rawHeight = le32(h + 22);
    bool topDown = (rawHeight & 0x80000000U) != 0;
    uint32_t height = topDown ? (~rawHeight + 1U) : rawHeight;
    uint16_t bits = le16(h + 28);
    uint32_t offset = le32(h + 10);
    if (dib < 40 || uint64_t(dib) + 14 > offset || offset > fileSize ||
        !validDimensions(width, height) || le16(h + 26) != 1 ||
        (bits != 24 && bits != 32) || le32(h + 30) != 0) return false;
    uint32_t stride = (width * (bits / 8) + 3) & ~3U;
    if (uint64_t(offset) + uint64_t(stride) * height > fileSize) return false;
    out = {width, height, offset, stride, uint8_t(bits / 8), topDown};
    return true;
}
} // namespace gallery
