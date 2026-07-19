#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


#define KEYBOARD_SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define KEYBOARD_CHARACTERISTIC_UUID "abcd1234-1234-1234-1234-abcdef123456"


extern BLECharacteristic *keyboardCharacteristic; // PPG
extern bool deviceConnected;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) override;
    void onDisconnect(BLEServer *pServer) override;
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) override;
};



void BLE_init();
