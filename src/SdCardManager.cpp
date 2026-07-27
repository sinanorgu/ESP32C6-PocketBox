#include "SdCardManager.hpp"


int8_t FileSystemManager::createFileSystem(char* username, char* password) {
    if(!SD.mkdir(MAIN_FOLDER)) {
        Serial.println("📁 PocketBox klasörü oluşturulamadı.");
        return -1;
    }
    if(!SD.mkdir(SYSTEM_FOLDER)) {
        Serial.println("📁 System klasörü oluşturulamadı.");
        return -2;
    }

    if(!SD.exists(CONFIG_FILE)) {
        File file = SD.open(CONFIG_FILE, FILE_WRITE);
        if(!file) {
            return -3;
        }
        file.printf("{\"username\":\"%s\",\"password\":\"%s\"}\n", username, password);
        file.close();
    }
    
    if(!SD.exists(NETWORK_FILE)) {
        File file = SD.open(NETWORK_FILE, FILE_WRITE);
        if(!file) {
            return -4;
        }
        file.printf(NETWORKS_FILE_CONTENT);
        file.close();
    }
    return 0;
}