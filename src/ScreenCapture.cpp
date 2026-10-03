#include "ScreenCapture.hpp"
#include "SdCardManager.hpp"
#include <Preferences.h>
#include <time.h>
#include <algorithm>

namespace {
constexpr const char* ShadowPath = SYSTEM_FOLDER "/.screen.rgb565";
constexpr const char* ScreenshotsPath = GALLERY_FOLDER "/ScreenShots";
ScreenCaptureDisplay* captureDisplay = nullptr;

bool ensureDirectory(const char* path) {
    if (!SD.exists(path)) return SD.mkdir(path);
    File directory = SD.open(path, FILE_READ);
    return directory && directory.isDirectory();
}

void little16(uint8_t* target, uint16_t value) {
    target[0] = value; target[1] = value >> 8;
}
void little32(uint8_t* target, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) target[i] = value >> (8 * i);
}
} // namespace

ScreenCaptureDisplay::ScreenCaptureDisplay(Arduino_GFX& output)
    : Arduino_GFX(Width, Height), output(output),
      mutex(xSemaphoreCreateRecursiveMutexStatic(&mutexStorage)) {}

bool ScreenCaptureDisplay::begin(int32_t speed) {
    return output.begin(speed) && output.width() == Width && output.height() == Height;
}

void ScreenCaptureDisplay::startWrite() {
    xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
    // Do not leave an LCD SPI transaction open while accessing the SD shadow.
}

void ScreenCaptureDisplay::endWrite() {
    xSemaphoreGiveRecursive(mutex);
}

bool ScreenCaptureDisplay::beginCapture() {
    startWrite();
    captureReady = false;
    shadow.close();
    for (auto& entry : cache) { entry.firstRow = -1; entry.dirty = false; }
    if (SD.cardType() != CARD_NONE && ensureDirectory(MAIN_FOLDER) && ensureDirectory(SYSTEM_FOLDER)) {
        shadow = SD.open(ShadowPath, "w+");
        if (shadow) {
            memset(fileRow, 0, sizeof(fileRow));
            size_t left = Width * Height * sizeof(uint16_t);
            while (left) {
                size_t count = std::min(left, sizeof(fileRow));
                if (shadow.write(fileRow, count) != count) break;
                left -= count;
                delay(1);
            }
            shadow.flush();
            captureReady = left == 0;
        }
    }
    // Initialize the physical display and shadow to the same complete frame.
    output.fillScreen(RGB565_BLACK);
    if (!captureReady) shadow.close();
    bool ready = captureReady;
    endWrite();
    return ready;
}

bool ScreenCaptureDisplay::flushCache(Cache& entry) {
    if (!entry.dirty || entry.firstRow < 0) return true;
    const size_t rows = std::min(RowsPerCache, Height - entry.firstRow);
    const size_t bytes = rows * Width * sizeof(uint16_t);
    if (!shadow.seek(entry.firstRow * Width * sizeof(uint16_t)) ||
        shadow.write(reinterpret_cast<const uint8_t*>(entry.pixels), bytes) != bytes) {
        captureReady = false;
        return false;
    }
    entry.dirty = false;
    return true;
}

ScreenCaptureDisplay::Cache* ScreenCaptureDisplay::rowCache(int y) {
    if (!captureReady) return nullptr;
    int firstRow = y / RowsPerCache * RowsPerCache;
    Cache* selected = &cache[0];
    for (auto& entry : cache) {
        if (entry.firstRow == firstRow) { entry.used = ++usage; return &entry; }
        if (entry.firstRow < 0 || entry.used < selected->used) selected = &entry;
    }
    if (!flushCache(*selected)) return nullptr;
    size_t bytes = std::min(RowsPerCache, Height - firstRow) * Width * sizeof(uint16_t);
    if (!shadow.seek(firstRow * Width * sizeof(uint16_t)) ||
        shadow.read(reinterpret_cast<uint8_t*>(selected->pixels), bytes) != bytes) {
        captureReady = false;
        return nullptr;
    }
    selected->firstRow = firstRow;
    selected->used = ++usage;
    selected->dirty = false;
    return selected;
}

void ScreenCaptureDisplay::mirrorRow(int x, int y, int count, const uint16_t* pixels, uint16_t color) {
    Cache* entry = rowCache(y);
    if (!entry) return;
    uint16_t* destination = entry->pixels + (y - entry->firstRow) * Width + x;
    if (pixels) memcpy(destination, pixels, count * sizeof(uint16_t));
    else std::fill_n(destination, count, color);
    entry->dirty = true;
}

void ScreenCaptureDisplay::writePixelPreclipped(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= Width || y >= Height) return;
    startWrite();
    output.drawPixel(x, y, color);
    mirrorRow(x, y, 1, nullptr, color);
    endWrite();
}

void ScreenCaptureDisplay::writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    int left = std::max(0, int(x)), top = std::max(0, int(y));
    int right = std::min(Width, int(x) + w), bottom = std::min(Height, int(y) + h);
    if (left >= right || top >= bottom) return;
    startWrite();
    output.fillRect(left, top, right - left, bottom - top, color);
    for (int row = top; row < bottom; ++row) mirrorRow(left, row, right - left, nullptr, color);
    endWrite();
}

void ScreenCaptureDisplay::draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* pixels, int16_t w, int16_t h) {
    draw16bitRGBBitmap(x, y, static_cast<const uint16_t*>(pixels), w, h);
}

void ScreenCaptureDisplay::draw16bitRGBBitmap(int16_t x, int16_t y, const uint16_t* pixels, int16_t w, int16_t h) {
    if (!pixels || w <= 0 || h <= 0) return;
    int left = std::max(0, int(x)), top = std::max(0, int(y));
    int right = std::min(Width, int(x) + w), bottom = std::min(Height, int(y) + h);
    if (left >= right || top >= bottom) return;
    startWrite();
    for (int row = top; row < bottom; ++row) {
        const uint16_t* source = pixels + (row - y) * w + left - x;
        output.draw16bitRGBBitmap(left, row, source, right - left, 1);
        mirrorRow(left, row, right - left, source, 0);
    }
    endWrite();
}

ScreenshotResult ScreenCaptureDisplay::screenshot() {
    ScreenshotResult result;
    if (xSemaphoreTakeRecursive(mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        result.error = "Display is busy; try again.";
        return result;
    }
    // Keep drawing blocked until the last row is written: no second frame buffer.
    auto capture = [&]() -> const char* {
        if (!captureReady || !shadow) return "Screen capture is unavailable (SD shadow not initialized or an SD error occurred).";
        for (auto& entry : cache) if (!flushCache(entry)) return "Could not flush the screen shadow.";
        shadow.flush();
        if (!ensureDirectory(GALLERY_FOLDER) || !ensureDirectory(ScreenshotsPath))
            return "Could not create Gallery/ScreenShots.";

        time_t now = time(nullptr);
        tm utc{};
        if (!gmtime_r(&now, &utc)) return "Could not read the system clock.";
        char stamp[32];
        if (!strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%SZ", &utc)) return "Invalid system time.";
        const char* prefix = utc.tm_year >= 120 ? "" : "unsynced_";

        Preferences preferences;
        if (!preferences.begin("screenshots", false)) return "Could not open screenshot counter storage.";
        uint64_t counter = preferences.getULong64("counter", 0);
        char partial[176];
        do {
            if (counter == UINT64_MAX) return "Screenshot counter exhausted.";
            ++counter;
            snprintf(result.path, sizeof(result.path), "%s/ScreenShot_%s%s_%010llu.bmp",
                     ScreenshotsPath, prefix, stamp, static_cast<unsigned long long>(counter));
            snprintf(partial, sizeof(partial), "%s.part", result.path);
        } while (SD.exists(result.path) || SD.exists(partial));
        // Persist before creating the file; failed/interrupted captures may leave gaps.
        if (preferences.putULong64("counter", counter) != sizeof(counter))
            return "Could not persist screenshot counter.";
        preferences.end();

        File destination = SD.open(partial, "w");
        if (!destination) return "Could not create screenshot file.";
        constexpr uint32_t stride = (Width * 3 + 3) & ~3U;
        constexpr uint32_t fileSize = 54 + stride * Height;
        static_assert(stride == sizeof(fileRow), "BMP row buffer must include padding");
        uint8_t header[54]{};
        header[0] = 'B'; header[1] = 'M';
        little32(header + 2, fileSize);
        little32(header + 10, sizeof(header));
        little32(header + 14, 40);
        little32(header + 18, Width);
        little32(header + 22, Height); // Positive height: bottom row first.
        little16(header + 26, 1);
        little16(header + 28, 24);
        little32(header + 34, stride * Height);
        bool ok = destination.write(header, sizeof(header)) == sizeof(header);
        for (int y = Height - 1; y >= 0 && ok; --y) {
            Cache* entry = rowCache(y);
            if (!entry) { ok = false; break; }
            const uint16_t* row = entry->pixels + (y - entry->firstRow) * Width;
            for (int x = 0; x < Width; ++x) {
                uint16_t color = row[x];
                uint8_t r = (color >> 11) & 31, g = (color >> 5) & 63, b = color & 31;
                fileRow[x * 3] = (b << 3) | (b >> 2);
                fileRow[x * 3 + 1] = (g << 2) | (g >> 4);
                fileRow[x * 3 + 2] = (r << 3) | (r >> 2);
            }
            ok = destination.write(fileRow, stride) == stride;
            delay(1);
        }
        destination.flush();
        ok = ok && destination.size() == fileSize && !destination.getWriteError();
        destination.close();
        if (!ok) { SD.remove(partial); return "Screenshot write failed (SD full or unavailable)."; }
        if (!SD.rename(partial, result.path)) { SD.remove(partial); return "Could not finalize screenshot file."; }
        return nullptr;
    };
    result.error = capture();
    result.ok = result.error == nullptr;
    endWrite();
    return result;
}

void setScreenshotDisplay(ScreenCaptureDisplay* display) { captureDisplay = display; }

ScreenshotResult takeScreenshot() {
    if (captureDisplay) return captureDisplay->screenshot();
    ScreenshotResult result;
    result.error = "Screenshot display is not initialized.";
    return result;
}
