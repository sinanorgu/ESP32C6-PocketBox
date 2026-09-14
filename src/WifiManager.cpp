#include "WifiManager.hpp"
#include "System.hpp"

const char* encryptionTypeToString(wifi_auth_mode_t encryptionType)
{
    switch (encryptionType) {
        case WIFI_AUTH_OPEN:
            return "OPEN";

        case WIFI_AUTH_WEP:
            return "WEP";

        case WIFI_AUTH_WPA_PSK:
            return "WPA";

        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";

        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";

        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-ENTERPRISE";

        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";

        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3";

        default:
            return "UNKNOWN";
    }
}

bool WifiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}


void WifiManager::scanWifiNetworks()
{
    Serial.println();
    Serial.println("Wi-Fi taramasi baslatiliyor...");

    /*
     * WIFI_STA:
     * ESP32 bir access point'e istemci olarak baglanacak.
     */
    WiFi.mode(WIFI_STA);

    /*
     * Önceden kalmış bağlantıyı kes.
     *
     * İlk false:
     *   Wi-Fi ayarlarını kalıcı bellekten silme.
     *
     * İkinci false:
     *   Wi-Fi radyosunu kapatma.
     */
    WiFi.disconnect(false, false);
    delay(100);

    /*
     * Senkron tarama.
     * Fonksiyon tarama bitene kadar bekler.
     *
     * Parametreler:
     * async       = false
     * show_hidden = true
     */
    const int16_t networkCount = WiFi.scanNetworks(false, true);

    if (networkCount == WIFI_SCAN_FAILED) {
        Serial.println("Wi-Fi taramasi basarisiz.");
        return;
    }

    if (networkCount == 0) {
        Serial.println("Hicbir Wi-Fi agi bulunamadi.");
        WiFi.scanDelete();
        return;
    }

    Serial.printf(
        "%d Wi-Fi agi bulundu:\n\n",
        networkCount
    );

    for (int16_t i = 0; i < networkCount; ++i) {
        const String ssid = WiFi.SSID(i);
        const int32_t rssi = WiFi.RSSI(i);
        const int32_t channel = WiFi.channel(i);
        const wifi_auth_mode_t authMode =
            WiFi.encryptionType(i);

        Serial.printf(
            "[%d] SSID: %-24s RSSI: %4ld dBm "
            "Channel: %2ld Security: %s\n",
            i,
            ssid.c_str(),
            static_cast<long>(rssi),
            static_cast<long>(channel),
            encryptionTypeToString(authMode)
        );
    }

    /*
     * Tarama sonuçları RAM'de tutulur.
     * İşimiz bittikten sonra temizliyoruz.
     */
    WiFi.scanDelete();
}



std::vector<String>& WifiManager::getAvailableNetworks()
{
    if (connectionMutex == nullptr ||
        xSemaphoreTake(connectionMutex, portMAX_DELAY) != pdTRUE)
    {
        return availableNetworks;
    }

    System::getInstance().setWifiConnectionStatus(WifiConnectionState::Scanning);
    availableNetworks.clear();
    WiFi.mode(WIFI_STA);
    const int16_t networkCount = WiFi.scanNetworks(false, true);

    if (networkCount == WIFI_SCAN_FAILED) {
        Serial.println("Wi-Fi taramasi basarisiz.");
        System::getInstance().setWifiConnectionStatus(WifiConnectionState::Disconnected);
        xSemaphoreGive(connectionMutex);
        return availableNetworks;
    }

    if (networkCount == 0) {
        Serial.println("Hicbir Wi-Fi agi bulunamadi.");
        WiFi.scanDelete();
        System::getInstance().setWifiConnectionStatus(WifiConnectionState::Disconnected);
        xSemaphoreGive(connectionMutex);
        return availableNetworks;
    }

    Serial.printf(
        "%d Wi-Fi agi bulundu:\n\n",
        networkCount
    );

    for (int16_t i = 0; i < networkCount; ++i) {
        const String ssid = WiFi.SSID(i);
        availableNetworks.push_back(ssid);
    }
    WiFi.scanDelete();
    System::getInstance().setWifiConnectionStatus(
        WiFi.status() == WL_CONNECTED
            ? WifiConnectionState::Connected
            : WifiConnectionState::Disconnected);
    xSemaphoreGive(connectionMutex);

    return availableNetworks;
}


bool WifiManager::connectToWiFi(
    const char* ssid,
    const char* password
)
{
    if (connectionMutex == nullptr ||
        xSemaphoreTake(connectionMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    const bool result = connectToWiFiLocked(ssid, password);
    xSemaphoreGive(connectionMutex);
    return result;
}

bool WifiManager::connectToWiFiLocked(
    const char* ssid,
    const char* password
)
{
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("Gecersiz SSID.");
        return false;
    }

    Serial.printf("Wi-Fi agina baglaniliyor: %s\n", ssid);
    System::getInstance().setWifiConnectionStatus(WifiConnectionState::Connecting);

    WiFi.mode(WIFI_STA);

    /*
     * Eski bağlantı varsa temiz şekilde kes.
     * Kalıcı credential bilgilerini silmiyoruz.
     */
    WiFi.disconnect(false, false);
    delay(100);

    WiFi.begin(ssid, password);

    const uint32_t startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println();
            Serial.println("Wi-Fi baglanti zaman asimi.");

            WiFi.disconnect(false, false);
            System::getInstance().setWifiConnectionStatus(WifiConnectionState::Disconnected);
            return false;
        }

        Serial.print('.');
        delay(250);
    }

    Serial.println();
    Serial.println("Wi-Fi baglantisi basarili.");

    Serial.printf(
        "SSID       : %s\n",
        WiFi.SSID().c_str()
    );

    Serial.printf(
        "IP address : %s\n",
        WiFi.localIP().toString().c_str()
    );

    Serial.printf(
        "Gateway    : %s\n",
        WiFi.gatewayIP().toString().c_str()
    );

    Serial.printf(
        "DNS        : %s\n",
        WiFi.dnsIP().toString().c_str()
    );

    Serial.printf(
        "RSSI       : %ld dBm\n",
        static_cast<long>(WiFi.RSSI())
    );

    Serial.printf(
        "MAC        : %s\n",
        WiFi.macAddress().c_str()
    );

    System::getInstance().setWifiConnectionStatus(WifiConnectionState::Connected);
    markLastConnectedNetwork(ssid);

    return true;
}

WifiNetwork* WifiManager::findKnownNetwork(const char* ssid)
{
    if (!ssid)
        return nullptr;

    for (WifiNetwork& network : knownNetworks)
    {
        if (network.ssid == ssid)
            return &network;
    }

    return nullptr;
}

const std::vector<WifiNetwork>& WifiManager::getKnownNetworks() const
{
    return knownNetworks;
}

bool WifiManager::loadKnownNetworks()
{
    knownNetworks.clear();

    File file = SD.open(NETWORK_FILE, FILE_READ);

    if (!file)
    {
        Serial.println("Failed to open networks file");
        return false;
    }

    JsonDocument document;

    DeserializationError error =
        deserializeJson(document, file);

    file.close();

    if (error)
    {
        Serial.print("Failed to parse networks file: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonArray networksArray =
        document["networks"].as<JsonArray>();

    if (networksArray.isNull())
    {
        Serial.println("Networks array not found");
        return false;
    }

    for (JsonObject networkObject : networksArray)
    {
        const char* ssid =
            networkObject["ssid"] | "";

        const char* password =
            networkObject["password"] | "";

        bool autoConnect =
            networkObject["autoConnect"] | true;

        bool hidden =
            networkObject["hidden"] | false;

        int priority =
            networkObject["priority"] | 0;

        bool lastConnected =
            networkObject["lastConnected"] | false;

        if (ssid[0] == '\0')
        {
            Serial.println(
                "Skipping network with empty SSID");
            continue;
        }

        knownNetworks.emplace_back(
            ssid,
            password,
            autoConnect,
            hidden,
            priority,
            lastConnected);
    }

    Serial.printf(
        "Loaded %u known networks\n",
        static_cast<unsigned>(knownNetworks.size()));

    return true;
}
bool WifiManager::saveKnownNetworks()
{
    constexpr const char* TEMP_FILE =
        NETWORK_FILE ".tmp";

    JsonDocument document;

    JsonArray networksArray =
        document["networks"].to<JsonArray>();

    for (const WifiNetwork& network : knownNetworks)
    {
        JsonObject networkObject =
            networksArray.add<JsonObject>();

        networkObject["ssid"] =
            network.ssid;

        networkObject["password"] =
            network.password;

        networkObject["autoConnect"] =
            network.autoConnect;

        networkObject["hidden"] =
            network.hidden;

        networkObject["priority"] =
            network.priority;

        networkObject["lastConnected"] =
            network.lastConnected;
    }

    SD.remove(TEMP_FILE);

    File file = SD.open(TEMP_FILE, FILE_WRITE);

    if (!file)
    {
        Serial.println(
            "Failed to create temporary networks file");
        return false;
    }

    size_t bytesWritten =
        serializeJsonPretty(document, file);

    file.write('\n');
    file.close();

    if (bytesWritten == 0)
    {
        Serial.println(
            "Failed to serialize networks file");

        SD.remove(TEMP_FILE);
        return false;
    }

    SD.remove(NETWORK_FILE);

    if (!SD.rename(TEMP_FILE, NETWORK_FILE))
    {
        Serial.println(
            "Failed to replace networks file");

        SD.remove(TEMP_FILE);
        return false;
    }

    return true;
}

bool WifiManager::saveNetwork(
    const char* ssid,
    const char* password,
    bool autoConnect,
    bool hidden,
    int priority)
{
    if (connectionMutex == nullptr ||
        xSemaphoreTake(connectionMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    if (!ssid || ssid[0] == '\0')
    {
        Serial.println("SSID cannot be empty");
        xSemaphoreGive(connectionMutex);
        return false;
    }

    if (!password)
        password = "";

    WifiNetwork* existingNetwork =
        findKnownNetwork(ssid);

    if (existingNetwork)
    {
        existingNetwork->password =
            password;

        existingNetwork->autoConnect =
            autoConnect;

        existingNetwork->hidden =
            hidden;

        existingNetwork->priority =
            priority;
    }
    else
    {
        knownNetworks.emplace_back(
            ssid,
            password,
            autoConnect,
            hidden,
            priority);
    }

    for (WifiNetwork& network : knownNetworks)
    {
        network.lastConnected = network.ssid == ssid;
    }

    if (!saveKnownNetworks())
    {
        Serial.println(
            "Failed to save known networks");
        xSemaphoreGive(connectionMutex);
        return false;
    }

    xSemaphoreGive(connectionMutex);
    return true;
}

bool WifiManager::removeNetwork(const char* ssid)
{
    if (connectionMutex == nullptr ||
        xSemaphoreTake(connectionMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    if (!ssid || ssid[0] == '\0')
    {
        xSemaphoreGive(connectionMutex);
        return false;
    }

    for (auto iterator = knownNetworks.begin();
         iterator != knownNetworks.end();
         ++iterator)
    {
        if (iterator->ssid == ssid)
        {
            knownNetworks.erase(iterator);
            const bool result = saveKnownNetworks();
            xSemaphoreGive(connectionMutex);
            return result;
        }
    }

    xSemaphoreGive(connectionMutex);
    return false;
}


bool WifiManager::connectToKnownWiFi(const char* ssid)
{
    if (connectionMutex == nullptr ||
        xSemaphoreTake(connectionMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    const bool result = connectToKnownWiFiLocked(ssid);
    xSemaphoreGive(connectionMutex);
    return result;
}

bool WifiManager::connectToKnownWiFiLocked(const char* ssid)
{
    WifiNetwork* network = findKnownNetwork(ssid);

    if (!network)
    {
        Serial.printf(
            "No known network found with SSID: %s\n",
            ssid);
        return false;
    }

    return connectToWiFiLocked(
        network->ssid.c_str(),
        network->password.c_str());
}

void WifiManager::markLastConnectedNetwork(const char* ssid)
{
    bool changed = false;

    for (WifiNetwork& network : knownNetworks)
    {
        const bool shouldBeLast = network.ssid == ssid;
        if (network.lastConnected != shouldBeLast)
        {
            network.lastConnected = shouldBeLast;
            changed = true;
        }
    }

    if (changed)
    {
        saveKnownNetworks();
    }
}

void WifiManager::startAutoConnectTask()
{
    if (connectionMutex == nullptr ||
        xSemaphoreTake(connectionMutex, portMAX_DELAY) != pdTRUE)
    {
        return;
    }

    if (!autoConnectTaskRunning)
    {
        autoConnectTaskRunning = true;
        if (xTaskCreate(
                WifiManager::autoConnectTaskEntry,
                "wifi_autoconnect",
                4096,
                this,
                1,
                &autoConnectTaskHandle) != pdPASS)
        {
            autoConnectTaskRunning = false;
            Serial.println("Wi-Fi auto-connect task baslatilamadi.");
        }
    }

    xSemaphoreGive(connectionMutex);
}

void WifiManager::autoConnectTaskEntry(void* parameter)
{
    static_cast<WifiManager*>(parameter)->autoConnectTaskLoop();
    vTaskDelete(nullptr);
}

void WifiManager::autoConnectTaskLoop()
{
    while (true)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            break;
        }

        if (connectionMutex != nullptr &&
            xSemaphoreTake(connectionMutex, portMAX_DELAY) == pdTRUE)
        {
            // A manual connection may have completed while this task slept.
            if (WiFi.status() != WL_CONNECTED)
            {
                WifiNetwork* lastNetwork = nullptr;
                for (WifiNetwork& network : knownNetworks)
                {
                    if (network.lastConnected && network.autoConnect)
                    {
                        lastNetwork = &network;
                        break;
                    }
                }

                if (lastNetwork != nullptr)
                {
                    connectToWiFiLocked(
                        lastNetwork->ssid.c_str(),
                        lastNetwork->password.c_str());
                }
                else
                {
                    System::getInstance().setWifiConnectionStatus(WifiConnectionState::Disconnected);
                }
            }

            xSemaphoreGive(connectionMutex);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_AUTOCONNECT_PERIOD_MS));
    }

    if (connectionMutex != nullptr &&
        xSemaphoreTake(connectionMutex, portMAX_DELAY) == pdTRUE)
    {
        autoConnectTaskRunning = false;
        autoConnectTaskHandle = nullptr;
        xSemaphoreGive(connectionMutex);
    }
}


void WifiManager::onWiFiEvent(arduino_event_id_t event)
{
    switch (event)
    {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("Connected to AP");
            System::getInstance().setWifiConnectionStatus(WifiConnectionState::Connecting);
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("Got IP");
            System::getInstance().setWifiConnectionStatus(WifiConnectionState::Connected);
            // EventManager.publish(WifiConnectedEvent{});
            System::getInstance().sshManager->begin(SSH_USERNAME, SSH_PASSWORD, SSH_KEY_FILE_PATH, SSH_PORT);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("Disconnected");
            // EventManager.publish(WifiDisconnectedEvent{});
            System::getInstance().setWifiConnectionStatus(WifiConnectionState::Disconnected);
            System::getInstance().wifiManager.startAutoConnectTask();
            break;

        default:
            break;
    }
}