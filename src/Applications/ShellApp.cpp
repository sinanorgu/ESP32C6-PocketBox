#include "Application.hpp"
#include "System.hpp"
#include "ListMenu.hpp"
#include "Definitions.hpp"
#include "Event.hpp"
#include "TextBox.hpp"
#include "Shell.hpp"

namespace {
class ScreenShellOutput : public ShellOutput {
public:
    explicit ScreenShellOutput(Arduino_GFX* gfx) : gfx(gfx) {}
    void write(const char* text) override {
        if (firstWrite) {
            int top = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
            gfx->fillRect(0, top, gfx->width(), gfx->height() - top, COLOR_BACKGROUND);
            gfx->setCursor(5, top + 5);
            gfx->setTextSize(1);
            gfx->setTextColor(RGB565_WHITE);
            firstWrite = false;
        }
        gfx->print(text);
    }
    void clear() override { gfx->fillScreen(COLOR_BACKGROUND); }
private:
    Arduino_GFX* gfx;
    bool firstWrite = true;
};
}

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

        event = System::getInstance().nextEvent();
        delay(5);
        
        if(event.type == EventType::TextInput) {        
            //System::getInstance().gfx->print((char)event.event.keyboard.character);
            if(event.event.keyboard.character == 0x0A) { //Newline
                // Other on-device shell commands are still a UI prototype.
                if (strcmp(txtbox.text, "screenshot") == 0) {
                    ScreenShellOutput output(System::getInstance().gfx);
                    shell.executeCommand(txtbox.text, output);
                }
                txtbox.clearText();

                int cursorX = 0;
                int cursorY = System::getInstance().gfx->getCursorY();
                System::getInstance().gfx->setCursor(cursorX, cursorY + 16);
                System::getInstance().gfx->setTextColor(RGB565_BLUE);
                System::getInstance().gfx->setTextSize(2);
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

        if(buttonAction(event, ButtonCode::Left)){
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
