#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "Application.hpp"
#include "Event.hpp"
#include "Definitions.hpp"
#include "WifiManager.hpp"
#include "SdCardManager.hpp"
#include "SshManager.hpp"


class Interface {
    public:
        int index;
        int offset;
        int innerIndex;
        int infoPanelHeight;
        int arrowPanelHeight;
        int margin = 5;
        bool changed;
        Arduino_GFX *gfx;
    
    private:

        struct {
            uint16_t ble:1, 
                    wifi:1, 
                    time:1,
                    menu:1,
                    sdCard:1,
                    ssh:1;
        } dirtyFlags;

        bool fullRedraw;
        bool infoPanelVisible = true;
        bool menuChanged;
        bool menuRendered;
        int renderedIndex;
        int renderedOffset;

    public:    
        Interface() : index(0), offset(0), innerIndex(0), infoPanelHeight(20), arrowPanelHeight(20), changed(true), fullRedraw(true), menuChanged(true), menuRendered(false), renderedIndex(0), renderedOffset(0), dirtyFlags({1,1,1,1,1,1}) {}

        void drawMenu(int16_t x, int16_t y) const;
        void drawSelectionFrame(int slot, int16_t x, int16_t y, int16_t iconSize, uint16_t color) const;
        
        void drawInfoPanel(int16_t x, int16_t y);
        void setInfoPanelVisible(bool visible) {
            infoPanelVisible = visible;
            if (visible && gfx != nullptr) drawInfoPanel(0, 0);
        }

        void drawArrowPanel(int16_t x, int16_t y);

        void draw(){
            if(gfx == nullptr || (!changed && !fullRedraw && !menuChanged)){
                return;
            }

            if(fullRedraw){
                gfx->fillScreen(COLOR_BACKGROUND);
                drawInfoPanel(0, 0);
                drawMenu(0, infoPanelHeight);
                fullRedraw = false;
                menuChanged = false;
                renderedIndex = index;
                renderedOffset = offset;
                menuRendered = true;
            } else if(menuChanged){
                drawMenu(0, infoPanelHeight);
                renderedIndex = index;
                renderedOffset = offset;
                menuChanged = false;
                menuRendered = true;
            }

            changed = false;
        }

        void incrementIndex();
        void decrementIndex();
        void runApp();
        void invalidate();
        void setBleConnectionStatus(bool status) {
            dirtyFlags.ble = 1;
            drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }
};


class System{
    public:
        Interface interface;
        EventQueue<32> * systemEventQueue;
        WifiManager wifiManager;
        SdCardManager sdCardManager;
        SSHManager *sshManager;
        Arduino_GFX *gfx;

        bool isSDCardInserted = false;
        WifiConnectionState wifiStatus = WifiConnectionState::Disconnected;
        bool isBleConnected = false;
        bool isSshBegin = false;

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
        void setSshManager(SSHManager *sshManager) {
            this->sshManager = sshManager;
        }

        void setBleConnectionStatus(bool status) {
            isBleConnected = status;
            interface.drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }
        void setWifiConnectionStatus(WifiConnectionState status) {
            wifiStatus = status;
            interface.drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }
        void setSdCardStatus(bool status) {
            isSDCardInserted = status;
            interface.drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }

        void setSshStatus(bool status) {
            isSshBegin = status;
            interface.drawInfoPanel(0, 0); // Redraw the info panel to reflect the change
        }

    private:
        System();
        System(const System&) = delete;
        System& operator=(const System&) = delete;
};




