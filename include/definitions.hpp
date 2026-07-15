#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "Application.hpp"

// ========================================
// SD card pins
// ========================================
static constexpr uint8_t SD_CS_PIN   = 4;
static constexpr uint8_t SD_MISO_PIN = 5;
static constexpr uint8_t SD_MOSI_PIN = 6;
static constexpr uint8_t SD_SCLK_PIN = 7;

// ========================================
// Button pins
// ========================================
static constexpr uint8_t BUTTON_UP_PIN    = 0;
static constexpr uint8_t BUTTON_LEFT_PIN  = 1;
static constexpr uint8_t BUTTON_RIGHT_PIN = 2;
static constexpr uint8_t BUTTON_DOWN_PIN  = 3;

// ========================================
// Waveshare ESP32-C6-LCD-1.47 pins
// ========================================
static constexpr uint8_t TFT_CS   = 14;
static constexpr uint8_t TFT_DC   = 15;
static constexpr uint8_t TFT_RST  = 21;
static constexpr uint8_t TFT_BL   = 22;
static constexpr uint8_t TFT_MOSI = 6;
static constexpr uint8_t TFT_SCLK = 7;


// ========================================
// LCD geometry
// ========================================
static constexpr int16_t TFT_WIDTH  = 172;
static constexpr int16_t TFT_HEIGHT = 320;

// ST7789'in fiziksel RAM genişliği 240 piksel.
// 172 piksellik görüntü alanı ortalandığı için:
// (240 - 172) / 2 = 34 pixel offset
static constexpr int16_t TFT_X_OFFSET = 34;
static constexpr int16_t TFT_Y_OFFSET = 0;

// ========================================
// Shared SPI bus
// ========================================
//
// LCD ve SD aynı fiziksel SPI hattını kullanıyor:
//
// SCLK = GPIO7
// MOSI = GPIO6
//
// SD ayrıca MISO = GPIO5 kullanıyor.
//
// Arduino_ESP32SPI constructor:
// DC, CS, SCLK, MOSI, MISO, SPI peripheral, shared interface
//


// ========================================
// LCD file browser appearance
// ========================================
static constexpr uint16_t COLOR_BACKGROUND = RGB565_BLACK;
static constexpr uint16_t COLOR_HEADER      = RGB565_CYAN;
static constexpr uint16_t COLOR_DIRECTORY   = RGB565_YELLOW;
static constexpr uint16_t COLOR_FILE        = RGB565_WHITE;
static constexpr uint16_t COLOR_SIZE        = RGB565_LIGHTGREY;
static constexpr uint16_t COLOR_ERROR       = RGB565_RED;
static constexpr uint16_t COLOR_SEPARATOR   = RGB565_DARKGREY;

static constexpr int16_t SCREEN_MARGIN_X = 4;
static constexpr int16_t HEADER_Y        = 5;
static constexpr int16_t CONTENT_START_Y = 30;
static constexpr int16_t LINE_HEIGHT     = 14;