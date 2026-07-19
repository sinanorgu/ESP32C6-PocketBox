#include "Application.hpp"
#include "System.hpp"
#include "ListMenu.hpp"
#include "Definitions.hpp"
#include "Event.hpp"
#include "TextBox.hpp"

class KeyboardTestApplication : public Application {
    public:
        KeyboardTestApplication();
        void run() override;
    void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

KeyboardTestApplication::KeyboardTestApplication(){
    name = "Keyboard Test";
}

void KeyboardTestApplication::run() {

    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;
    System::getInstance().gfx->fillRect(menuX, menuY, menuWidth, menuHeight, COLOR_BACKGROUND);

    System::getInstance().gfx->setCursor(menuX + 10, menuY + 10);
    System::getInstance().gfx->setTextColor(RGB565_WHITE);
    System::getInstance().gfx->setTextSize(2);
    TextBox<256> txtbox(menuX , menuY , menuWidth - 20, menuHeight - 20, RGB565_WHITE, COLOR_BACKGROUND);
        
    Event event;
    
    while (true) {

        event = System::getInstance().systemEventQueue->pop(event)? event : Event(); 
        
        if(event.type == EventType::TextInput) {        
            //System::getInstance().gfx->print((char)event.event.keyboard.character);
            txtbox.draw(System::getInstance().gfx, event);
        } 

        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            delay(200);
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }

    }
    
}

void KeyboardTestApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const {
    if (gfx == nullptr) {
        return;
    }
    int p = 20; 
    gfx->fillRoundRect(x + width/p, y+height/p, width - 2*(width/p), height - 2*(height/p), 10, RGB565_PURPLE);
    gfx->setCursor(x + width/p , y+height/p + 5);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->print("Keyboard");
    gfx->setCursor(x + width/p, y+height/p + 16+5);
    gfx->print("Test");




}

void registerKeyboardTestApplication() {
    static bool registered = false;
    if (registered) {
        return;
    }

    System& system = System::getInstance();
    system.addApplication(new KeyboardTestApplication(), nullptr);
    ApplicationFolder* utilitiesFolder = new ApplicationFolder("Utilities");
    system.addApplication(new KeyboardTestApplication(), utilitiesFolder);
    system.addApplicationFolder(utilitiesFolder);
    registered = true;
}