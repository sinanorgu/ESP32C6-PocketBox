#pragma once 
#include <Arduino.h>
#include <FS.h>
#include <SD.h>

class SdCardManager {
    
public:
    SdCardManager() = default;
};

class FileSystemManager {
public:
    FileSystemManager() = default;
    int8_t checkFileSystem(); // check if the "PocketBox" folder exists in the root directory of the SD card
    int8_t createFileSystem(char* username, char* password); // create a new folder with name "PocketBox" in the root directory of the SD card
};