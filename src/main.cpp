#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include "Definitions.hpp"
#include "Screen.hpp"
#include "ScreenCapture.hpp"
#include "System.hpp"
#include "BleManager.hpp"
#include "SdCardManager.hpp"
#include "libssh_esp32.h"
#include "SshManager.hpp"


void registerSettingsApplication();
void registerKeyboardTestApplication();
void registerMockApplication();
void registerShellApplication();
void registerGelleryApplication();
Application* app;

Arduino_DataBus *lcdBus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, SD_MISO_PIN, FSPI, true);

Arduino_ST7789 lcd(lcdBus, TFT_RST, 1, true, TFT_WIDTH, TFT_HEIGHT, TFT_X_OFFSET, TFT_Y_OFFSET, TFT_X_OFFSET, TFT_Y_OFFSET);
ScreenCaptureDisplay screen(lcd);
Arduino_GFX *gfx = &screen;

// SD kütüphanesi için SPI nesnesi
SPIClass sdSpi(FSPI);

Shell shell;
SSHManager sshManager(shell);


void setup()
{
    Serial.begin(115200);

    // Native USB CDC'nin hazırlanmasını bekle.
    delay(1500);

    Serial.println();
    Serial.println("ESP32-C6 SD + LCD baslatiliyor...");
    System::getInstance().setGFX(gfx);
    setScreenshotDisplay(&screen);
    System::getInstance().setSshManager(&sshManager);

    
    // ========================================
    // Application registration
    // ========================================
    registerSettingsApplication();
    registerKeyboardTestApplication();
    registerShellApplication();
    registerGelleryApplication();
    registerMockApplication();
    BLE_init();

    // ========================================
    // Chip-select initialization
    // ========================================
    pinMode(TFT_CS, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);

    // Başlangıçta iki cihaz da seçilmemiş olsun.
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);

    // ========================================
    // LCD initialization
    // ========================================
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    if (!gfx->begin(40'000'000))
    {
        Serial.println("LCD baslatilamadi.");
    }

    // Landscape orientation: 320 x 172
    gfx->fillScreen(COLOR_BACKGROUND);
    gfx->setTextWrap(true);


    digitalWrite(TFT_BL, HIGH);

    // ========================================
    // SD SPI initialization
    // ========================================
    //
    // begin(SCLK, MISO, MOSI, SS)
    //
    sdSpi.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    constexpr uint32_t SD_FREQUENCY = 10'000'000;

    if (!SD.begin(SD_CS_PIN, sdSpi, SD_FREQUENCY))
    {

        Serial.println("SD kart baglanamadi.");
        Serial.println("Kontrol et:");
        Serial.println("1. Kart yuvaya tam takili mi?");
        Serial.println("2. Kart FAT32 olarak bicimlendirilmis mi?");
        Serial.println("3. GPIO pinleri dogru mu?");
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("SD yuvasinda kart algilanmadi.");
        System::getInstance().isSDCardInserted = false;
    }
    else{
        Serial.println("SD kart basariyla baglandi.");
        System::getInstance().isSDCardInserted = true;
        FileSystemManager fsManager;
        const char* username = "admin";
        const char* password = "admin";
        int8_t fsCreateResult = fsManager.createFileSystem((char*)username, (char*)password);
        if (!screen.beginCapture()) Serial.println("Screenshot shadow could not be initialized.");
    }


    System::getInstance().wifiManager.loadKnownNetworks();
    System::getInstance().wifiManager.startAutoConnectTask();
    // UTC filenames; synchronization happens asynchronously once Wi-Fi is available.
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");


    digitalWrite(SD_CS_PIN, HIGH);

    setBacklightBrightness(127);
    app = System::getInstance().rootApplicationFolder->getApplication(0);
    System::getInstance().setButtonHandler([](const Event& event, void*) {
        const uint8_t shortcut = buttonMask(ButtonCode::Left) | buttonMask(ButtonCode::Right);
        if (event.type != EventType::ButtonChord || event.event.button.pressedMask != shortcut) return false;
        ScreenshotResult result = takeScreenshot();
        if (result.ok) Serial.printf("Screenshot saved: %s\n", result.path);
        else Serial.printf("Screenshot failed: %s\n", result.error);
        return true;
    });
    if (!System::getInstance().buttons.begin(*System::getInstance().systemEventQueue)) {
        Serial.println("Button input task could not be started.");
    }
    
}

void loop()
{
    auto& system = System::getInstance();
    system.interface.draw();
    const Event event = system.nextEvent();
    if (buttonAction(event, ButtonCode::Up)) system.interface.runApp();
    else if (buttonAction(event, ButtonCode::Right, true)) system.interface.incrementIndex();
    else if (buttonAction(event, ButtonCode::Left, true)) system.interface.decrementIndex();
    delay(5);
}
