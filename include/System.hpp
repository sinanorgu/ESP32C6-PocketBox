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
        Arduino_GFX *gfx;
    
    private:

        bool isBleConnected = false;
        bool isWifiConnected = false;
        struct {
            uint16_t ble:1, 
                    wifi:1, 
                    time:1,
                    menu:1;
        } dirtyFlags;

    public:    
        Interface() : index(0), offset(0), innerIndex(0), infoPanelHeight(20), changed(true), dirtyFlags({1,1,1,1}) {}

        void drawMenu(int16_t x, int16_t y) const;
        
        void drawInfoPanel(int16_t x, int16_t y);

        void draw(){
            if(gfx == nullptr || changed == false){
                return;
            }
            dirtyFlags.ble = 1;
            dirtyFlags.wifi = 1;
            dirtyFlags.time = 1;
            dirtyFlags.menu = 1;
            gfx->fillScreen(COLOR_BACKGROUND);
            drawInfoPanel(0, 0);
            drawMenu(0, infoPanelHeight);
            changed = false;
        }

        void incrementIndex();
        void decrementIndex();
        void runApp();
        void setBleConnectionStatus(bool status) {
            isBleConnected = status;
            dirtyFlags.ble = 1;
            drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }
        void setWifiConnectionStatus(bool status) {
            isWifiConnected = status;
            dirtyFlags.wifi = 1;
            drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }
};


class System{
    public:
        Interface interface;
        EventQueue<32> * systemEventQueue;
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
    
        void setGFX(Arduino_GFX *gfx) {
            this->gfx = gfx;
            interface.gfx = gfx;
        }

    private:
        System();
        System(const System&) = delete;
        System& operator=(const System&) = delete;
};





