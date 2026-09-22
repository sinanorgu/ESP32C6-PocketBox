#include "Application.hpp"
#include "System.hpp"
#include "ListMenu.hpp"
#include "GalleryImage.hpp"
#include <JPEGDEC.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>

namespace {
constexpr size_t PageSize = 60; // Leave room for navigation in ListMenu.
static_assert(PNG_MAX_BUFFERED_PIXELS >= 2 * (gallery::MaxDimension * 4 + 1) + 32,
              "PNG needs two RGBA rows plus alignment padding");

int contentY() {
    const auto& ui = System::getInstance().interface;
    return ui.infoPanelHeight + ui.margin;
}

void clearContent() {
    auto* gfx = System::getInstance().gfx;
    gfx->fillRect(0, contentY(), gfx->width(), gfx->height() - contentY(), RGB565_BLACK);
}

void message(const char* text) {
    auto* gfx = System::getInstance().gfx;
    clearContent();
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextWrap(true);
    gfx->setCursor(5, contentY() + 8);
    gfx->print(text);
    gfx->setCursor(5, gfx->height() - 12);
    gfx->print("Left: back");
}

// Consume the release too, so one press cannot cross two screens.
void releaseButton(int pin) {
    do {
        while (digitalRead(pin) == LOW) delay(10);
        delay(25);
    } while (digitalRead(pin) == LOW);
}

void waitBack() {
    while (digitalRead(BUTTON_LEFT_PIN) != LOW) delay(10);
    releaseButton(BUTTON_LEFT_PIN);
}

bool hasMemory(size_t bytes) {
    return heap_caps_get_free_size(MALLOC_CAP_8BIT) >= bytes + gallery::HeapReserve &&
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= bytes;
}

struct RenderContext {
    int areaTop = contentY();
    int sourceWidth = 0, sourceHeight = 0;
    gallery::Size size{0, 0};
    int x = 0, y = 0;
    PNG* png = nullptr;
    uint16_t sourceRow[gallery::MaxDimension];
    uint16_t outputRow[TFT_HEIGHT];
    uint8_t bmpRow[gallery::MaxDimension * 4];

    bool configure(int width, int height) {
        if (!gallery::validDimensions(width, height)) return false;
        auto* gfx = System::getInstance().gfx;
        if (gfx->width() > TFT_HEIGHT) return false;
        sourceWidth = width;
        sourceHeight = height;
        size = gallery::fit(width, height, gfx->width(), gfx->height() - areaTop);
        x = (gfx->width() - size.width) / 2;
        y = areaTop + (gfx->height() - areaTop - size.height) / 2;
        return size.width > 0 && size.height > 0;
    }

    void row(int sourceX, int sourceY, const uint16_t* pixels, int count) {
        if (sourceX < 0 || sourceY < 0 || sourceX >= sourceWidth || sourceY >= sourceHeight) return;
        if (count > sourceWidth - sourceX) count = sourceWidth - sourceX;
        int left = gallery::boundary(sourceX, sourceWidth, size.width);
        int right = gallery::boundary(sourceX + count, sourceWidth, size.width);
        int top = gallery::boundary(sourceY, sourceHeight, size.height);
        int bottom = gallery::boundary(sourceY + 1, sourceHeight, size.height);
        if (left >= right || top >= bottom) return;
        for (int dx = left; dx < right; ++dx) {
            outputRow[dx - left] = pixels[dx * sourceWidth / size.width - sourceX];
        }
        auto* gfx = System::getInstance().gfx;
        for (int dy = top; dy < bottom; ++dy) {
            gfx->draw16bitRGBBitmap(x + left, y + dy, outputRow, right - left, 1);
        }
    }
};

// Decoder callbacks are synchronous; only this application's viewer owns this file.
File imageFile;
uint32_t decodeStarted;
bool cancelled = false, timedOut = false, ioFailed = false;

bool keepDecoding() {
    delay(1); // Let networking and the watchdog run during decoding.
    cancelled |= digitalRead(BUTTON_LEFT_PIN) == LOW;
    timedOut |= uint32_t(millis() - decodeStarted) >= gallery::DecodeTimeoutMs;
    return !cancelled && !timedOut && !ioFailed;
}

template<typename DecoderFile>
int32_t readImage(DecoderFile*, uint8_t* data, int32_t length) {
    if (length <= 0 || !keepDecoding()) return 0;
    if (imageFile.position() > imageFile.size()) { ioFailed = true; return 0; }
    size_t remaining = imageFile.size() - imageFile.position();
    size_t wanted = size_t(length) < remaining ? size_t(length) : remaining;
    size_t got = imageFile.read(data, wanted);
    if (got != wanted) ioFailed = true;
    return int32_t(got);
}

template<typename DecoderFile>
int32_t seekImage(DecoderFile*, int32_t position) {
    if (position < 0 || uint32_t(position) > imageFile.size() ||
        !keepDecoding() || !imageFile.seek(position)) {
        ioFailed = true;
        return -1;
    }
    return position;
}

void closeImage(void*) {} // The viewer closes the file on every result path.
void* openPng(const char*, int32_t* size) {
    if (!imageFile.seek(0)) return nullptr;
    *size = imageFile.size();
    return &imageFile;
}

int drawJpeg(JPEGDRAW* block) {
    if (!keepDecoding()) return 0;
    auto& render = *static_cast<RenderContext*>(block->pUser);
    for (int y = 0; y < block->iHeight; ++y) {
        render.row(block->x, block->y + y, block->pPixels + y * block->iWidth,
                   block->iWidthUsed);
    }
    return 1;
}

int drawPng(PNGDRAW* line) {
    if (!keepDecoding()) return 0;
    auto& render = *static_cast<RenderContext*>(line->pUser);
    if (line->iWidth != render.sourceWidth) return 0;
    if (gallery::boundary(line->y, render.sourceHeight, render.size.height) ==
        gallery::boundary(line->y + 1, render.sourceHeight, render.size.height)) return 1;
    render.png->getLineAsRGB565(line, render.sourceRow, PNG_RGB565_LITTLE_ENDIAN, 0);
    render.row(0, line->y, render.sourceRow, line->iWidth);
    return 1;
}

const char* decodeJpeg(RenderContext& render) {
    if (!hasMemory(sizeof(JPEGDEC))) return "Not enough free memory.";
    std::unique_ptr<JPEGDEC> decoder(new (std::nothrow) JPEGDEC());
    if (!decoder) return "Not enough free memory.";
    if (!decoder->open(&imageFile, imageFile.size(), closeImage,
                       readImage<JPEGFILE>, seekImage<JPEGFILE>, drawJpeg)) {
        return "Invalid or unsupported JPEG.";
    }
    const char* error = nullptr;
    if (decoder->getJPEGType() != JPEG_MODE_BASELINE) {
        error = "Progressive JPEG is not supported.";
    } else if (!render.configure(decoder->getWidth(), decoder->getHeight())) {
        error = "Image too large. Maximum: 1024 x 1024.";
    } else {
        decoder->setUserPointer(&render);
        decoder->setPixelType(RGB565_LITTLE_ENDIAN);
        if (!decoder->decode(0, 0, 0)) error = "JPEG could not be decoded.";
    }
    decoder->close();
    return error;
}

const char* decodePng(RenderContext& render) {
    if (!hasMemory(sizeof(PNG))) return "Not enough free memory.";
    std::unique_ptr<PNG> decoder(new (std::nothrow) PNG());
    if (!decoder) return "Not enough free memory.";
    if (decoder->open("", openPng, closeImage, readImage<PNGFILE>, seekImage<PNGFILE>, drawPng) != PNG_SUCCESS) {
        return "Invalid or unsupported PNG.";
    }
    const char* error = nullptr;
    if (!render.configure(decoder->getWidth(), decoder->getHeight())) {
        error = "Image too large. Maximum: 1024 x 1024.";
    } else if (decoder->isInterlaced()) {
        error = "Interlaced PNG is not supported.";
    } else if (decoder->getBpp() > 8) {
        error = "16-bit PNG is not supported. Use 8-bit channels.";
    } else {
        render.png = decoder.get();
        if (decoder->decode(&render, PNG_CHECK_CRC) != PNG_SUCCESS) error = "PNG could not be decoded.";
        render.png = nullptr;
    }
    decoder->close();
    return error;
}

const char* decodeBmp(RenderContext& render, const uint8_t* header, size_t headerSize) {
    gallery::BmpInfo info;
    if (!gallery::parseBmp(header, headerSize, imageFile.size(), info)) {
        return "Invalid/oversized BMP. Use uncompressed 24/32-bit RGB.";
    }
    if (!render.configure(info.width, info.height)) return "Unsupported image dimensions.";
    // Read only rows needed by the display, never allocate the full image.
    for (int dy = 0; dy < render.size.height; ++dy) {
        if (!keepDecoding()) return "Image loading stopped.";
        uint32_t sy = dy * info.height / render.size.height;
        uint32_t fileRow = info.topDown ? sy : info.height - 1 - sy;
        if (!imageFile.seek(info.offset + fileRow * info.stride) ||
            imageFile.read(render.bmpRow, info.stride) != info.stride) return "SD read failed or BMP is truncated.";
        for (uint32_t x = 0; x < info.width; ++x) {
            const uint8_t* p = render.bmpRow + x * info.bytesPerPixel;
            render.sourceRow[x] = ((p[2] & 0xf8) << 8) | ((p[1] & 0xfc) << 3) | (p[0] >> 3);
        }
        render.row(0, sy, render.sourceRow, info.width);
    }
    return nullptr;
}

const char* loadImage(const String& path, bool fullscreen) {
    imageFile = SD.open(path, FILE_READ);
    if (!imageFile || imageFile.isDirectory()) return "File could not be opened.";
    if (imageFile.size() > gallery::MaxFileBytes) return "File too large. Maximum: 2 MiB.";
    uint8_t header[54] = {};
    size_t count = imageFile.read(header, sizeof(header));
    if (count < 8 || !imageFile.seek(0)) return "Invalid or unreadable image.";
    if (!hasMemory(sizeof(RenderContext))) return "Not enough free memory.";
    std::unique_ptr<RenderContext> render(new (std::nothrow) RenderContext());
    if (!render) return "Not enough free memory.";
    render->areaTop = fullscreen ? 0 : contentY();
    if (header[0] == 0xff && header[1] == 0xd8) return decodeJpeg(*render);
    const uint8_t pngSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (memcmp(header, pngSignature, sizeof(pngSignature)) == 0) {
        const char* error = gallery::checkPngHeader(header, count);
        return error ? error : decodePng(*render);
    }
    if (header[0] == 'B' && header[1] == 'M') return decodeBmp(*render, header, count);
    return "Unsupported format. Use JPEG, PNG or BMP.";
}

String galleryPath(File& file) {
    String name = file.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    return String(GALLERY_FOLDER) + "/" + name;
}

bool isImage(File& file) {
    uint8_t header[8] = {};
    if (file.read(header, sizeof(header)) != sizeof(header)) return false;
    const uint8_t png[] = {137, 80, 78, 71, 13, 10, 26, 10};
    return (header[0] == 0xff && header[1] == 0xd8) ||
           (header[0] == 'B' && header[1] == 'M') ||
           memcmp(header, png, sizeof(png)) == 0;
}

enum class NeighborResult { Found, Boundary, Cancelled, Error };

// Walk directory order across list pages, retaining only one candidate path.
NeighborResult findNeighbor(const String& current, bool forward, String& result) {
    File directory = SD.open(GALLERY_FOLDER);
    if (!directory || !directory.isDirectory()) return NeighborResult::Error;
    bool foundCurrent = false;
    String previous;
    const uint32_t started = millis();
    while (true) {
        delay(1);
        if (digitalRead(BUTTON_LEFT_PIN) == LOW) return NeighborResult::Cancelled;
        if (uint32_t(millis() - started) >= gallery::DecodeTimeoutMs || !hasMemory(2048))
            return NeighborResult::Error;
        File file = directory.openNextFile();
        if (!file) break;
        if (file.isDirectory()) continue;
        String path = galleryPath(file);
        if (path == current) {
            foundCurrent = true;
            if (!forward) {
                if (previous.isEmpty()) return NeighborResult::Boundary;
                result = previous;
                return NeighborResult::Found;
            }
        } else if (isImage(file)) {
            if (foundCurrent && forward) {
                result = path;
                return NeighborResult::Found;
            }
            if (!forward) previous = path;
        }
    }
    return foundCurrent ? NeighborResult::Boundary : NeighborResult::Error;
}

void showImage(void* parameter) {
    String path = *static_cast<String*>(parameter);
    bool fullscreen = false, redraw = true;
    auto& system = System::getInstance();
    while (true) {
        if (redraw) {
            if (fullscreen) system.gfx->fillScreen(RGB565_BLACK);
            else clearContent();
            decodeStarted = millis();
            cancelled = timedOut = ioFailed = false;
            const char* error = loadImage(path, fullscreen);
            imageFile.close();
            if (cancelled) {
                releaseButton(BUTTON_LEFT_PIN);
                break;
            }
            if (timedOut) error = "Image loading exceeded 10 seconds.";
            else if (ioFailed) error = "SD read failed or image is damaged.";
            if (error) {
                if (fullscreen) system.gfx->fillScreen(RGB565_BLACK);
                message(error);
            }
            redraw = false;
        }
        if (digitalRead(BUTTON_LEFT_PIN) == LOW) {
            releaseButton(BUTTON_LEFT_PIN);
            break;
        } else if (digitalRead(BUTTON_RIGHT_PIN) == LOW) {
            releaseButton(BUTTON_RIGHT_PIN);
            fullscreen = !fullscreen;
            system.gfx->fillScreen(RGB565_BLACK);
            system.interface.setInfoPanelVisible(!fullscreen);
            redraw = true;
        } else if (digitalRead(BUTTON_UP_PIN) == LOW || digitalRead(BUTTON_DOWN_PIN) == LOW) {
            const bool forward = digitalRead(BUTTON_UP_PIN) != LOW;
            releaseButton(forward ? BUTTON_DOWN_PIN : BUTTON_UP_PIN);
            String next;
            const NeighborResult result = findNeighbor(path, forward, next);
            if (result == NeighborResult::Cancelled) {
                releaseButton(BUTTON_LEFT_PIN);
                break;
            }
            if (result == NeighborResult::Found) {
                path = next;
                redraw = true;
            } else if (result == NeighborResult::Error) {
                if (fullscreen) system.gfx->fillScreen(RGB565_BLACK);
                message("Gallery could not be scanned.");
            }
        }
        delay(10);
    }
    system.gfx->fillScreen(RGB565_BLACK);
    system.interface.setInfoPanelVisible(true);
    clearContent();
}

struct PageAction { int* page; int step; bool* reload; };
void changePage(void* parameter) {
    auto& action = *static_cast<PageAction*>(parameter);
    *action.page += action.step;
    *action.reload = true;
}
} // namespace

class GelleryApplication : public Application {
public:
    GelleryApplication() { name = "Gellery"; }
    void run() override {
        if (!System::getInstance().gfx) return;
        releaseButton(BUTTON_UP_PIN);
        if (!System::getInstance().isSDCardInserted || SD.cardType() == CARD_NONE) {
            message("SD card is not available.");
            waitBack();
            return;
        }
        if ((!SD.exists(MAIN_FOLDER) && !SD.mkdir(MAIN_FOLDER)) ||
            (!SD.exists(GALLERY_FOLDER) && !SD.mkdir(GALLERY_FOLDER))) {
            message("Gallery folder could not be created.");
            waitBack();
            return;
        }
        int page = 0;
        bool running = true;
        while (running) {
            clearContent();
            if (!hasMemory(PageSize * sizeof(String) + 8192)) {
                message("Not enough free memory for the file list.");
                waitBack();
                return;
            }
            std::unique_ptr<String[]> paths(new (std::nothrow) String[PageSize]);
            if (!paths) { message("Not enough free memory."); waitBack(); return; }
            ListMenu menu;
            auto* gfx = System::getInstance().gfx;
            menu.setGraphics(0, contentY(), gfx->width(), gfx->height() - contentY());
            menu.setHeader("Gellery / " + String(page + 1));
            bool reload = false;
            PageAction previous{&page, -1, &reload}, next{&page, 1, &reload};
            File directory = SD.open(GALLERY_FOLDER);
            if (!directory || !directory.isDirectory()) {
                message("Gallery folder could not be read.");
                waitBack();
                return;
            }
            if (page > 0) menu.addtoList("< Previous page", changePage, &previous);
            size_t seen = 0, count = 0;
            bool more = false, failed = false;
            while (true) {
                File file = directory.openNextFile();
                if (!file) break;
                if (!file.isDirectory() && seen++ >= size_t(page) * PageSize) {
                    if (count == PageSize) { more = true; break; }
                    if (!hasMemory(2048)) { failed = true; break; }
                    String filename = file.name();
                    int slash = filename.lastIndexOf('/');
                    if (slash >= 0) filename = filename.substring(slash + 1);
                    paths[count] = String(GALLERY_FOLDER) + "/" + filename;
                    if (paths[count].length() != strlen(GALLERY_FOLDER) + 1 + filename.length()) {
                        failed = true;
                        break;
                    }
                    // A row must not wrap over its neighbours; keep the full path for opening.
                    int maxChars = (gfx->width() - 10) / 12;
                    String label = filename;
                    if (int(label.length()) > maxChars && maxChars > 3) label = label.substring(0, maxChars - 3) + "...";
                    if (!menu.addtoList(label, showImage, &paths[count])) { failed = true; break; }
                    ++count;
                }
                file.close();
                delay(1);
                if (digitalRead(BUTTON_LEFT_PIN) == LOW) { releaseButton(BUTTON_LEFT_PIN); return; }
            }
            directory.close();
            if (failed) { message("Not enough free memory for the file list."); waitBack(); return; }
            if (more) menu.addtoList("Next page >", changePage, &next);
            if (count == 0) menu.addtoList("No files", nullptr);
            while (!reload && running) {
                menu.draw();
                if (digitalRead(BUTTON_LEFT_PIN) == LOW) {
                    releaseButton(BUTTON_LEFT_PIN);
                    running = false;
                } else if (digitalRead(BUTTON_RIGHT_PIN) == LOW) {
                    releaseButton(BUTTON_RIGHT_PIN);
                    menu.runSelectedItem();
                } else if (digitalRead(BUTTON_UP_PIN) == LOW) {
                    menu.decrementIndex();
                    delay(150);
                } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
                    menu.incrementIndex();
                    delay(150);
                }
                delay(10);
            }
        }
    }

    void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override {
        if (!gfx) return;
        gfx->fillRoundRect(x, y, width, height, 10, RGB565_BLUE);
        gfx->drawRoundRect(x, y, width, height, 10, RGB565_WHITE);
        gfx->drawRect(x + 10, y + 12, width - 20, height - 24, RGB565_WHITE);
        gfx->fillCircle(x + width * 3 / 4, y + height / 3, 7, RGB565_YELLOW);
        gfx->fillTriangle(x + 15, y + height - 18, x + width / 2, y + height / 3,
                          x + width - 15, y + height - 18, RGB565_GREEN);
    }
};

void registerGelleryApplication() {
    static GelleryApplication app;
    static bool registered = false;
    if (!registered) {
        System::getInstance().addApplication(&app, nullptr);
        registered = true;
    }
}
