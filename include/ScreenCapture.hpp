#pragma once

#include <Arduino_GFX_Library.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct ScreenshotResult {
    bool ok = false;
    char path[160]{};
    const char* error = nullptr;
};

// Immediate-mode display with an exact RGB565 shadow on SD, not a full RAM canvas.
// The landscape orientation is fixed, matching PocketBox's existing UI.
class ScreenCaptureDisplay : public Arduino_GFX {
public:
    explicit ScreenCaptureDisplay(Arduino_GFX& output);
    bool begin(int32_t speed = GFX_NOT_DEFINED) override;
    bool beginCapture(); // Call after SD initialization, before the first UI frame.
    ScreenshotResult screenshot();
    void startWrite() override;
    void endWrite() override;
    void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override;
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
        writeFillRectPreclipped(x, y, 1, h, color);
    }
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
        writeFillRectPreclipped(x, y, w, 1, color);
    }
    void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* pixels, int16_t w, int16_t h) override;
    void draw16bitRGBBitmap(int16_t x, int16_t y, const uint16_t* pixels, int16_t w, int16_t h) override;

private:
    static constexpr int Width = 320, Height = 172;
    static constexpr int RowsPerCache = 8, CacheCount = 3;
    struct Cache {
        uint16_t pixels[Width * RowsPerCache]{};
        int firstRow = -1;
        uint32_t used = 0;
        bool dirty = false;
    } cache[CacheCount];
    Arduino_GFX& output;
    File shadow;
    StaticSemaphore_t mutexStorage{};
    SemaphoreHandle_t mutex;
    bool captureReady = false;
    uint32_t usage = 0;
    uint8_t fileRow[Width * 3]{}; // Also used for shadow initialization.
    bool flushCache(Cache& entry);
    Cache* rowCache(int y);
    void mirrorRow(int x, int y, int count, const uint16_t* pixels, uint16_t fillColor);
};

void setScreenshotDisplay(ScreenCaptureDisplay* display);
ScreenshotResult takeScreenshot();
