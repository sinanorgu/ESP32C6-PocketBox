#include "Application.hpp"
#include "System.hpp"
#include "ListMenu.hpp"
#include "Definitions.hpp"

class SettingsApplication : public Application {
    public:
        SettingsApplication();
        void run() override;
    void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

SettingsApplication::SettingsApplication(){
    name = "Settings";
}

void wifiSettingsCallback() {
    Serial.println("WiFi Settings selected");
    System::getInstance().wifiManager.scanWifiNetworks();
    
}
void SettingsApplication::run() {
    Serial.println("Settings application opened");
    ListMenu menu;

    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;

    menu.setGraphics(menuX, menuY, menuWidth, menuHeight);

    menu.addtoList("WiFi Settings", wifiSettingsCallback);

    menu.addtoList("Display Settings", nullptr);
    menu.addtoList("System Info", nullptr);
    menu.addtoList("System Info1", nullptr);
    menu.addtoList("System Info2", nullptr);
    

    
    
    while (true) {
        menu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            menu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            menu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            menu.runSelectedItem();
            delay(200);
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }

    }
    
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