#include "WifiManager.hpp"

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
    availableNetworks.clear();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(100);
    const int16_t networkCount = WiFi.scanNetworks(false, true);

    if (networkCount == WIFI_SCAN_FAILED) {
        Serial.println("Wi-Fi taramasi basarisiz.");
        return availableNetworks;
    }

    if (networkCount == 0) {
        Serial.println("Hicbir Wi-Fi agi bulunamadi.");
        WiFi.scanDelete();
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

    return availableNetworks;
}


bool WifiManager::connectToWiFi(
    const char* ssid,
    const char* password
)
{
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("Gecersiz SSID.");
        return false;
    }

    Serial.printf("Wi-Fi agina baglaniliyor: %s\n", ssid);

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
            priority);
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
    if (!ssid || ssid[0] == '\0')
    {
        Serial.println("SSID cannot be empty");
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

    if (!saveKnownNetworks())
    {
        Serial.println(
            "Failed to save known networks");
        return false;
    }

    return true;
}

bool WifiManager::removeNetwork(const char* ssid)
{
    if (!ssid || ssid[0] == '\0')
        return false;

    for (auto iterator = knownNetworks.begin();
         iterator != knownNetworks.end();
         ++iterator)
    {
        if (iterator->ssid == ssid)
        {
            knownNetworks.erase(iterator);
            return saveKnownNetworks();
        }
    }

    return false;
}