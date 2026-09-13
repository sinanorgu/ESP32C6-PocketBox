#include "Application.hpp"
#include "System.hpp"
#include "ListMenu.hpp"
#include "Definitions.hpp"
#include "TextBox.hpp"

class SettingsApplication : public Application {
    public:
        SettingsApplication();
        void run() override;
    void drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const override;
};

SettingsApplication::SettingsApplication(){
    name = "Settings";
}


void connectToWiFiCallback(void* params) {
    delay(1000);
    char* ssid = static_cast<char*>(params);
    Serial.printf("Connecting to Wi-Fi network: %s\n", ssid);
    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;
    System::getInstance().gfx->fillRect(menuX, menuY, menuWidth, menuHeight, COLOR_BACKGROUND);
    System::getInstance().gfx->setCursor(menuX, menuY);
    System::getInstance().gfx->setTextSize(2);
    System::getInstance().gfx->setTextColor(RGB565_WHITE);
    System::getInstance().gfx->printf("Password for %s: ", ssid);
    
    int16_t passwordBoxX = menuX;
    int16_t passwordBoxY = System::getInstance().gfx->getCursorY() + 16; // Position the password box below the prompt
    int16_t passwordBoxWidth = menuWidth - 10;
    int16_t passwordBoxHeight = 30;
    TextBox<32> passwordBox(passwordBoxX, passwordBoxY, passwordBoxWidth, passwordBoxHeight, RGB565_WHITE, COLOR_BACKGROUND);


        
    Event event;
    
    while (true) {

        event = System::getInstance().systemEventQueue->pop(event)? event : Event(); 
        
        if(event.type == EventType::TextInput) {        
            //System::getInstance().gfx->print((char)event.event.keyboard.character);
            passwordBox.draw(System::getInstance().gfx, event);
        } 

        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            delay(200);
            // Attempt to connect to Wi-Fi using the entered password
            char * password = passwordBox.text;
            bool connected = System::getInstance().wifiManager.connectToWiFi(ssid, password);
            if (connected) {
                Serial.println("Connected to Wi-Fi successfully!");
                System::getInstance().setWifiConnectionStatus(WifiConnectionState::Connected);
                System::getInstance().wifiManager.saveNetwork(ssid, password, true, false, 100);
                System::getInstance().gfx->setCursor(menuX, menuY+36);
                System::getInstance().gfx->printf("\nConnected to %s", ssid);
                System::getInstance().sshManager->begin(SSH_USERNAME, SSH_PASSWORD, SSH_KEY_FILE_PATH, SSH_PORT);
            } else {
                Serial.println("Failed to connect to Wi-Fi.");  
                System::getInstance().gfx->printf("\nFailed to connect to %s", ssid);
                Serial.printf("SSID: %s, Password entered: %s\n", ssid, password);
    
            }
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }

    }

    
}


struct KnownNetworkMenuContext {
    String ssid;
    bool deleted = false;
};

void connectToKnownWiFi(void* params){
    KnownNetworkMenuContext* context = static_cast<KnownNetworkMenuContext*>(params);
    const char* ssid = context->ssid.c_str();
    bool is_connected = System::getInstance().wifiManager.connectToKnownWiFi(ssid);
    if(is_connected){
        Serial.printf("Connected to known Wi-Fi network: %s\n", ssid);
        System::getInstance().setWifiConnectionStatus(WifiConnectionState::Connected);
        System::getInstance().sshManager->begin(SSH_USERNAME, SSH_PASSWORD, SSH_KEY_FILE_PATH, SSH_PORT);
    } else {
        Serial.printf("Failed to connect to known Wi-Fi network: %s\n", ssid);
    }
}

void deleteKnownWiFi(void* params){
    KnownNetworkMenuContext* context = static_cast<KnownNetworkMenuContext*>(params);
    const char* ssid = context->ssid.c_str();
    if(System::getInstance().wifiManager.removeNetwork(ssid)){
        context->deleted = true;
        Serial.printf("Deleted known Wi-Fi network: %s\n", ssid);
    } else {
        Serial.printf("Failed to delete known Wi-Fi network: %s\n", ssid);
    }
}

void connectToKnonwnWiFiCallback(void* params){
    delay(200);
    KnownNetworkMenuContext context{static_cast<const char*>(params)};
    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;
    ListMenu networkMenu;
    networkMenu.setGraphics(menuX, menuY, menuWidth, menuHeight);
    networkMenu.setHeader(context.ssid);
    networkMenu.addtoList("Connect", connectToKnownWiFi, &context);
    networkMenu.addtoList("Delete", deleteKnownWiFi, &context);

    while (true) {
        networkMenu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            networkMenu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            networkMenu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            networkMenu.runSelectedItem();
            delay(200);
            networkMenu.changed = true;
            if(context.deleted){
                break;
            }
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break;
        }
    }
}

void scanNetworkCallback(void* params) {
    Serial.println("WiFi Settings selected");
    std::vector<String> networks = System::getInstance().wifiManager.getAvailableNetworks();
    ListMenu wifiMenu;
    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;
    wifiMenu.setGraphics(menuX, menuY, menuWidth, menuHeight);  
    

    for (const auto& network : networks) {
        wifiMenu.addtoList(network, connectToWiFiCallback, (void*)network.c_str());
    }
        
    delay(200);
    while (true) {
        wifiMenu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            wifiMenu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            wifiMenu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            wifiMenu.runSelectedItem();
            delay(200);
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }
    }
}

void knownNetworksCallback(void* params) {
    Serial.println("Known Networks selected");
    std::vector<WifiNetwork> knownNetworks = System::getInstance().wifiManager.getKnownNetworks();
    
    ListMenu knownNetworksMenu;
    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;
    knownNetworksMenu.setGraphics(menuX, menuY, menuWidth, menuHeight);  
    
    for (const auto& network : knownNetworks) {
        knownNetworksMenu.addtoList(network.ssid, connectToKnonwnWiFiCallback, (void*)network.ssid.c_str());
    }
        
    delay(200);
    while (true) {
        knownNetworksMenu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            knownNetworksMenu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            knownNetworksMenu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            knownNetworksMenu.runSelectedItem();
            delay(200);
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }
    }
}

void exampleItemCallback(void* params) {
    char* itemName = static_cast<char*>(params);
    Serial.printf("Selected item: %s\n", itemName);

}

void exampleCallback(void *params) {

    ListMenu menu;

    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;

    char namelist[10][10] = {"Item 1", "Item 2", "Item 3", "Item 4", "Item 5", "Item 6", "Item 7", "Item 8", "Item 9", "Item 10"};
    menu.setGraphics(menuX, menuY, menuWidth, menuHeight);

    for(int i = 0; i < 10; i++){
        menu.addtoList(namelist[i], exampleItemCallback, &namelist[i]);
    }

    delay(200);
    while (true) {
        menu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            menu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            menu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            menu.runSelectedItem();
            delay(200);
            menu.changed = true; // Mark the menu as changed to redraw after returning from the callback
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }

    }
}


void wifiSettingsCallback(void* params) {
    ListMenu wifiMenu;

    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;

    wifiMenu.setGraphics(menuX, menuY, menuWidth, menuHeight);

    wifiMenu.addtoList("Known Networks", knownNetworksCallback);
    wifiMenu.addtoList("Scan Networks", scanNetworkCallback);

    delay(300);
    while (true) {
        wifiMenu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            wifiMenu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            wifiMenu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            wifiMenu.runSelectedItem();
            delay(200);
            wifiMenu.changed = true; // Mark the menu as changed to redraw after returning from the callback
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }

    }
}


void SettingsApplication::run() {
    Serial.println("Settings application opened");
    ListMenu menu;

    int16_t menuX = 0;
    int16_t menuY = System::getInstance().interface.infoPanelHeight + System::getInstance().interface.margin;
    int16_t menuWidth = TFT_HEIGHT;
    int16_t menuHeight = TFT_WIDTH - menuY;

    menu.setGraphics(menuX, menuY, menuWidth, menuHeight);

    menu.addtoList("WiFi Settings", wifiSettingsCallback);

    menu.addtoList("Display Settings", nullptr);
    menu.addtoList("System Info", nullptr);
    menu.addtoList("System Info1", nullptr);
    menu.addtoList("System Info2", nullptr);
    menu.addtoList("Example", exampleCallback);

    

    
    
    while (true) {
        menu.draw();
        if(digitalRead(BUTTON_DOWN_PIN) == LOW){
            menu.incrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_UP_PIN) == LOW){
            menu.decrementIndex();
            delay(200);
        }
        if(digitalRead(BUTTON_RIGHT_PIN) == LOW){
            menu.runSelectedItem();
            delay(200);
            menu.changed = true; // Mark the menu as changed to redraw after returning from the callback
        }
        if(digitalRead(BUTTON_LEFT_PIN) == LOW){
            delay(200);
            break; // Exit the settings application
        }

    }
    
}

void SettingsApplication::drawIcon(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t width, int16_t height) const {
    if (gfx == nullptr) {
        return;
    }

    const int16_t centerX = x + width / 2;
    const int16_t centerY = y + height / 2;
    const int16_t radius = min(width, height) / 3;
    const int16_t halfHeight =
        static_cast<int16_t>(radius * 0.8660254f);

    const uint16_t iconColor = RGB565(200, 250, 250);

    const int16_t px[6] = {
        static_cast<int16_t>(centerX - radius),
        static_cast<int16_t>(centerX - radius / 2),
        static_cast<int16_t>(centerX + radius / 2),
        static_cast<int16_t>(centerX + radius),
        static_cast<int16_t>(centerX + radius / 2),
        static_cast<int16_t>(centerX - radius / 2)
    };

    const int16_t py[6] = {
        centerY,
        static_cast<int16_t>(centerY - halfHeight),
        static_cast<int16_t>(centerY - halfHeight),
        centerY,
        static_cast<int16_t>(centerY + halfHeight),
        static_cast<int16_t>(centerY + halfHeight)
    };

    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t next = (i + 1) % 6;

        gfx->fillTriangle(
            centerX,
            centerY,
            px[i],
            py[i],
            px[next],
            py[next],
            iconColor
        );
    }

    gfx->fillCircle(
        centerX,
        centerY,
        radius / 3,
        RGB565_BLACK
    );

}

void registerSettingsApplication() {
    static bool registered = false;
    if (registered) {
        return;
    }

    System& system = System::getInstance();
    system.addApplication(new SettingsApplication(), nullptr);
    ApplicationFolder* utilitiesFolder = new ApplicationFolder("Utilities");
    system.addApplication(new SettingsApplication(), utilitiesFolder);
    system.addApplicationFolder(utilitiesFolder);
    registered = true;
}