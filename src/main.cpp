#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include "Definitions.hpp"
#include "Screen.hpp"
#include "System.hpp"
#include "BleManager.hpp"


void registerSettingsApplication();
void registerKeyboardTestApplication();
void registerMockApplication();
Application* app;

Arduino_DataBus *lcdBus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, SD_MISO_PIN, FSPI, true);

Arduino_GFX *gfx = new Arduino_ST7789(lcdBus, TFT_RST, 1, true, TFT_WIDTH, TFT_HEIGHT, TFT_X_OFFSET, TFT_Y_OFFSET, TFT_X_OFFSET, TFT_Y_OFFSET);

// SD kütüphanesi için SPI nesnesi
SPIClass sdSpi(FSPI);


void setup()
{
    Serial.begin(115200);

    // Native USB CDC'nin hazırlanmasını bekle.
    delay(1500);

    Serial.println();
    Serial.println("ESP32-C6 SD + LCD baslatiliyor...");
    System::getInstance().setGFX(gfx);
    registerSettingsApplication();
    registerKeyboardTestApplication();
    registerMockApplication();
    BLE_init();

    // ========================================
    // Button initialization
    // ========================================
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);

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

    // Portrait orientation: 172 x 320
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
        return;
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("SD yuvasinda kart algilanmadi.");
        return;
    }
    Serial.println("SD kart basariyla baglandi.");
    digitalWrite(SD_CS_PIN, HIGH);

    setBacklightBrightness(127);
    app = System::getInstance().rootApplicationFolder->getApplication(0);
    
}

int x = 0;
int y = 0;
bool changed = true;
void loop()
{

    System::getInstance().interface.draw();
    

    if(digitalRead(BUTTON_UP_PIN) == 0){
        System::getInstance().interface.runApp();
        System::getInstance().interface.changed = true; // Mark the interface as changed to redraw the main menu

    }
    if(digitalRead(BUTTON_DOWN_PIN) == 0){
        y++;
        changed = true;
    }
    if(digitalRead(BUTTON_RIGHT_PIN) == 0){
        System::getInstance().interface.incrementIndex();
        delay(200);
    }
    if(digitalRead(BUTTON_LEFT_PIN) == 0){
        System::getInstance().interface.decrementIndex();
        delay(200);
    }


    


}