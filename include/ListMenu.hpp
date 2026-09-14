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
        ListMenu() : innerIndex(0), offset(0), changed(true), itemCount(0), maxVisibleItems(0), textSize(2), textHeight(0), headerHeight(0), header(""), fullRedraw(true), renderedInnerIndex(0), renderedOffset(0), hasRendered(false) {};
        void setHeader(const String& header){
            this->header = header;
            updateLayout();
            invalidate();
        }
        void invalidate(){
            changed = true;
            fullRedraw = true;
            hasRendered = false;
        }
        void draw() {
            if(System::getInstance().gfx == nullptr || changed == false){

                return;
            }
            Serial.printf("reached line:%d in file %s\n", __LINE__, __FILE__);
            System::getInstance().gfx->setTextSize(textSize);
            if(fullRedraw){
                clearItems();
            }
            if(fullRedraw && header.length() > 0){
                System::getInstance().gfx->fillRect(x + 2, y + 2, width - 4, textHeight, RGB565_DARKGREY);
                System::getInstance().gfx->setCursor(x + 5, y + 5);
                System::getInstance().gfx->setTextColor(RGB565_YELLOW);
                System::getInstance().gfx->print(header);
            }

            if(fullRedraw || !hasRendered){
                for(int i = 0; i < maxVisibleItems && i < itemCount; i++){
                    drawItem(i, offset + i, i == innerIndex);
                }
            } else {
                drawItem(renderedInnerIndex, renderedOffset + renderedInnerIndex, false);
                drawItem(innerIndex, offset + innerIndex, true);
            }

            renderedInnerIndex = innerIndex;
            renderedOffset = offset;
            hasRendered = true;
            fullRedraw = false;
            changed = false;
        }

        void incrementIndex(){
            if(innerIndex < maxVisibleItems - 1 && innerIndex < itemCount - 1){
                innerIndex++;
                changed = true;
            } else if(offset + innerIndex < itemCount-1){
                offset++;
                changed = true;
                fullRedraw = true;
            }
        }

        void decrementIndex(){
            if(innerIndex > 0){
                innerIndex--;
                changed = true;
            } else if(offset > 0){
                offset--;
                changed = true;
                fullRedraw = true;
            }
        }

        void runSelectedItem(){
            int selectedIndex = offset + innerIndex;
            if(selectedIndex < itemCount && items[selectedIndex]->action != nullptr){
                items[selectedIndex]->action(items[selectedIndex]->params);
                invalidate();
            }
        }
        bool addtoList(const String& name, void (*action)(void *), void* params = nullptr){
            if(itemCount < 64){
                items[itemCount] = new ListMenuItem(name, action, params);
                itemCount++;
                invalidate();
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
            updateLayout();
            invalidate();
            
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
        String header;
        uint16_t headerHeight;
        bool fullRedraw;
        int renderedInnerIndex;
        int renderedOffset;
        bool hasRendered;

        void updateLayout(){
            headerHeight = header.length() > 0 ? textHeight : 0;
            maxVisibleItems = textHeight > 0
                ? (height - headerHeight) / textHeight
                : 0;
        }

        void clearItems(){
            System::getInstance().gfx->fillRect(
                x,
                y + headerHeight,
                width,
                height - headerHeight,
                COLOR_BACKGROUND);
        }

        void drawItem(int visibleIndex, int itemIndex, bool selected){
            if(visibleIndex < 0 || visibleIndex >= maxVisibleItems){
                return;
            }

            const int16_t itemY = y + headerHeight + visibleIndex * textHeight;
            System::getInstance().gfx->fillRect(
                x + 2,
                itemY + 2,
                width - 4,
                textHeight,
                selected ? RGB565_CYAN : COLOR_BACKGROUND);

            if(itemIndex < 0 || itemIndex >= itemCount || itemIndex >= 64 ||
               items[itemIndex]->name.length() == 0){
                return;
            }

            System::getInstance().gfx->setTextColor(
                selected ? RGB565_BLACK : RGB565_WHITE);
            System::getInstance().gfx->setCursor(x + 5, itemY + 5);
            System::getInstance().gfx->print(items[itemIndex]->name);
        }
    };