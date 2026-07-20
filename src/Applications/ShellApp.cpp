#include "Application.hpp"
#include "System.hpp"
#include "ListMenu.hpp"
#include "Definitions.hpp"
#include "Event.hpp"
#include "TextBox.hpp"
#include "Shell.hpp"

class ShellApplication : public Application {
    public:
        ShellApplication();
        void run() override;
    void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

ShellApplication::ShellApplication(){
    name = "Shell";
}



void ShellApplication::run() {

    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;
    System::getInstance().gfx->fillRect(menuX, menuY, menuWidth, menuHeight, COLOR_BACKGROUND);
    Shell shell;

    System::getInstance().gfx->setCursor(menuX, menuY);
    System::getInstance().gfx->setTextColor(RGB565_BLUE);
    System::getInstance().gfx->setTextSize(2);
    System::getInstance().gfx->print(">>"); // Prompt for shell input
    System::getInstance().gfx->setTextColor(RGB565_WHITE);
    TextBox<256> txtbox(menuX + 24, menuY, menuWidth - 20, menuHeight - 20, RGB565_WHITE, COLOR_BACKGROUND);
    Event event;
    
    while (true) {

        event = System::getInstance().systemEventQueue->pop(event)? event : Event(); 
        
        if(event.type == EventType::TextInput) {        
            //System::getInstance().gfx->print((char)event.event.keyboard.character);
            if(event.event.keyboard.character == 0x0A) { //Newline
                //shell.executeCommand(txtbox.text); 
                txtbox.clearText();



                int cursorX = 0;
                int cursorY = System::getInstance().gfx->getCursorY();
                System::getInstance().gfx->setCursor(cursorX, cursorY + 16);
                System::getInstance().gfx->setTextColor(RGB565_BLUE);
                System::getInstance().gfx->print(">>"); // Prompt for shell input
                System::getInstance().gfx->setTextColor(RGB565_WHITE);
                cursorX = System::getInstance().gfx->getCursorX();
                cursorY = System::getInstance().gfx->getCursorY();
                txtbox.setPosition(cursorX, cursorY);
            } 
            else{
                txtbox.draw(System::getInstance().gfx, event);
            }
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

void ShellApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const {
    if (gfx == nullptr) {
        return;
    }
    int p = 20; 
    gfx->drawRoundRect(x + width/p, y+height/p, width - 2*(width/p), height - 2*(height/p), 10, RGB565_GREEN);
    gfx->setCursor(x + 10 , y+height/2-8);
    gfx->setTextColor(RGB565_GREEN);
    gfx->setTextSize(2);
    gfx->print(">Shell");
    




}

void registerShellApplication() {
    static bool registered = false;
    if (registered) {
        return;
    }

    System& system = System::getInstance();
    system.addApplication(new ShellApplication(), nullptr);
    ApplicationFolder* utilitiesFolder = new ApplicationFolder("Utilities");
    system.addApplication(new ShellApplication(), utilitiesFolder);
    system.addApplicationFolder(utilitiesFolder);
    registered = true;
}