#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include "Definitions.hpp"

// Ekrana yazılacak mevcut satır
int16_t lcdCursorY = CONTENT_START_Y;

extern Arduino_DataBus *lcdBus ;
extern Arduino_GFX *gfx;

// Ekranda daha fazla satır için yer olup olmadığı
bool lcdHasSpace()
{
    return lcdCursorY + LINE_HEIGHT <= gfx->height();
}

// String çok uzunsa LCD genişliğine göre kısaltır.
String truncateText(const String &text, size_t maxCharacters)
{
    if (text.length() <= maxCharacters)
    {
        return text;
    }

    if (maxCharacters <= 3)
    {
        return text.substring(0, maxCharacters);
    }

    return text.substring(0, maxCharacters - 3) + "...";
}

// File.name() bazı sürümlerde tam yolu döndürebilir.
// Yalnızca son dosya/klasör adını çıkarır.
String getBaseName(const char *path)
{
    if (path == nullptr)
    {
        return "";
    }

    String result(path);

    while (result.endsWith("/") && result.length() > 1)
    {
        result.remove(result.length() - 1);
    }

    const int slashPosition = result.lastIndexOf('/');

    if (slashPosition >= 0)
    {
        return result.substring(slashPosition + 1);
    }

    return result;
}

// Dosya boyutunu okunabilir hale getirir.
String formatFileSize(uint64_t sizeBytes)
{
    if (sizeBytes < 1024ULL)
    {
        return String(static_cast<unsigned long>(sizeBytes)) + " B";
    }

    if (sizeBytes < 1024ULL * 1024ULL)
    {
        const float sizeKB = sizeBytes / 1024.0f;
        return String(sizeKB, 1) + " KB";
    }

    if (sizeBytes < 1024ULL * 1024ULL * 1024ULL)
    {
        const float sizeMB = sizeBytes / (1024.0f * 1024.0f);
        return String(sizeMB, 1) + " MB";
    }

    const float sizeGB =
        sizeBytes / (1024.0f * 1024.0f * 1024.0f);

    return String(sizeGB, 1) + " GB";
}

void drawHeader(const char *path)
{
    gfx->fillScreen(COLOR_BACKGROUND);

    gfx->setTextSize(1);
    gfx->setTextWrap(false);

    gfx->setTextColor(COLOR_HEADER);
    gfx->setCursor(SCREEN_MARGIN_X, HEADER_Y);
    gfx->print("SD CARD: ");

    String displayedPath = truncateText(String(path), 17);

    gfx->setTextColor(COLOR_FILE);
    gfx->print(displayedPath);

    gfx->drawFastHLine(
        0,
        22,
        gfx->width(),
        COLOR_SEPARATOR);

    lcdCursorY = CONTENT_START_Y;
}

void drawDirectoryLine(const String &name, uint8_t depth)
{
    if (!lcdHasSpace())
    {
        return;
    }

    // Her recursion seviyesi için küçük girinti
    const int16_t indent = depth * 7;
    const int16_t x = SCREEN_MARGIN_X + indent;

    gfx->setCursor(x, lcdCursorY);
    gfx->setTextColor(COLOR_DIRECTORY);
    gfx->setTextSize(1);

    gfx->print("[D] ");

    // Derin klasörlerde kullanılabilir karakter sayısını azalt
    const size_t maxNameLength =
        depth >= 3 ? 14 : depth == 2 ? 16
                      : depth == 1   ? 18
                                     : 21;

    gfx->print(truncateText(name, maxNameLength));

    lcdCursorY += LINE_HEIGHT;
}

void drawFileLine(const String &name, uint64_t sizeBytes, uint8_t depth)
{
    if (!lcdHasSpace())
    {
        return;
    }

    const int16_t indent = depth * 7;
    const int16_t x = SCREEN_MARGIN_X + indent;

    gfx->setCursor(x, lcdCursorY);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_FILE);

    gfx->print("[F] ");

    // Sağ tarafta boyut bilgisinin sığması için
    // dosya adını daha kısa tutuyoruz.
    const size_t maxNameLength =
        depth >= 3 ? 7 : depth == 2 ? 9
                     : depth == 1   ? 11
                                    : 13;

    gfx->print(truncateText(name, maxNameLength));

    String sizeText = formatFileSize(sizeBytes);

    // Dosya boyutunu ekranın sağ tarafına hizala.
    // Varsayılan font yaklaşık 6 px/karakter genişliğindedir.
    const int16_t sizeX =
        gfx->width() -
        static_cast<int16_t>(sizeText.length() * 6) -
        SCREEN_MARGIN_X;

    gfx->setCursor(sizeX, lcdCursorY);
    gfx->setTextColor(COLOR_SIZE);
    gfx->print(sizeText);

    lcdCursorY += LINE_HEIGHT;
}

void drawMoreItemsIndicator()
{
    // Son satırı temizleyip "... more items" yaz.
    const int16_t y = gfx->height() - LINE_HEIGHT;

    gfx->fillRect(
        0,
        y,
        gfx->width(),
        LINE_HEIGHT,
        COLOR_BACKGROUND);

    gfx->setCursor(SCREEN_MARGIN_X, y);
    gfx->setTextColor(COLOR_HEADER);
    gfx->setTextSize(1);
    gfx->print("... more items");
}

// ========================================
// LCD directory listing
// ========================================
//
// dirname:
//   Gösterilecek başlangıç dizini.
//
// levels:
//   Kaç alt klasör seviyesine girileceği.
//
// depth:
//   Görsel girinti seviyesi.
//
void printDirectoryToLCD(fs::FS &fs, const char *dirname, uint8_t levels, uint8_t depth = 0)
{
    File root = fs.open(dirname);

    if (!root)
    {
        gfx->setCursor(SCREEN_MARGIN_X, lcdCursorY);
        gfx->setTextColor(COLOR_ERROR);
        gfx->print("Cannot open directory");
        return;
    }

    if (!root.isDirectory())
    {
        gfx->setCursor(SCREEN_MARGIN_X, lcdCursorY);
        gfx->setTextColor(COLOR_ERROR);
        gfx->print("Path is not directory");
        root.close();
        return;
    }

    File file = root.openNextFile();

    while (file)
    {
        if (!lcdHasSpace())
        {
            drawMoreItemsIndicator();
            file.close();
            break;
        }

        const String fileName = getBaseName(file.name());

        if (file.isDirectory())
        {
            drawDirectoryLine(fileName, depth);

            if (levels > 0 && lcdHasSpace())
            {
                // file.path() geçerli olduğu sürece recursive çağrı
                printDirectoryToLCD(
                    fs,
                    file.path(),
                    levels - 1,
                    depth + 1);
            }
        }
        else
        {
            drawFileLine(
                fileName,
                static_cast<uint64_t>(file.size()),
                depth);
        }

        file.close();
        file = root.openNextFile();
    }

    root.close();
}

void refreshDirectoryScreen(fs::FS &fs, const char *dirname, uint8_t levels)
{
    /*
     * SD işlemi tamamlandıktan sonra SD CS HIGH olmalı.
     * LCD kütüphanesi kendi işlemi sırasında TFT_CS pinini yönetir.
     */
    digitalWrite(SD_CS_PIN, HIGH);

    drawHeader(dirname);
    printDirectoryToLCD(fs, dirname, levels);
}

// ========================================
// Serial directory listing
// ========================================
void listDirectory(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("\nDizin aciliyor: %s\n", dirname);

    File root = fs.open(dirname);

    if (!root)
    {
        Serial.println("Dizin acilamadi.");
        return;
    }

    if (!root.isDirectory())
    {
        Serial.println("Verilen yol bir dizin degil.");
        root.close();
        return;
    }

    File file = root.openNextFile();

    while (file)
    {
        if (file.isDirectory())
        {
            Serial.printf("[DIR]  %s\n", file.name());

            if (levels > 0)
            {
                listDirectory(
                    fs,
                    file.path(),
                    levels - 1);
            }
        }
        else
        {
            Serial.printf(
                "[FILE] %-32s %llu byte\n",
                file.name(),
                static_cast<unsigned long long>(file.size()));
        }

        file.close();
        file = root.openNextFile();
    }

    root.close();
}

bool writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("\nDosya yaziliyor: %s\n", path);
    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Dosya yazmak icin acilamadi.");
        return false;
    }

    const bool success = file.print(message);
    file.close();

    if (!success)
    {
        Serial.println("Dosya yazma basarisiz.");
        return false;
    }

    Serial.println("Dosya yazildi.");
    return true;
}

bool readFile(fs::FS &fs, const char *path)
{
    Serial.printf("\nDosya okunuyor: %s\n", path);

    File file = fs.open(path, FILE_READ);

    if (!file)
    {
        Serial.println("Dosya acilamadi.");
        return false;
    }

    Serial.println("----- DOSYA ICERIGI -----");

    while (file.available())
    {
        Serial.write(file.read());
    }

    Serial.println("\n-------------------------");

    file.close();
    return true;
}

void printCardInfo()
{
    const uint8_t cardType = SD.cardType();

    Serial.print("Kart tipi: ");

    switch (cardType)
    {
    case CARD_MMC:
        Serial.println("MMC");
        break;

    case CARD_SD:
        Serial.println("SDSC");
        break;

    case CARD_SDHC:
        Serial.println("SDHC/SDXC");
        break;

    case CARD_NONE:
        Serial.println("Kart bulunamadi");
        break;

    default:
        Serial.println("Bilinmeyen");
        break;
    }

    const uint64_t cardSizeMB =
        SD.cardSize() / (1024ULL * 1024ULL);

    const uint64_t totalMB =
        SD.totalBytes() / (1024ULL * 1024ULL);

    const uint64_t usedMB =
        SD.usedBytes() / (1024ULL * 1024ULL);

    Serial.printf(
        "Kart kapasitesi : %llu MB\n",
        cardSizeMB);

    Serial.printf(
        "Dosya sistemi   : %llu MB\n",
        totalMB);

    Serial.printf(
        "Kullanilan alan : %llu MB\n",
        usedMB);
}

void showLCDMessage(const char *title, const char *message, uint16_t color)
{
    digitalWrite(SD_CS_PIN, HIGH);

    gfx->fillScreen(COLOR_BACKGROUND);
    gfx->setTextWrap(true);

    gfx->setTextColor(COLOR_HEADER);
    gfx->setTextSize(1);
    gfx->setCursor(5, 8);
    gfx->println(title);

    gfx->drawFastHLine(
        0,
        23,
        gfx->width(),
        COLOR_SEPARATOR);

    gfx->setTextColor(color);
    gfx->setCursor(5, 34);
    gfx->println(message);
}
