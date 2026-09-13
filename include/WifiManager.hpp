#pragma once 
#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <ArduinoJson.h>
#include "SdCardManager.hpp"

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t WIFI_AUTOCONNECT_PERIOD_MS = 30000;

enum class WifiConnectionState : uint8_t {
    Disconnected,
    Scanning,
    Connecting,
    Connected
};

class WifiNetwork
{
public:
    String ssid;
    String password;
    bool autoConnect;
    bool hidden;
    int priority;
    bool lastConnected;

    WifiNetwork() = default;

    WifiNetwork(
        const String& ssid,
        const String& password,
        bool autoConnect,
        bool hidden,
        int priority,
        bool lastConnected = false)
        : ssid(ssid),
          password(password),
          autoConnect(autoConnect),
          hidden(hidden),
          priority(priority),
          lastConnected(lastConnected)
    {
    }
};


class WifiManager
{
public:
    WifiManager(){
        connectionMutex = xSemaphoreCreateMutex();
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
    void startAutoConnectTask();

    bool saveNetwork(
        const char* ssid,
        const char* password,
        bool autoConnect,
        bool hidden,
        int priority);

    bool removeNetwork(const char* ssid);

    const std::vector<WifiNetwork>& getKnownNetworks() const;

private:
    static void autoConnectTaskEntry(void* parameter);
    void autoConnectTaskLoop();
    bool connectToWiFiLocked(const char* ssid, const char* password);
    bool connectToKnownWiFiLocked(const char* ssid);
    void markLastConnectedNetwork(const char* ssid);
    bool saveKnownNetworks();

    WifiNetwork* findKnownNetwork(const char* ssid);

private:
    String SSID;
    String password;
    bool connected = false;
    SemaphoreHandle_t connectionMutex = nullptr;
    TaskHandle_t autoConnectTaskHandle = nullptr;
    bool autoConnectTaskRunning = false;

    std::vector<String> availableNetworks;
    std::vector<WifiNetwork> knownNetworks;
};

