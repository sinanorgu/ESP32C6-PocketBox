#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "Application.hpp"
#include "Event.hpp"
#include "Definitions.hpp"
#include "WifiManager.hpp"


class Interface {
public:
    int index;
    int offset;
    int innerIndex;
    int infoPanelHeight;
    int margin = 5;
    bool changed;
    
    Interface() : index(0), offset(0), innerIndex(0), infoPanelHeight(20), changed(true) {}

    void drawMenu(Arduino_GFX* gfx, int16_t x, int16_t y) const;
    
    void drawInfoPanel(Arduino_GFX* gfx, int16_t x, int16_t y) const;

    void draw(Arduino_GFX* gfx){
        if(gfx == nullptr || changed == false){
            return;
        }
        gfx->fillScreen(COLOR_BACKGROUND);
        drawInfoPanel(gfx, 0, 0);
        drawMenu(gfx, 0, infoPanelHeight);
        changed = false;
    }

    void incrementIndex();
    void decrementIndex();
    void runApp();
};


class System{
    public:
        Interface interface;
        EventQueue<32> systemEventQueue;
        WifiManager wifiManager;
        Arduino_GFX *gfx;

    public:
        bool addApplication(Application* app, ApplicationFolder* folder);
        bool addApplicationFolder(ApplicationFolder* folder, ApplicationFolder* parentFolder = nullptr);
        static System& getInstance(){
            static System instance;
            return instance;
        }
        std::vector<ApplicationFolder*> childApplicationFolders;
        ApplicationFolder * rootApplicationFolder;

    private:
        System();
        System(const System&) = delete;
        System& operator=(const System&) = delete;
};





