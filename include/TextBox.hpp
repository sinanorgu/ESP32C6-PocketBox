#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "Event.hpp"


template <size_t Capacity>
class TextBox {
    public:
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    char text[Capacity];
    uint16_t textSize;
    uint16_t textColor;
    uint16_t backgroundColor;
    
    public:
        TextBox(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t textColor, uint16_t backgroundColor)
        : x(x), y(y), width(width), height(height), textColor(textColor), backgroundColor(backgroundColor) {
            text[0] = '\0'; // Initialize the text array
            textSize = 0; // Default text size
        }
        void clearText() {  
            text[0] = '\0'; // Clear the text
            textSize = 0; // Reset the text size
        }

        void setText(Arduino_GFX* gfx, char *newText, size_t size) {
            Event event;
            event.type = EventType::TextInput;
            for(int i = 0; i < size && i < Capacity - 1; i++) {
                event.event.keyboard.character = newText[i];
                draw(gfx, event); // Draw the character                
            }
        }

        void setPosition(int16_t newX, int16_t newY) {
            x = newX;
            y = newY;
        }
        void setSize(int16_t newWidth, int16_t newHeight) {
            width = newWidth;
            height = newHeight;
        }

        void draw(Arduino_GFX* gfx, Event event) {
            if (gfx == nullptr) {
                return;
            }
            if(event.type == EventType::TextInput) {
                if(event.event.keyboard.character == 0x08) { // Backspace
                    if(textSize > 0) {
                        int beforeCursorX = gfx->getCursorX();
                        int beforeCursorY = gfx->getCursorY();
                        text[textSize - 1] = '\0'; // Remove the last character
                        textSize--;
                        gfx->setCursor(x, y);
                        gfx->print(text); // Print the updated text
                        int cursorX = gfx->getCursorX();
                        int cursorY = gfx->getCursorY();
                        int fillLenX = width + x - cursorX; 
                        int fillLenY = height + y - cursorY;
                        gfx->fillRect(cursorX, cursorY, width, height, backgroundColor); // Clear the text box
                        if (cursorY < beforeCursorY) {
                            gfx->fillRect(0, beforeCursorY, width, height + y - beforeCursorY , backgroundColor); // Clear the line above if needed
                        }
                    }
                } 
                else {
                    if(textSize >= Capacity - 1) {
                        return; // Prevent buffer overflow
                    }
                    if(textSize == 0) {
                        gfx->setCursor(x, y); // Set cursor to the start of the text box
                    }
                    text[textSize] = static_cast<char>(event.event.keyboard.character); // Add the new character
                    textSize++;
                    text[textSize] = '\0'; // Null-terminate the string
                    gfx->print(static_cast<char>(event.event.keyboard.character)); // Print the new character
                }
            }
        }
};