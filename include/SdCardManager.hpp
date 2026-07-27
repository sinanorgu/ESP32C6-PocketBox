#pragma once 
#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#define MAIN_FOLDER         "/PocketBox"
#define SYSTEM_FOLDER       MAIN_FOLDER"/System"
#define CONFIG_FILE         SYSTEM_FOLDER"/config.json"
#define NETWORK_FILE        SYSTEM_FOLDER"/networks.json"

#define NETWORKS_FILE_CONTENT               \
"{\"networks\":[\n"                         \
    "\t{\n\t\t\"ssid\":\"Home\",\n"         \
    "\t\t\"password\":\"12345678\",\n"      \
    "\t\t\"priority\":100,\n"               \
    "\t\t\"autoConnect\":true,\n"           \
    "\t\t\"hidden\":false\n\t}\n"           \
"]}\n"

class SdCardManager {
    
public:
    SdCardManager() = default;
};

class FileSystemManager {
public:
    FileSystemManager() = default;
    int8_t createFileSystem(char* username, char* password); // create a new folder with name "PocketBox" in the root directory of the SD card
};