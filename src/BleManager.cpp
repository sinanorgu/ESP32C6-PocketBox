#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BleManager.hpp"
#include "System.hpp"
#include "Event.hpp"

BLECharacteristic *keyboardCharacteristic = nullptr;       // PPG


bool deviceConnected = false;

// ---------------- CONNECTION CALLBACKS ----------------

void MyServerCallbacks::onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("📲 Telefon bağlandı");
    System::getInstance().interface.changed = true;
    System::getInstance().setBleConnectionStatus(true);
}

void MyServerCallbacks::onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("📴 Telefon bağlantısı kesildi");
    System::getInstance().setBleConnectionStatus(false);
    pServer->getAdvertising()->start();
}



void printCodepoint(uint32_t cp)
{
    char utf8[5] = {0};

    if (cp <= 0x7F) {
        utf8[0] = cp;
    }
    else if (cp <= 0x7FF) {
        utf8[0] = 0xC0 | (cp >> 6);
        utf8[1] = 0x80 | (cp & 0x3F);
    }
    else if (cp <= 0xFFFF) {
        utf8[0] = 0xE0 | (cp >> 12);
        utf8[1] = 0x80 | ((cp >> 6) & 0x3F);
        utf8[2] = 0x80 | (cp & 0x3F);
    }
    else if (cp <= 0x10FFFF) {
        utf8[0] = 0xF0 | (cp >> 18);
        utf8[1] = 0x80 | ((cp >> 12) & 0x3F);
        utf8[2] = 0x80 | ((cp >> 6) & 0x3F);
        utf8[3] = 0x80 | (cp & 0x3F);
    }

    Serial.printf("%s\n", utf8);
}

void MyCharacteristicCallbacks::onWrite(BLECharacteristic *characteristic) {

    
    // String rx = characteristic->getValue().c_str();  // Arduino String
    // Serial.print("📩 Veri alındı: ");
    // Serial.print(rx);
    // Serial.print(" (");
    // Serial.printf("%d", rx);
    // Serial.println(")");
    
    // uint8_t *data = characteristic->getData();
    // Serial.print("📩 Veri alındı: ");
    // Serial.printf("0x%02X, char: %c\n", data[0], data[0]);

    uint32_t * value = reinterpret_cast<uint32_t *>(characteristic->getData());  // std::string



    Serial.printf("Unicode: U+%04lX, \n", (unsigned long)*value);
    printCodepoint(*value);

    if (*value == 0x08) {
        // Backspace
    } else if (*value == 0x0A) {
        // Enter
    } else {
        // Unicode karakterini işle
    }

    Event event;
    event.type = EventType::TextInput;
    event.timestamp = millis();
    event.event.keyboard.character = static_cast<uint32_t>(*value);
    System::getInstance().systemEventQueue->push(event);
}




// ---------------- BLE INIT ----------------
void BLE_init(){

    BLEDevice::init("ESP32C6 PocketBox");

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(KEYBOARD_SERVICE_UUID);

    keyboardCharacteristic = pService->createCharacteristic(
        KEYBOARD_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );

    
    keyboardCharacteristic->addDescriptor(new BLE2902());
    keyboardCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    pService->start();
    pServer->getAdvertising()->start();

    Serial.println("📡 BLE Yayını Başladı");
}





