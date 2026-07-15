#include "Application.hpp"
#include "System.hpp"

class SettingsApplication : public Application {
    public:
        SettingsApplication();
        void run() override;
    void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

SettingsApplication::SettingsApplication(){
    name = "Settings";
}

void SettingsApplication::run() {
    Serial.println("Settings application opened");
}

void SettingsApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const {
    if (gfx == nullptr) {
        return;
    }

    const int16_t centerX = x + width / 2;
    const int16_t centerY = y + height / 2;
    const int16_t radius = min(width, height) / 3;
    const int16_t halfHeight =
        static_cast<int16_t>(radius * 0.8660254f);

    const uint16_t iconColor = RGB565(200, 250, 250);

    const int16_t px[6] = {
        static_cast<int16_t>(centerX - radius),
        static_cast<int16_t>(centerX - radius / 2),
        static_cast<int16_t>(centerX + radius / 2),
        static_cast<int16_t>(centerX + radius),
        static_cast<int16_t>(centerX + radius / 2),
        static_cast<int16_t>(centerX - radius / 2)
    };

    const int16_t py[6] = {
        centerY,
        static_cast<int16_t>(centerY - halfHeight),
        static_cast<int16_t>(centerY - halfHeight),
        centerY,
        static_cast<int16_t>(centerY + halfHeight),
        static_cast<int16_t>(centerY + halfHeight)
    };

    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t next = (i + 1) % 6;

        gfx->fillTriangle(
            centerX,
            centerY,
            px[i],
            py[i],
            px[next],
            py[next],
            iconColor
        );
    }

    gfx->fillCircle(
        centerX,
        centerY,
        radius / 3,
        RGB565_BLACK
    );

}

void registerSettingsApplication() {
    static bool registered = false;
    if (registered) {
        return;
    }

    System& system = System::getInstance();
    system.addApplication(new SettingsApplication(), nullptr);
    ApplicationFolder* utilitiesFolder = new ApplicationFolder("Utilities");
    system.addApplication(new SettingsApplication(), utilitiesFolder);
    system.addApplicationFolder(utilitiesFolder);
    registered = true;
}