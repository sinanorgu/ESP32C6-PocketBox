#pragma once 
#include <Arduino.h>
#include <WiFi.h>

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

class WifiManager {
    public:
        String SSID;
        String password;
        bool connected;

    public:
        WifiManager() = default;
        void scanWifiNetworks();
        bool connectToWiFi(const char* ssid, const char* password);
        //void disconnectFromWiFi();
        //bool isConnected() const;
        //String getConnectedSSID() const;
};

