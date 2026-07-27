#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "Definitions.hpp"


class ListMenuItem {
public:
    ListMenuItem(const String& n, void (*a)(void *), void* params = nullptr) : name(n), action(a), params(params) {}
    String name;
    void (*action)(void *);
    void* params;
};

class ListMenu {

    public:
        ListMenu() : innerIndex(0), offset(0), changed(true), itemCount(0), maxVisibleItems(0), textSize(2) {};
        void draw() {
            if(System::getInstance().gfx == nullptr || changed == false){

                return;
            }
            Serial.printf("reached line:%d in file %s\n", __LINE__, __FILE__);
            System::getInstance().gfx->fillRect(x, y, width, height, COLOR_BACKGROUND);
            System::getInstance().gfx->setCursor(x + 5, y + 5);
            System::getInstance().gfx->setTextSize(textSize);
            System::getInstance().gfx->setTextColor(RGB565_WHITE);

            for(int i = 0; i < maxVisibleItems && i < itemCount; i++){
                int itemIndex = offset + i;
                if(itemIndex < 64 && items[itemIndex]->name.length() > 0){
                    if(i == innerIndex){
                        System::getInstance().gfx->fillRect(x + 2, y + 2 + i * textHeight, width - 4, textHeight, RGB565_CYAN);
                        System::getInstance().gfx->setTextColor(RGB565_BLACK);
                    } else {
                        System::getInstance().gfx->setTextColor(RGB565_WHITE);
                    }
                    System::getInstance().gfx->setCursor(x + 5, y + 5 + i * textHeight);
                    System::getInstance().gfx->print(items[itemIndex]->name);
                }
            }
            changed = false;
        }

        void incrementIndex(){
            if(innerIndex < maxVisibleItems - 1 && innerIndex < itemCount - 1){
                innerIndex++;
                changed = true;
            } else if(offset + innerIndex < itemCount-1){
                offset++;
                changed = true;
            }
        }

        void decrementIndex(){
            if(innerIndex > 0){
                innerIndex--;
                changed = true;
            } else if(offset > 0){
                offset--;
                changed = true;
            }
        }

        void runSelectedItem(){
            int selectedIndex = offset + innerIndex;
            if(selectedIndex < itemCount && items[selectedIndex]->action != nullptr){
                items[selectedIndex]->action(items[selectedIndex]->params);
            }
        }
        bool addtoList(const String& name, void (*action)(void *), void* params = nullptr){
            if(itemCount < 64){
                items[itemCount] = new ListMenuItem(name, action, params);
                itemCount++;
                return true;
            }
            return false;
        }

        void setGraphics(int16_t x, int16_t y, int16_t width, int16_t height){
            Serial.printf("reached line:%d\n", __LINE__);
            
            this->x = x;
            this->y = y;
            this->width = width;
            this->height = height;
            Serial.printf("reached line:%d\n", __LINE__);
            //System::getInstance().gfx->getTextBounds("A", 0, 0, nullptr, nullptr, nullptr, &textHeight);

            Serial.printf("reached line:%d\n", __LINE__);
            this->textHeight = 8 * textSize+4; // Assuming a base height of 8 pixels for text size 1
            this->maxVisibleItems = height / textHeight;
            
        }
    
    public :
        bool changed;
    private:
        int innerIndex;
        int offset;
        ListMenuItem* items[64];
        uint8_t itemCount ;
        int maxVisibleItems;
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        uint8_t textSize;
        uint16_t textHeight;
    };