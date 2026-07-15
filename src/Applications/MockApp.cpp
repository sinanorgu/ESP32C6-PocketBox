#include "Application.hpp"
#include "System.hpp"

class MockAppApplication : public Application {
    public:
        MockAppApplication();
        void run() override;
        void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

class ReaderAppApplication : public Application {
    public:
        ReaderAppApplication();
        void run() override;
        void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

class CameraAppApplication : public Application {
    public:
        CameraAppApplication();
        void run() override;
        void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

class MusicAppApplication : public Application {
    public:
        MusicAppApplication();
        void run() override;
        void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

MockAppApplication::MockAppApplication()
{
    name = "Mock App";
}

void MockAppApplication::run()
{
    Serial.println("Mock App opened");
}

void MockAppApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const
{
    if (gfx == nullptr) {
        return;
    }

    gfx->fillRoundRect(x, y, width, height, 10, RGB565_BLUE);
    gfx->drawRoundRect(x, y, width, height, 10, RGB565_WHITE);
    gfx->fillRoundRect(x + 6, y + 6, width - 12, height - 12, 8, RGB565_CYAN);
    gfx->fillTriangle(x + width / 2, y + 12, x + width / 2 - 10, y + height - 12, x + width / 2 + 10, y + height - 12, RGB565_WHITE);
}

ReaderAppApplication::ReaderAppApplication()
{
    name = "Reader";
}

void ReaderAppApplication::run()
{
    Serial.println("Reader opened");
}

void ReaderAppApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const
{
    if (gfx == nullptr) {
        return;
    }

    gfx->fillRoundRect(x, y, width, height, 10, RGB565_RED);
    gfx->drawRoundRect(x, y, width, height, 10, RGB565_WHITE);
    gfx->fillRect(x + 10, y + 10, width - 20, height - 20, RGB565_YELLOW);
    gfx->drawLine(x + 14, y + 18, x + width - 14, y + 18, RGB565_RED);
    gfx->drawLine(x + 14, y + 30, x + width - 14, y + 30, RGB565_RED);
}

CameraAppApplication::CameraAppApplication()
{
    name = "Camera";
}

void CameraAppApplication::run()
{
    Serial.println("Camera opened");
}

void CameraAppApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const
{
    if (gfx == nullptr) {
        return;
    }

    gfx->fillRoundRect(x, y, width, height, 10, RGB565_DARKGREY);
    gfx->drawRoundRect(x, y, width, height, 10, RGB565_WHITE);
    gfx->fillRoundRect(x + 10, y + 12, width - 20, height - 24, 8, RGB565_GREEN);
    gfx->fillCircle(x + width / 2, y + height / 2, 10, RGB565_WHITE);
}

MusicAppApplication::MusicAppApplication()
{
    name = "Music";
}

void MusicAppApplication::run()
{
    Serial.println("Music opened");
}

void MusicAppApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const
{
    if (gfx == nullptr) {
        return;
    }

    gfx->fillRoundRect(x, y, width, height, 10, RGB565_BLUE);
    gfx->drawRoundRect(x, y, width, height, 10, RGB565_WHITE);
    gfx->fillCircle(x + width / 2 - 8, y + height / 2 + 6, 8, RGB565_WHITE);
    gfx->drawLine(x + width / 2 - 8, y + height / 2 - 18, x + width / 2 - 8, y + height / 2 + 6, RGB565_WHITE);
    gfx->drawLine(x + width / 2 - 8, y + height / 2 - 18, x + width / 2 + 10, y + height / 2 - 12, RGB565_WHITE);
}

void registerMockApplication()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    System& system = System::getInstance();
    system.addApplication(new MockAppApplication(), nullptr);
    system.addApplication(new ReaderAppApplication(), nullptr);
    system.addApplication(new CameraAppApplication(), nullptr);
    system.addApplication(new MusicAppApplication(), nullptr);
    registered = true;
}