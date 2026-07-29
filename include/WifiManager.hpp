#pragma once 
#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <ArduinoJson.h>
#include "SdCardManager.hpp"

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

class WifiNetwork
{
public:
    String ssid;
    String password;
    bool autoConnect;
    bool hidden;
    int priority;

    WifiNetwork() = default;

    WifiNetwork(
        const String& ssid,
        const String& password,
        bool autoConnect,
        bool hidden,
        int priority)
        : ssid(ssid),
          password(password),
          autoConnect(autoConnect),
          hidden(hidden),
          priority(priority)
    {
    }
};


class WifiManager
{
public:
    WifiManager(){
        WiFi.onEvent(WifiManager::onWiFiEvent);
    };

    static void onWiFiEvent(arduino_event_id_t event);
    void scanWifiNetworks();

    bool connectToWiFi(
        const char* ssid,
        const char* password);
    bool connectToKnownWiFi(const char* ssid);

    bool isConnected() const;


    std::vector<String>& getAvailableNetworks();

    bool loadKnownNetworks();

    bool saveNetwork(
        const char* ssid,
        const char* password,
        bool autoConnect,
        bool hidden,
        int priority);

    bool removeNetwork(const char* ssid);

    const std::vector<WifiNetwork>& getKnownNetworks() const;

private:
    bool saveKnownNetworks();

    WifiNetwork* findKnownNetwork(const char* ssid);

private:
    String SSID;
    String password;
    bool connected = false;

    std::vector<String> availableNetworks;
    std::vector<WifiNetwork> knownNetworks;
};

