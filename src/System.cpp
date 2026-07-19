#include "System.hpp"


System::System() {
    rootApplicationFolder = new ApplicationFolder("Root");
    systemEventQueue = new EventQueue<32>();
}

bool System::addApplication(Application* app, ApplicationFolder* folder)
{
    if(folder == nullptr){
        rootApplicationFolder->addApplication(app);
        return true;
    } else {
        folder->addApplication(app);
        return true;
    }
}

bool System::addApplicationFolder(ApplicationFolder* folder, ApplicationFolder* parentFolder)
{
    if (folder == nullptr) {
        return false;
    }

    if (parentFolder == nullptr) {
        childApplicationFolders.push_back(folder);
        return true;
    }

    parentFolder->addFolder(folder);
    return true;
}





void Interface::drawMenu(int16_t x, int16_t y) const{
    int iconSize = 100;
    int appCount = System::getInstance().rootApplicationFolder->getApplicationCount();
    for(int i = 0; i < 3; i++){
        int appIndex = offset  + i;

        if(appIndex < appCount){
            if(index == appIndex){
                int iconX = x + iconSize * i + margin * (i+1);
                int iconY = y + 20;
                gfx->drawRoundRect(iconX-3, iconY-3, iconSize+6, iconSize+6, 8, RGB565_CYAN);
            }

            Application* app = System::getInstance().rootApplicationFolder->getApplication(appIndex);
            if(app != nullptr){
                int iconX = x + iconSize * i + margin * (i+1);
                int iconY = y + 20;
                app->drawIcon(gfx, iconX, iconY, iconSize, iconSize);
            }
            
        }
        
    }
}


void Interface::incrementIndex() {
    int appCount = System::getInstance().rootApplicationFolder->getApplicationCount();
    if (index < appCount - 1) {
        index++;
        if (innerIndex < 2) {
            innerIndex++;
        } else {
            offset++;
        }
        changed = true;
    }
}
void Interface::decrementIndex() {
    int appCount = System::getInstance().rootApplicationFolder->getApplicationCount();
    if (index > 0) {
        index--;
        if (innerIndex > 0) {
            innerIndex--;
        } else {
            offset--;
        }
        changed = true;
    }
}

void Interface::runApp() {
    Application* app = System::getInstance().rootApplicationFolder->getApplication(index);
    if (app != nullptr) {
        app->run();
    }
}

void Interface::drawInfoPanel(int16_t x, int16_t y){
    gfx->fillRect(x, y, TFT_HEIGHT, infoPanelHeight, RGB565_DARKGREY);
    gfx->drawRect(x, y, TFT_HEIGHT, infoPanelHeight, RGB565_WHITE);
    

    // Draw the Wi-Fi icon
    {
        int start_angle = -140, end_angle = -40; 
        int color = isWifiConnected ? RGB565(0, 255, 0) : RGB565(255, 0, 0);
        int offsetX = 20, offsetY = infoPanelHeight-3;
        gfx->fillArc(x+offsetX, y+offsetY, 15, 13, start_angle, end_angle, color);
        gfx->fillArc(x+offsetX, y+offsetY, 10, 8,  start_angle, end_angle, color);
        gfx->fillArc(x+offsetX, y+offsetY, 5, 4,  start_angle, end_angle, color);
        gfx->fillCircle(x+offsetX, y+offsetY, 1, color);
        dirtyFlags.wifi = 0;
    }

    //draw bluettooth icon
    {
        int centerX = 45;
        int centerY = infoPanelHeight/2;
        int color = isBleConnected ? RGB565(0, 255, 0) : RGB565(255, 0, 0);
        int sizeX = 5, sizeY = 4;
        gfx->drawLine(centerX-sizeX, centerY - sizeY, centerX+sizeX, centerY + sizeY, color);    
        gfx->drawLine(centerX-sizeX, centerY + sizeY, centerX+sizeX, centerY - sizeY, color);      
        gfx->drawLine(centerX+sizeX, centerY + sizeY, centerX, centerY + sizeY*2, color); 
        gfx->drawLine(centerX+sizeX, centerY - sizeY, centerX, centerY - sizeY*2, color);
        gfx->drawFastVLine(centerX, centerY - sizeY*2, sizeY*4, color); 
        dirtyFlags.ble = 0;
    }

    //Draw time:
    {
        int offsetX = 130;
        int offsetY = 3;
        gfx->setCursor(x+offsetX, y+offsetY);
        gfx->setTextColor(RGB565(255, 255, 255));
        gfx->setTextSize(2);
        gfx->print("12:34");
    }
}
