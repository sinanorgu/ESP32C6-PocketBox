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
    constexpr int16_t iconSize = 100;
    int appCount = System::getInstance().rootApplicationFolder->getApplicationCount();
    const bool redrawAll = !menuRendered || renderedOffset != offset;

    if(redrawAll){
        gfx->fillRect(x, y, TFT_HEIGHT, TFT_WIDTH - y, COLOR_BACKGROUND);

        for(int i = 0; i < 3; i++){
            const int appIndex = offset + i;
            if(appIndex < appCount){
                const int iconX = x + iconSize * i + margin * (i+1);
                const int iconY = y + 20;
                Application* app = System::getInstance().rootApplicationFolder->getApplication(appIndex);
                if(app != nullptr){
                    app->drawIcon(gfx, iconX, iconY, iconSize, iconSize);
                }
            }
        }

        drawSelectionFrame(index - offset, x, y, iconSize, RGB565_CYAN);
        return;
    }

    // The icons are unchanged while the selection moves within the window.
    drawSelectionFrame(renderedIndex - renderedOffset, x, y, iconSize, COLOR_BACKGROUND);
    drawSelectionFrame(index - offset, x, y, iconSize, RGB565_CYAN);
}

void Interface::drawSelectionFrame(
    int slot,
    int16_t x,
    int16_t y,
    int16_t iconSize,
    uint16_t color) const
{
    if(slot < 0 || slot >= 3){
        return;
    }

    const int16_t iconX = x + iconSize * slot + margin * (slot + 1);
    const int16_t iconY = y + 20;
    const int16_t frameX = iconX - 3;
    const int16_t frameY = iconY - 3;
    const int16_t frameWidth = iconSize + 6;
    const int16_t frameHeight = iconSize + 6;

    if(frameX >= 0 && frameY >= 0 &&
       frameX + frameWidth <= TFT_HEIGHT &&
         frameY + frameHeight <= TFT_WIDTH){
        gfx->drawRoundRect(
            frameX,
            frameY,
            frameWidth,
            frameHeight,
            8,
            color);
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
        menuChanged = true;
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
        menuChanged = true;
    }
}

void Interface::runApp() {
    Application* app = System::getInstance().rootApplicationFolder->getApplication(index);
    if (app != nullptr) {
        app->run();
        invalidate();
    }
}

void Interface::invalidate()
{
    changed = true;
    fullRedraw = true;
    menuChanged = true;
    menuRendered = false;
}

void Interface::drawInfoPanel(int16_t x, int16_t y){
    if (!infoPanelVisible || gfx == nullptr) return;

    gfx->fillRect(x, y, TFT_HEIGHT, infoPanelHeight, RGB565_DARKGREY);
    gfx->drawRect(x, y, TFT_HEIGHT, infoPanelHeight, RGB565_WHITE);
    
    WifiConnectionState wifiStatus = System::getInstance().wifiStatus;
    bool isBleConnected = System::getInstance().isBleConnected; 
    bool isSDCardInserted = System::getInstance().isSDCardInserted;
    bool isSshBegin = System::getInstance().isSshBegin;

    // Draw the Wi-Fi icon
    {
        int start_angle = -140, end_angle = -40; 
        int color = RGB565(255, 0, 0);
        if (wifiStatus == WifiConnectionState::Connected)
        {
            color = RGB565(0, 255, 0);
        }
        else if (wifiStatus == WifiConnectionState::Scanning ||
                 wifiStatus == WifiConnectionState::Connecting)
        {
            color = RGB565(0, 0, 0);;
        }
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
    
    //draw Sd card icon
    {
        int centerX = 70;
        int centerY = infoPanelHeight/2;
        int color = isSDCardInserted ? RGB565(0, 255, 0) : RGB565(255, 0, 0);
        int sizeX = 10, sizeY = 8;
        gfx->drawRect(centerX-sizeX, centerY - sizeY, sizeX*2, sizeY*2, color);    
        gfx->setCursor(centerX-sizeX+2, centerY - sizeY + 2);
        gfx->setTextColor(color);
        gfx->setTextSize(1);
        gfx->print("SD");
        dirtyFlags.sdCard = 0;
    }
    
    //draw SSH conncetion icon
    {
        int centerX = 100;
        int centerY = infoPanelHeight/2;
        int color = isSshBegin ? RGB565(0, 255, 0) : RGB565(255, 0, 0);
        int sizeX = 12, sizeY = 8;
        gfx->drawRect(centerX-sizeX, centerY - sizeY, sizeX*2, sizeY*2, color);    
        gfx->setCursor(centerX-sizeX+2, centerY - sizeY + 2);
        gfx->setTextColor(color);
        gfx->setTextSize(1);
        gfx->print("SSH");
        dirtyFlags.ssh = 0;
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

void Interface::drawArrowPanel(int16_t x, int16_t y){
    gfx->fillRect(x, y, TFT_HEIGHT, arrowPanelHeight, RGB565_DARKGREY);
    gfx->drawRect(x, y, TFT_HEIGHT, arrowPanelHeight, RGB565_WHITE);

    // Draw left arrow
    {
        int centerX = 20;
        int centerY = y + arrowPanelHeight / 2;
        int size = 10;
        gfx->drawLine(centerX + size, centerY - size, centerX - size, centerY, RGB565_WHITE);
        gfx->drawLine(centerX + size, centerY + size, centerX - size, centerY, RGB565_WHITE);
        dirtyFlags.menu = 0;
    }

    // Draw right arrow
    {
        int centerX = TFT_HEIGHT - 20;
        int centerY = y + arrowPanelHeight / 2;
        int size = 10;
        gfx->drawLine(centerX - size, centerY - size, centerX + size, centerY, RGB565_WHITE);
        gfx->drawLine(centerX - size, centerY + size, centerX + size, centerY, RGB565_WHITE);
        dirtyFlags.menu = 0;
    }
}
