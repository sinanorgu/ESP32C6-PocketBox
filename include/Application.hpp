#pragma once
#include <Arduino.h>
#include <vector>
#include <Arduino_GFX_Library.h>

class Arduino_GFX;

class Application {
    public:
        Application() = default;
        virtual ~Application() = default;
        virtual void run() = 0;
        virtual void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const {
            if (gfx == nullptr) {
                return;
            }

            gfx->fillRoundRect(x, y, width, height, 8, RGB565_DARKGREY);
            gfx->drawRoundRect(x, y, width, height, 8, RGB565_WHITE);
            gfx->fillRect(x + width / 4, y + height / 4, width / 2, height / 2, RGB565_LIGHTGREY);
        }
        String name;
    private:
    
};

class ApplicationFolder {
    public:
        ApplicationFolder(String folderName){
            this->folderName = folderName;
        }
        const String& getName() const {
            return folderName;
        }
        void addApplication(Application* app){
            applications.push_back(app);
        }
        void addFolder(ApplicationFolder* folder){
            childFolders.push_back(folder);
        }
        size_t getApplicationCount() const {
            return applications.size();
        }
        Application* getApplication(size_t index) const {
            if (index < applications.size()) {
                return applications[index];
            }
            return nullptr;
        }
        size_t getFolderCount() const {
            return childFolders.size();
        }
        ApplicationFolder* getFolder(size_t index) const {
            if (index < childFolders.size()) {
                return childFolders[index];
            }
            return nullptr;
        }
        void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const {
            if (gfx == nullptr) {
                return;
            }

            gfx->fillRoundRect(x, y + height / 4, width, height * 3 / 4, 6, RGB565_YELLOW);
            gfx->fillRoundRect(x + width / 8, y, width / 2, height / 3, 4, RGB565_YELLOW);
            gfx->drawRoundRect(x, y + height / 4, width, height * 3 / 4, 6, RGB565_WHITE);
        }
    private:
        String folderName;
        std::vector<Application*> applications;
        std::vector<ApplicationFolder*> childFolders;
};