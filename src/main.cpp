#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include "definitions.hpp"
#include "Screen.hpp"

Arduino_DataBus *lcdBus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, SD_MISO_PIN, FSPI, true);

Arduino_GFX *gfx = new Arduino_ST7789(lcdBus, TFT_RST, 0, TFT_WIDTH, TFT_HEIGHT);

// SD kütüphanesi için SPI nesnesi
SPIClass sdSpi(FSPI);


void setup()
{
    Serial.begin(115200);

    // Native USB CDC'nin hazırlanmasını bekle.
    delay(1500);

    Serial.println();
    Serial.println("ESP32-C6 SD + LCD baslatiliyor...");

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
    gfx->setRotation(0);
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

    setBacklightBrightness(1);

    gfx->fillScreen(RGB565_CYAN);
}

void loop()
{
    /*
     * Buton kısmı değiştirilmedi.
     * INPUT_PULLUP olduğundan basılı durum LOW'dur.
     */

    if (digitalRead(BUTTON_UP_PIN) == LOW)
    {
        Serial.println("UP");
    }

    if (digitalRead(BUTTON_RIGHT_PIN) == LOW)
    {
        Serial.println("RIGHT");
    }

    if (digitalRead(BUTTON_DOWN_PIN) == LOW)
    {
        Serial.println("DOWN");
    }

    if (digitalRead(BUTTON_LEFT_PIN) == LOW)
    {
        Serial.println("LEFT");
    }
}