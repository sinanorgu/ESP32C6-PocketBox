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
    const int16_t networkCount =
        WiFi.scanNetworks(false, true);

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
