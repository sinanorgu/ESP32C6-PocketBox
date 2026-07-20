#include "SdCardManager.hpp"


int8_t FileSystemManager::checkFileSystem() {
    if(SD.exists("/PocketBox")) {
        Serial.println("📁 PocketBox klasörü bulundu.");
        if(!SD.exists("/PocketBox/config.json")) {
            return -1;
        }
        return 0;
    } else {
        Serial.println("📁 PocketBox klasörü bulunamadı.");
        return -2;
    }
}

int8_t FileSystemManager::createFileSystem(char* username, char* password) {
    if(!SD.mkdir("/PocketBox")) {
        Serial.println("📁 PocketBox klasörü oluşturulamadı.");
        return -1;
    }
    File file = SD.open("/PocketBox/config.json", FILE_WRITE);
    if(!file) {
        return -2;
    }
    file.printf("{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
    file.close();
    char usernameFolderPath[50];
    snprintf(usernameFolderPath, sizeof(usernameFolderPath), "/PocketBox/Users/%s", username);
    if(!SD.mkdir(usernameFolderPath)) {
        Serial.println("📁 Kullanıcı klasörü oluşturulamadı.");
        return -3;
    }

    return 0;
}

