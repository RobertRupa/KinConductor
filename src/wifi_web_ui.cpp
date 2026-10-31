#include "wifi_web_ui.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

namespace {

String makeRandomPassword(size_t length) {
    static constexpr char alphabet[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    String password;
    password.reserve(length);
    for (size_t index = 0; index < length; ++index) {
        password += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    return password;
}

String mimeType(const String &path) {
    if (path.endsWith(".html")) return "text/html; charset=utf-8";
    if (path.endsWith(".css")) return "text/css; charset=utf-8";
    if (path.endsWith(".js")) return "application/javascript; charset=utf-8";
    if (path.endsWith(".svg")) return "image/svg+xml";
    return "application/octet-stream";
}

bool findSavedRf24Packet(Preferences &preferences, uint32_t packetId,
                         uint32_t &payload) {
    DynamicJsonDocument packets(2048);
    const String stored = preferences.getString("rf24_saved", "[]");
    if (deserializeJson(packets, stored) || !packets.is<JsonArray>()) return false;
    for (JsonObject packet : packets.as<JsonArray>()) {
        if ((packet["id"] | 0U) == packetId) {
            payload = packet["payload"] | 0U;
            return true;
        }
    }
    return false;
}

}

void WifiWebUi::begin(StatusProvider statusProvider, RelayWriter relayWriter,
                      SettingsApplier settingsApplier,
                      BeeperTester beeperTester,
                      IftttUpdater iftttUpdater,
                      BluetoothScanner bluetoothScanner,
                      RadioPacketSender radioPacketSender,
                      RadioPacketHistoryProvider radioPacketHistoryProvider) {
    statusProvider_ = std::move(statusProvider);
    relayWriter_ = std::move(relayWriter);
    settingsApplier_ = std::move(settingsApplier);
    beeperTester_ = std::move(beeperTester);
    iftttUpdater_ = std::move(iftttUpdater);
    bluetoothScanner_ = std::move(bluetoothScanner);
    radioPacketSender_ = std::move(radioPacketSender);
    radioPacketHistoryProvider_ = std::move(radioPacketHistoryProvider);

    prepare();
    if (preferencesReady_ && iftttUpdater_) {
        iftttUpdater_(preferences_.getString("ifttt", "[]"));
    }
    webPassword_ = preferences_.getString("webpass", "");
    if (webPassword_.isEmpty()) {
        webPassword_ = makeRandomPassword(12);
        preferences_.putString("webpass", webPassword_);
    }
    if (settingsApplier_) settingsApplier_(settings_);

    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("ERROR: LittleFS mount failed (partition: littlefs)");
    } else {
        Serial.printf("LittleFS mounted: %u/%u bytes, index.html: %s\n",
                      LittleFS.usedBytes(), LittleFS.totalBytes(),
                      LittleFS.exists("/index.html") ? "found" : "missing");
    }

    const String ssid = preferences_.getString("ssid", "");
    const String password = preferences_.getString("password", "");
    if (ssid.isEmpty() || !connectToWifi(ssid, password)) {
        startSetupAccessPoint();
    } else {
        Serial.printf("Wi-Fi connected: %s (%s)\n", WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str());
        MDNS.begin("kc868-a6");
    }

    Serial.printf("Web login: admin / %s\n", webPassword_.c_str());
    startServer();
}

void WifiWebUi::prepare() {
    if (preferencesReady_) return;
    if (!preferences_.begin("kc868a6", false)) {
        Serial.println("ERROR: could not open NVS namespace kc868a6");
        return;
    }
    loadSettings();
    if (!preferences_.isKey("rf24_saved")) {
        preferences_.putString("rf24_saved", "[]");
    }
    String savedRules = preferences_.getString("ifttt", "[]");
    DynamicJsonDocument savedRulesDocument(2048);
    bool rulesAreValid = !deserializeJson(savedRulesDocument, savedRules) &&
                         savedRulesDocument.is<JsonArray>();
    if (!rulesAreValid) {
        for (size_t offset = 1; offset < savedRules.length(); ++offset) {
            if (savedRules[offset] != '[') continue;
            String candidate = savedRules.substring(offset);
            DynamicJsonDocument candidateDocument(2048);
            if (!deserializeJson(candidateDocument, candidate) &&
                candidateDocument.is<JsonArray>()) {
                savedRules = candidate;
                rulesAreValid = true;
                break;
            }
        }
        if (!rulesAreValid) savedRules = "[]";
        preferences_.putString("ifttt", savedRules);
        Serial.println(rulesAreValid
                           ? "Repaired IFTTT rules from legacy NVS value"
                           : "Reset invalid IFTTT rules in NVS");
    }
    preferencesReady_ = true;
}

bool WifiWebUi::connectToWifi(const String &ssid, const String &password) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid.c_str(), password.c_str());

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15000) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

void WifiWebUi::startSetupAccessPoint() {
    const uint64_t mac = ESP.getEfuseMac();
    char name[24];
    snprintf(name, sizeof(name), "KC868-A6-%04X",
             static_cast<unsigned int>(mac & 0xFFFF));
    apSsid_ = name;

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid_.c_str());
    setupAccessPoint_ = true;
    if (!dnsActive_) {
        dnsServer_.start(53, "*", WiFi.softAPIP());
        dnsActive_ = true;
    }

    Serial.printf("Open Wi-Fi setup AP: %s\nOpen: http://%s\n",
                  apSsid_.c_str(),
                  WiFi.softAPIP().toString().c_str());
}

void WifiWebUi::startServer() {
    server_.on("/", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/index.html");
    });
    server_.on("/app.css", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/app.css");
    });
    server_.on("/app.js", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/app.js");
    });
    server_.on("/api/status", HTTP_GET, [this]() {
        if (requireAuthentication()) handleStatus();
    });
    server_.on("/api/scan", HTTP_GET, [this]() {
        if (requireAuthentication()) handleScan();
    });
    server_.on("/api/wifi", HTTP_POST, [this]() {
        if (requireAuthentication()) handleWifiSave();
    });
    server_.on("/api/relay", HTTP_POST, [this]() {
        if (requireAuthentication()) handleRelay();
    });
    server_.on("/api/beeper/test", HTTP_POST, [this]() {
        if (requireAuthentication()) handleBeeperTest();
    });
    server_.on("/api/settings", HTTP_GET, [this]() {
        if (requireAuthentication()) handleSettings(false);
    });
    server_.on("/api/settings", HTTP_POST, [this]() {
        if (requireAuthentication()) handleSettings(true);
    });
    server_.on("/api/ifttt", HTTP_GET, [this]() {
        if (requireAuthentication()) handleIfttt(false);
    });
    server_.on("/api/ifttt", HTTP_POST, [this]() {
        if (requireAuthentication()) handleIfttt(true);
    });
    server_.on("/api/bluetooth/scan", HTTP_GET, [this]() {
        if (requireAuthentication()) handleBluetoothScan();
    });
    server_.on("/api/rf24/packets", HTTP_GET, [this]() {
        if (requireAuthentication()) handleRf24Packets(false);
    });
    server_.on("/api/rf24/packets", HTTP_POST, [this]() {
        if (requireAuthentication()) handleRf24Packets(true);
    });
    server_.on("/api/rf24/replay", HTTP_POST, [this]() {
        if (requireAuthentication()) handleRf24Replay();
    });
    server_.onNotFound([this]() {
        if (setupAccessPoint_) {
            server_.sendHeader("Location", "/", true);
            server_.send(302, "text/plain", "");
        } else {
            server_.send(404, "text/plain", "Not found");
        }
    });
    server_.begin();
}

bool WifiWebUi::requireAuthentication() {
    if (setupAccessPoint_) return true;
    if (server_.authenticate("admin", webPassword_.c_str())) return true;
    server_.requestAuthentication(BASIC_AUTH, "KC868-A6");
    return false;
}

void WifiWebUi::serveFile(const String &path) {
    if (!LittleFS.exists(path)) {
        server_.send(404, "text/plain", "UI file not found; upload LittleFS data");
        return;
    }
    File file = LittleFS.open(path, "r");
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server_.sendHeader("Pragma", "no-cache");
    server_.streamFile(file, mimeType(path));
    file.close();
}

void WifiWebUi::handleStatus() {
    DynamicJsonDocument document(8192);
    const String deviceStatus = statusProvider_ ? statusProvider_() : "{}";
    const DeserializationError statusError = deserializeJson(document, deviceStatus);
    if (statusError) {
        Serial.printf("ERROR: status JSON parse failed: %s (%u bytes)\n",
                      statusError.c_str(), deviceStatus.length());
        server_.send(500, "application/json", "{\"error\":\"status_json_failed\"}");
        return;
    }

    JsonObject network = document["network"].to<JsonObject>();
    network["connected"] = WiFi.status() == WL_CONNECTED;
    network["ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : apSsid_;
    network["ip"] = WiFi.status() == WL_CONNECTED
                         ? WiFi.localIP().toString()
                         : WiFi.softAPIP().toString();
    network["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    network["setup"] = setupAccessPoint_;
    network["hostname"] = "kc868-a6.local";

    String response;
    serializeJson(document, response);
    server_.send(200, "application/json", response);
}

void WifiWebUi::handleScan() {
    WiFi.mode(setupAccessPoint_ ? WIFI_AP_STA : WIFI_STA);
    const int count = WiFi.scanNetworks(false, true);
    DynamicJsonDocument document(3072);
    JsonArray networks = document["networks"].to<JsonArray>();
    for (int index = 0; index < count && index < 24; ++index) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(index);
        network["rssi"] = WiFi.RSSI(index);
        network["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();

    String response;
    serializeJson(document, response);
    server_.send(count < 0 ? 503 : 200, "application/json", response);
}

void WifiWebUi::handleWifiSave() {
    DynamicJsonDocument request(512);
    if (deserializeJson(request, server_.arg("plain"))) {
        server_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    const String ssid = request["ssid"] | "";
    const String password = request["password"] | "";
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
        (!password.isEmpty() && password.length() < 8)) {
        server_.send(400, "application/json", "{\"error\":\"invalid_credentials\"}");
        return;
    }

    preferences_.putString("ssid", ssid);
    preferences_.putString("password", password);
    server_.send(200, "application/json", "{\"saved\":true}");
    reconnectPending_ = true;
    reconnectAt_ = millis() + 1200;
}

void WifiWebUi::handleRelay() {
    DynamicJsonDocument request(256);
    if (deserializeJson(request, server_.arg("plain"))) {
        server_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    const int channel = request["channel"] | 0;
    const bool on = request["on"] | false;
    if (channel < 1 || channel > 6 || !relayWriter_ ||
        !relayWriter_(static_cast<uint8_t>(channel - 1), on)) {
        server_.send(400, "application/json", "{\"error\":\"relay_command_failed\"}");
        return;
    }
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WifiWebUi::handleBeeperTest() {
    if (!settings_.beeperEnabled) {
        server_.send(409, "application/json", "{\"error\":\"beeper_disabled\"}");
        return;
    }
    if (!beeperTester_ || !beeperTester_()) {
        server_.send(500, "application/json", "{\"error\":\"beeper_start_failed\"}");
        return;
    }
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WifiWebUi::loadSettings() {
#if defined(USE_GPS)
    constexpr uint8_t defaultUart1Mode = 1;
#elif defined(USE_ZIGBEE_THREAD_COPROCESSOR)
    constexpr uint8_t defaultUart1Mode = 2;
#else
    constexpr uint8_t defaultUart1Mode = 0;
#endif
    settings_.sensorIntervalMs = preferences_.getUShort("sensor_ms", 1000);
    settings_.i2cClockHz = preferences_.getUInt("i2c_hz", 100000);
    settings_.rs485Baud = preferences_.getUInt("rs485_baud", 9600);
    settings_.uart1Mode = preferences_.getUChar("uart1_mode", defaultUart1Mode);
    settings_.uart1Baud = preferences_.getUInt(
        "uart1_baud", settings_.uart1Mode == 2 ? 115200 : 9600);
    settings_.beeperFrequencyHz = preferences_.getUShort("beep_hz", 2000);
    settings_.beeperDurationMs = preferences_.getUShort("beep_ms", 250);
    settings_.gpio32SensorType = preferences_.getUChar("gpio32_type", 2);
    settings_.gpio33SensorType = preferences_.getUChar("gpio33_type", 0);
    settings_.displayMode = preferences_.getUChar("display_mode", 0);
    settings_.displayPageSeconds = preferences_.getUChar("display_secs", 5);
    settings_.displayBrightness = preferences_.getUChar("display_bright", 160);
    settings_.displayRotation = preferences_.getUChar("display_rotate", 0);
    settings_.displayTextSize = preferences_.getUChar("display_size", 1);
    settings_.displayFieldMask = preferences_.getUChar("display_fields", 0x3F);
    settings_.bluetoothMode = preferences_.getUChar("bt_mode", 1);
    settings_.beeperEnabled = preferences_.getBool("beep_on", false);
    settings_.displayEnabled = preferences_.getBool("display_on", true);
    settings_.s2InputEnabled = preferences_.getBool("s2_input", false);
    settings_.temperature1Enabled = preferences_.getBool("temp1_on", true);
    settings_.temperature2Enabled = preferences_.getBool("temp2_on", true);
    settings_.temperature1Enabled = settings_.gpio32SensorType != 0;
    settings_.temperature2Enabled = settings_.gpio33SensorType != 0;
    settings_.bme680Enabled = preferences_.getBool("bme_on", true);
    settings_.ens160Enabled = preferences_.getBool("ens_on", true);
    settings_.compassEnabled = preferences_.getBool("compass_on", true);
    settings_.gpsEnabled = preferences_.getBool(
        "gps_on", settings_.uart1Mode == 1);
    settings_.rtcEnabled = preferences_.getBool("rtc_on", true);
    settings_.pn532Enabled = preferences_.getBool("pn532_on", true);
}

void WifiWebUi::handleSettings(bool save) {
    if (save) {
        DynamicJsonDocument request(2048);
        if (deserializeJson(request, server_.arg("plain"))) {
            server_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
            return;
        }

        const uint16_t interval = request["sensorIntervalMs"] | settings_.sensorIntervalMs;
        const uint32_t i2cClock = request["i2cClockHz"] | settings_.i2cClockHz;
        const uint32_t rs485Baud = request["rs485Baud"] | settings_.rs485Baud;
        const uint32_t uart1Baud = request["uart1Baud"] | settings_.uart1Baud;
        const uint8_t uart1Mode = request["uart1Mode"] | settings_.uart1Mode;
        const uint16_t beeperFrequency = request["beeperFrequencyHz"] | settings_.beeperFrequencyHz;
        const uint16_t beeperDuration = request["beeperDurationMs"] | settings_.beeperDurationMs;
        const uint8_t gpio32Type = request["gpio32SensorType"] | settings_.gpio32SensorType;
        const uint8_t gpio33Type = request["gpio33SensorType"] | settings_.gpio33SensorType;
        const uint8_t displayMode = request["displayMode"] | settings_.displayMode;
        const uint8_t displaySeconds = request["displayPageSeconds"] | settings_.displayPageSeconds;
        const uint8_t displayBrightness = request["displayBrightness"] | settings_.displayBrightness;
        const uint8_t displayRotation = request["displayRotation"] | settings_.displayRotation;
        const uint8_t displayTextSize = request["displayTextSize"] | settings_.displayTextSize;
        const uint8_t displayFieldMask = request["displayFieldMask"] | settings_.displayFieldMask;
        const uint8_t bluetoothMode = request["bluetoothMode"] | settings_.bluetoothMode;
        const bool validBaud = uart1Baud == 9600 || uart1Baud == 19200 ||
                               uart1Baud == 38400 || uart1Baud == 57600 ||
                               uart1Baud == 115200;
        const bool validRs485Baud = rs485Baud == 9600 || rs485Baud == 19200 ||
                                    rs485Baud == 38400 || rs485Baud == 57600 ||
                                    rs485Baud == 115200;
        if (interval < 500 || interval > 60000 ||
                        (i2cClock != 50000 && i2cClock != 100000) ||
                        !validBaud || !validRs485Baud ||
                        uart1Mode > 2 ||
            beeperFrequency < 250 || beeperFrequency > 5000 ||
            beeperDuration < 50 || beeperDuration > 3000 ||
            gpio32Type > 2 || gpio33Type > 2 || displayMode > 4 ||
            displaySeconds < 2 || displaySeconds > 60 || displayRotation > 3 ||
            displayTextSize < 1 || displayTextSize > 2 ||
            displayFieldMask > 0x3F || bluetoothMode > 2) {
            server_.send(400, "application/json", "{\"error\":\"invalid_settings\"}");
            return;
        }

        settings_.sensorIntervalMs = interval;
        settings_.i2cClockHz = i2cClock;
        settings_.rs485Baud = rs485Baud;
        settings_.uart1Baud = uart1Baud;
        settings_.uart1Mode = uart1Mode;
        settings_.beeperFrequencyHz = beeperFrequency;
        settings_.beeperDurationMs = beeperDuration;
        settings_.gpio32SensorType = gpio32Type;
        settings_.gpio33SensorType = gpio33Type;
        settings_.temperature1Enabled = gpio32Type != 0;
        settings_.temperature2Enabled = gpio33Type != 0;
        settings_.displayMode = displayMode;
        settings_.displayPageSeconds = displaySeconds;
        settings_.displayBrightness = displayBrightness;
        settings_.displayRotation = displayRotation;
        settings_.displayTextSize = displayTextSize;
        settings_.displayFieldMask = displayFieldMask;
        settings_.bluetoothMode = bluetoothMode;
        settings_.beeperEnabled = request["beeperEnabled"] | settings_.beeperEnabled;
        settings_.displayEnabled = request["displayEnabled"] | settings_.displayEnabled;
        settings_.s2InputEnabled = request["s2InputEnabled"] | settings_.s2InputEnabled;
        settings_.temperature1Enabled = gpio32Type != 0;
        settings_.temperature2Enabled = gpio33Type != 0;
        settings_.bme680Enabled = request["bme680Enabled"] | settings_.bme680Enabled;
        settings_.ens160Enabled = request["ens160Enabled"] | settings_.ens160Enabled;
        settings_.compassEnabled = request["compassEnabled"] | settings_.compassEnabled;
        settings_.gpsEnabled = request["gpsEnabled"] | settings_.gpsEnabled;
        settings_.rtcEnabled = request["rtcEnabled"] | settings_.rtcEnabled;
        settings_.pn532Enabled = request["pn532Enabled"] | settings_.pn532Enabled;

        preferences_.putUShort("sensor_ms", settings_.sensorIntervalMs);
        preferences_.putUInt("i2c_hz", settings_.i2cClockHz);
        preferences_.putUInt("rs485_baud", settings_.rs485Baud);
        preferences_.putUInt("uart1_baud", settings_.uart1Baud);
        preferences_.putUChar("uart1_mode", settings_.uart1Mode);
        preferences_.putUShort("beep_hz", settings_.beeperFrequencyHz);
        preferences_.putUShort("beep_ms", settings_.beeperDurationMs);
        preferences_.putUChar("gpio32_type", settings_.gpio32SensorType);
        preferences_.putUChar("gpio33_type", settings_.gpio33SensorType);
        preferences_.putUChar("display_mode", settings_.displayMode);
        preferences_.putUChar("display_secs", settings_.displayPageSeconds);
        preferences_.putUChar("display_bright", settings_.displayBrightness);
        preferences_.putUChar("display_rotate", settings_.displayRotation);
        preferences_.putUChar("display_size", settings_.displayTextSize);
        preferences_.putUChar("display_fields", settings_.displayFieldMask);
        preferences_.putUChar("bt_mode", settings_.bluetoothMode);
        preferences_.putBool("beep_on", settings_.beeperEnabled);
        preferences_.putBool("display_on", settings_.displayEnabled);
        preferences_.putBool("s2_input", settings_.s2InputEnabled);
        preferences_.putBool("temp1_on", settings_.temperature1Enabled);
        preferences_.putBool("temp2_on", settings_.temperature2Enabled);
        preferences_.putBool("bme_on", settings_.bme680Enabled);
        preferences_.putBool("ens_on", settings_.ens160Enabled);
        preferences_.putBool("compass_on", settings_.compassEnabled);
        preferences_.putBool("gps_on", settings_.gpsEnabled);
        preferences_.putBool("rtc_on", settings_.rtcEnabled);
        preferences_.putBool("pn532_on", settings_.pn532Enabled);
        if (settingsApplier_) settingsApplier_(settings_);
    }

    DynamicJsonDocument response(2048);
    response["sensorIntervalMs"] = settings_.sensorIntervalMs;
    response["i2cClockHz"] = settings_.i2cClockHz;
    response["rs485Baud"] = settings_.rs485Baud;
    response["uart1Baud"] = settings_.uart1Baud;
    response["uart1Mode"] = settings_.uart1Mode;
    response["beeperFrequencyHz"] = settings_.beeperFrequencyHz;
    response["beeperDurationMs"] = settings_.beeperDurationMs;
    response["gpio32SensorType"] = settings_.gpio32SensorType;
    response["gpio33SensorType"] = settings_.gpio33SensorType;
    response["displayMode"] = settings_.displayMode;
    response["displayPageSeconds"] = settings_.displayPageSeconds;
    response["displayBrightness"] = settings_.displayBrightness;
    response["displayRotation"] = settings_.displayRotation;
    response["displayTextSize"] = settings_.displayTextSize;
    response["displayFieldMask"] = settings_.displayFieldMask;
    response["bluetoothMode"] = settings_.bluetoothMode;
    response["beeperEnabled"] = settings_.beeperEnabled;
    response["displayEnabled"] = settings_.displayEnabled;
    response["s2InputEnabled"] = settings_.s2InputEnabled;
    response["temperature1Enabled"] = settings_.temperature1Enabled;
    response["temperature2Enabled"] = settings_.temperature2Enabled;
    response["bme680Enabled"] = settings_.bme680Enabled;
    response["ens160Enabled"] = settings_.ens160Enabled;
    response["compassEnabled"] = settings_.compassEnabled;
    response["gpsEnabled"] = settings_.gpsEnabled;
    response["rtcEnabled"] = settings_.rtcEnabled;
    response["pn532Enabled"] = settings_.pn532Enabled;
    String body;
    serializeJson(response, body);
    server_.send(200, "application/json", body);
}

void WifiWebUi::handleIfttt(bool save) {
    if (!preferencesReady_) {
        server_.send(503, "application/json", "{\"error\":\"nvs_unavailable\"}");
        return;
    }
    String rulesJson = preferences_.getString("ifttt", "[]");
    if (save) {
        DynamicJsonDocument request(2048);
        if (deserializeJson(request, server_.arg("plain")) || !request.is<JsonArray>()) {
            server_.send(400, "application/json", "{\"error\":\"invalid_rules\"}");
            return;
        }

        JsonArray rules = request.as<JsonArray>();
        if (rules.size() > 4) {
            server_.send(400, "application/json", "{\"error\":\"too_many_rules\"}");
            return;
        }

        static const char *allowedSources[] = {
            "temperature1", "temperature2", "bmeTemperature", "humidity", "airQuality",
            "buttonS2", "rf24Packet",
            "digital1", "digital2", "digital3", "digital4",
            "digital5", "digital6", "digital7", "digital8",
            "analog1", "analog2", "analog3", "analog4"
        };
        for (JsonObject rule : rules) {
            const char *source = rule["source"] | "";
            const char *comparison = rule["comparison"] | "";
            const int relay = rule["relay"] | 0;
            const uint8_t page = rule["page"] | 0;
            const float threshold = rule["threshold"] | 0.0F;
            const char *action = rule["action"] | "relay";
            const uint64_t payload = rule["payload"] | 0ULL;
            const uint64_t packetId = rule["packetId"] | 0ULL;
            bool sourceAllowed = false;
            for (const char *candidate : allowedSources) {
                if (strcmp(source, candidate) == 0) sourceAllowed = true;
            }
            const bool comparisonAllowed =
                strcmp(comparison, ">") == 0 || strcmp(comparison, "<") == 0 ||
                strcmp(comparison, ">=") == 0 || strcmp(comparison, "<=") == 0 ||
                strcmp(comparison, "==") == 0 || strcmp(comparison, "!=") == 0;
            const bool rf24ActionAllowed = payload <= UINT32_MAX;
            uint32_t savedPayload = 0;
            const bool rf24SavedActionAllowed =
                packetId <= UINT32_MAX &&
                findSavedRf24Packet(preferences_, static_cast<uint32_t>(packetId),
                                    savedPayload) &&
                savedPayload == payload;
            const bool actionAllowed =
                (strcmp(action, "relay") == 0 && relay >= 1 && relay <= 6) ||
                (strcmp(action, "rf24Send") == 0 && rf24ActionAllowed) ||
                (strcmp(action, "rf24Saved") == 0 && rf24SavedActionAllowed) ||
                (strcmp(action, "beeper") == 0) ||
                (strcmp(action, "oledPage") == 0 && page >= 1 && page <= 3);
            if (!rule["enabled"].is<bool>() || !sourceAllowed ||
                !comparisonAllowed || !actionAllowed ||
                !isfinite(threshold)) {
                server_.send(400, "application/json", "{\"error\":\"invalid_rule\"}");
                return;
            }
        }

        rulesJson = "";
        serializeJson(rules, rulesJson);
        preferences_.putString("ifttt", rulesJson);
        if (preferences_.getString("ifttt", "[]") != rulesJson) {
            server_.send(500, "application/json", "{\"error\":\"nvs_write_failed\"}");
            return;
        }
        if (iftttUpdater_) iftttUpdater_(rulesJson);
    }
    server_.send(200, "application/json", rulesJson);
}

void WifiWebUi::handleBluetoothScan() {
    if (settings_.bluetoothMode != 1 || !bluetoothScanner_) {
        server_.send(409, "application/json", "{\"error\":\"ble_scan_disabled\"}");
        return;
    }
    const String devices = bluetoothScanner_();
    server_.send(200, "application/json", devices);
}

void WifiWebUi::handleRf24Packets(bool save) {
    if (!save) {
        DynamicJsonDocument response(4096);
        DynamicJsonDocument history(2048);
        const String historyJson = radioPacketHistoryProvider_
                                       ? radioPacketHistoryProvider_()
                                       : "{}";
        if (!deserializeJson(history, historyJson)) {
            response["available"] = history["available"] | false;
            response["channel"] = history["channel"] | 76;
            response["pipe"] = history["pipe"] | "E8E8F0F0E1";
            JsonArray packets = response["history"].to<JsonArray>();
            for (JsonObject packet : history["history"].as<JsonArray>()) {
                JsonObject item = packets.createNestedObject();
                item["payload"] = packet["payload"] | 0U;
                item["sequence"] = packet["sequence"] | 0U;
                item["ageMs"] = packet["ageMs"] | 0U;
            }
        } else {
            response["available"] = false;
            response["history"].to<JsonArray>();
        }

        DynamicJsonDocument saved(2048);
        const String savedJson = preferences_.getString("rf24_saved", "[]");
        JsonArray savedOutput = response["saved"].to<JsonArray>();
        if (!deserializeJson(saved, savedJson) && saved.is<JsonArray>()) {
            for (JsonObject packet : saved.as<JsonArray>()) {
                JsonObject item = savedOutput.createNestedObject();
                item["id"] = packet["id"] | 0U;
                item["name"] = packet["name"] | "";
                item["payload"] = packet["payload"] | 0U;
            }
        }
        String body;
        serializeJson(response, body);
        server_.send(200, "application/json", body);
        return;
    }

    DynamicJsonDocument request(256);
    if (deserializeJson(request, server_.arg("plain")) ||
        !request["payload"].is<uint32_t>()) {
        server_.send(400, "application/json", "{\"error\":\"invalid_packet\"}");
        return;
    }
    String name = request["name"] | "";
    name.trim();
    if (name.isEmpty() || name.length() > 32) {
        server_.send(400, "application/json", "{\"error\":\"invalid_name\"}");
        return;
    }
    const uint32_t payload = request["payload"].as<uint32_t>();
    DynamicJsonDocument history(2048);
    if (!radioPacketHistoryProvider_ ||
        deserializeJson(history, radioPacketHistoryProvider_())) {
        server_.send(503, "application/json", "{\"error\":\"radio_unavailable\"}");
        return;
    }
    bool wasReceived = false;
    for (JsonObject packet : history["history"].as<JsonArray>()) {
        if ((packet["payload"] | 0U) == payload) wasReceived = true;
    }
    if (!wasReceived) {
        server_.send(409, "application/json", "{\"error\":\"packet_not_captured\"}");
        return;
    }

    DynamicJsonDocument saved(2048);
    if (deserializeJson(saved, preferences_.getString("rf24_saved", "[]")) ||
        !saved.is<JsonArray>()) {
        server_.send(500, "application/json", "{\"error\":\"packet_store_invalid\"}");
        return;
    }
    JsonArray packets = saved.as<JsonArray>();
    if (packets.size() >= 12) {
        server_.send(409, "application/json", "{\"error\":\"packet_store_full\"}");
        return;
    }
    uint32_t packetId = 1;
    for (JsonObject packet : packets) {
        const uint32_t currentId = packet["id"] | 0U;
        if (currentId >= packetId) packetId = currentId + 1;
    }
    if (packetId == 0) packetId = 1;
    JsonObject storedPacket = packets.createNestedObject();
    storedPacket["id"] = packetId;
    storedPacket["name"] = name;
    storedPacket["payload"] = payload;
    String stored;
    serializeJson(saved, stored);
    preferences_.putString("rf24_saved", stored);
    if (preferences_.getString("rf24_saved", "[]") != stored) {
        server_.send(500, "application/json", "{\"error\":\"nvs_write_failed\"}");
        return;
    }
    DynamicJsonDocument response(256);
    response["id"] = packetId;
    response["name"] = name;
    response["payload"] = payload;
    String body;
    serializeJson(response, body);
    server_.send(200, "application/json", body);
}

void WifiWebUi::handleRf24Replay() {
    DynamicJsonDocument request(128);
    if (deserializeJson(request, server_.arg("plain")) ||
        !request["id"].is<uint32_t>()) {
        server_.send(400, "application/json", "{\"error\":\"invalid_packet_id\"}");
        return;
    }
    if (!radioPacketSender_) {
        server_.send(409, "application/json", "{\"error\":\"radio_unavailable\"}");
        return;
    }
    uint32_t payload = 0;
    if (!findSavedRf24Packet(preferences_, request["id"].as<uint32_t>(), payload)) {
        server_.send(404, "application/json", "{\"error\":\"packet_not_saved\"}");
        return;
    }
    if (!radioPacketSender_(payload)) {
        server_.send(502, "application/json", "{\"error\":\"radio_send_failed\"}");
        return;
    }
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WifiWebUi::reconnectWithSavedCredentials() {
    reconnectPending_ = false;
    const String ssid = preferences_.getString("ssid", "");
    const String password = preferences_.getString("password", "");

    if (dnsActive_) {
        dnsServer_.stop();
        dnsActive_ = false;
    }
    WiFi.softAPdisconnect(true);
    setupAccessPoint_ = false;
    if (connectToWifi(ssid, password)) {
        MDNS.begin("kc868-a6");
        Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        startSetupAccessPoint();
    }
}

void WifiWebUi::loop() {
    if (dnsActive_) dnsServer_.processNextRequest();
    server_.handleClient();
    if (reconnectPending_ &&
        static_cast<int32_t>(millis() - reconnectAt_) >= 0) {
        reconnectWithSavedCredentials();
    }
}