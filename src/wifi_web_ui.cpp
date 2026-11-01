#include "wifi_web_ui.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "board/target_model.h"
#include "config/settings_json.h"
#include "web/board_status.h"
#include "web/capabilities.h"
#include "peripherals/udp_rules.h"
#include "peripherals/ble_targets.h"

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

#if defined(USE_RADIO_NRF24)
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
#endif

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
    loadScreen();
    loadBluetoothTarget();
    loadCurrentInputs();
    if (preferencesReady_ && iftttUpdater_) {
        iftttUpdater_(preferences_.getString("ifttt", "[]"));
    }
    webPassword_ = preferences_.getString("webpass", "");
    if (webPassword_.isEmpty()) {
        webPassword_ = makeRandomPassword(12);
        preferences_.putString("webpass", webPassword_);
    }
    if (settingsApplier_) settingsApplier_(settings());

    if (!LittleFS.begin(false, "/littlefs", 10, "fsdata")) {
        Serial.println("ERROR: LittleFS mount failed (partition: fsdata)");
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
        startMdns();
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
    const auto loaded = configuration_.begin();
    if (!loaded.ok()) {
        Serial.printf("ERROR: settings load failed: %s (%.*s)\n",
                      kc868a6::core::errorCodeName(loaded.code),
                      static_cast<int>(loaded.subject.size()), loaded.subject.data());
    }
#if defined(USE_RADIO_NRF24)
    if (!preferences_.isKey("rf24_saved")) {
        preferences_.putString("rf24_saved", "[]");
    }
#endif
    String savedRules = preferences_.getString("ifttt", "[]");
    DynamicJsonDocument savedRulesDocument(8192);
    const auto rulesError = deserializeJson(savedRulesDocument, savedRules);
    if (rulesError == DeserializationError::NoMemory) {
        Serial.println("ERROR: IFTTT rules exceed JSON capacity; preserving NVS record");
        preferencesReady_ = true;
        loadNetworkConfig();
        return;
    }
    bool rulesAreValid = !rulesError &&
                         savedRulesDocument.is<JsonArray>();
    if (!rulesAreValid) {
        for (size_t offset = 1; offset < savedRules.length(); ++offset) {
            if (savedRules[offset] != '[') continue;
            String candidate = savedRules.substring(offset);
            DynamicJsonDocument candidateDocument(8192);
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
    loadNetworkConfig();
}

bool WifiWebUi::connectToWifi(const String &ssid, const String &password) {
    if (!networkReady_) {
        Serial.println("ERROR: network configuration unavailable; use setup to repair");
        return false;
    }
    WiFi.mode(WIFI_STA);
    if (!WiFi.setHostname(networkConfig_.hostname.data())) {
        Serial.println("ERROR: could not set Wi-Fi hostname");
        return false;
    }
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
    server_.on("/favicon.svg", HTTP_GET, [this]() {
        const String model(kc868a6::board::targetModel.data());
        const int separator = model.lastIndexOf('-');
        const String badge = separator >= 0 ? model.substring(separator + 1) : model;
        String icon;
        icon.reserve(320);
        icon = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\">"
               "<rect width=\"64\" height=\"64\" rx=\"9\" fill=\"#183c32\"/>"
               "<path d=\"M0 49h64v15H0z\" fill=\"#d8f35c\"/>"
               "<text x=\"32\" y=\"34\" text-anchor=\"middle\" "
               "font-family=\"sans-serif\" font-size=\"";
        icon += badge.length() > 2 ? "20" : "25";
        icon += "\" font-weight=\"800\" fill=\"#fff\">";
        icon += badge;
        icon += "</text></svg>";
        server_.sendHeader("Cache-Control", "no-cache");
        server_.send(200, "image/svg+xml", icon);
    });
    server_.on("/favicon.ico", HTTP_GET, [this]() {
        server_.sendHeader("Location", "/favicon.svg", true);
        server_.send(302, "text/plain", "");
    });
    server_.on("/", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/index.html");
    });
    server_.on("/app.css", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/app.css");
    });
    server_.on("/app.js", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/app.js");
    });
    server_.on("/capabilities.js", HTTP_GET, [this]() {
        if (requireAuthentication()) serveFile("/capabilities.js");
    });
    server_.on("/api/status", HTTP_GET, [this]() {
        if (requireAuthentication()) handleStatus();
    });
    server_.on("/api/board", HTTP_GET, [this]() {
        if (requireAuthentication()) handleBoard();
    });
    server_.on("/api/capabilities", HTTP_GET, [this]() {
        if (requireAuthentication()) handleCapabilities();
    });
    server_.on("/api/actions", HTTP_POST, [this]() {
        if (requireAuthentication()) handleAction();
    });
    server_.on("/api/settings/schema", HTTP_GET, [this]() {
        if (requireAuthentication()) handleSettingsSchema();
    });
    server_.on("/api/network", HTTP_GET, [this]() {
        if (requireAuthentication()) handleNetworkConfig(false);
    });
    server_.on("/api/network", HTTP_POST, [this]() {
        if (requireAuthentication()) handleNetworkConfig(true);
    });
    server_.on("/api/current", HTTP_GET, [this]() {
        if (requireAuthentication()) handleCurrentInputs(false);
    });
    server_.on("/api/current", HTTP_POST, [this]() {
        if (requireAuthentication()) handleCurrentInputs(true);
    });
    server_.on("/api/screen", HTTP_GET, [this]() {
        if (requireAuthentication()) handleScreen(false);
    });
    server_.on("/api/screen", HTTP_POST, [this]() {
        if (requireAuthentication()) handleScreen(true);
    });
    server_.on("/api/scan", HTTP_GET, [this]() {
        if (requireAuthentication()) handleScan();
    });
    server_.on("/api/wifi", HTTP_POST, [this]() {
        if (requireAuthentication()) handleWifiSave();
    });
    server_.on("/api/password", HTTP_POST, [this]() {
        if (requireAuthentication()) handlePasswordChange();
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
    server_.on("/api/bluetooth/target", HTTP_GET, [this]() {
        if (requireAuthentication()) handleBluetoothTarget(false);
    });
    server_.on("/api/bluetooth/target", HTTP_POST, [this]() {
        if (requireAuthentication()) handleBluetoothTarget(true);
    });
    server_.on("/api/bluetooth/targets", HTTP_GET, [this]() {
        if (requireAuthentication()) handleBluetoothTarget(false, true);
    });
    server_.on("/api/bluetooth/targets", HTTP_POST, [this]() {
        if (requireAuthentication()) handleBluetoothTarget(true, true);
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
    network["hostname"] = networkConfig_.hostname.data();
    network["mdns"] = String(networkConfig_.hostname.data()) + ".local";
    network["mdnsReady"] = mdnsReady_;
    network["configurationReady"] = networkReady_;
    network["udpListenAddress"] = "0.0.0.0";
    network["udpListenPort"] = settings().udpListenPort;
    network["reconnectPending"] = reconnectPending_;

    String response;
    if (document.overflowed()) {
        sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "status"}, 500);
        return;
    }
    serializeJson(document, response);
    server_.send(200, "application/json", response);
}

void WifiWebUi::handleBoard() {
    DynamicJsonDocument document(kc868a6::web::boardJsonCapacity);
    const auto written = kc868a6::web::writeBoardStatus(
        document.to<JsonObject>(), settings(), kc868a6::board::compiledBoardOptions(),
        configuration_.status());
    if (!written.ok()) {
        sendSettingsError(written, 500);
        return;
    }
    String response;
    serializeJson(document, response);
    server_.send(200, "application/json", response);
}

void WifiWebUi::handleCapabilities() {
    DynamicJsonDocument document(kc868a6::web::capabilitiesJsonCapacity);
    const auto result = kc868a6::web::writeCapabilities(document.to<JsonObject>(), registry_, millis());
    if (!result.ok()) { sendSettingsError(result, 500); return; }
    String body;
    const size_t length = measureJson(document);
    if (!body.reserve(length) || serializeJson(document, body) != length || body.length() != length) {
        sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "capabilities_response"}, 500);
        return;
    }
    server_.send(200, "application/json", body);
}

void WifiWebUi::handleAction() {
    if (server_.arg("plain").length() > 1024) {
        sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "action_request"}, 413);
        return;
    }
    DynamicJsonDocument request(8192);
    if (server_.arg("plain").length() > 8192) {
        sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "ifttt"}, 413);
        return;
    }
    if (deserializeJson(request, server_.arg("plain"))) {
        sendSettingsError({kc868a6::core::ErrorCode::InvalidArgument, "action_json"}, 400);
        return;
    }
    const auto action = kc868a6::web::executeJsonAction(registry_, request.as<JsonVariantConst>());
    if (!action.result.ok()) {
        const auto code = action.result.code;
        sendSettingsError(action.result, code == kc868a6::core::ErrorCode::NotFound ? 404
            : code == kc868a6::core::ErrorCode::InvalidArgument ? 400
            : code == kc868a6::core::ErrorCode::Unavailable ? 503 : 500, &action);
        return;
    }
    StaticJsonDocument<128> response;
    response["applied"] = action.applied;
    response["confirmed"] = action.confirmed;
    String body;
    serializeJson(response, body);
    server_.send(200, "application/json", body);
}

void WifiWebUi::handleSettingsSchema() {
    DynamicJsonDocument document(8192);
    const auto result = kc868a6::config::writeSettingsSchema(document.to<JsonObject>());
    if (!result.ok()) { sendSettingsError(result, 500); return; }
    String body;
    serializeJson(document, body);
    server_.send(200, "application/json", body);
}

void WifiWebUi::loadScreen() {
    if (!preferencesReady_) { screenReady_ = false; return; }
    if (!preferences_.isKey("screen_grid")) return;
    DynamicJsonDocument document(kc868a6::display::layoutJsonCapacity);
    const String stored = preferences_.getString("screen_grid");
    kc868a6::core::Result result;
    if (stored.length() > kc868a6::display::layoutJsonCapacity || deserializeJson(document, stored)) {
        result = {kc868a6::core::ErrorCode::InvalidArgument, "screen_record"};
    } else {
        result = kc868a6::display::readLayout(document.as<JsonVariantConst>(), registry_, screenLayout_);
    }
    if (!result.ok()) {
        screenReady_ = false;
        Serial.printf("ERROR: screen load failed: %s\n", kc868a6::core::errorCodeName(result.code));
    }
}

void WifiWebUi::handleScreen(bool save) {
    if (!preferencesReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::StorageUnavailable, "screen"}, 503);
        return;
    }
    if (save) {
        if (server_.arg("plain").length() > kc868a6::display::layoutJsonCapacity) {
            sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "screen"}, 413); return;
        }
        DynamicJsonDocument request(kc868a6::display::layoutJsonCapacity);
        if (deserializeJson(request, server_.arg("plain"))) {
            sendSettingsError({kc868a6::core::ErrorCode::InvalidArgument, "screen_json"}, 400); return;
        }
        kc868a6::display::ScreenLayout candidate;
        const auto valid = kc868a6::display::readLayout(request.as<JsonVariantConst>(), registry_, candidate);
        if (!valid.ok()) { sendSettingsError(valid, 400); return; }
        String encoded;
        serializeJson(request, encoded);
        if (preferences_.putString("screen_grid", encoded) != encoded.length() ||
            preferences_.getString("screen_grid") != encoded) {
            screenReady_ = false;
            sendSettingsError({kc868a6::core::ErrorCode::StorageWriteFailed, "screen_grid"}, 500); return;
        }
        screenLayout_ = candidate;
        screenReady_ = true;
    }
    if (!screenReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "screen_layout"}, 503); return;
    }
    DynamicJsonDocument document(kc868a6::display::layoutJsonCapacity);
    const auto written = kc868a6::display::writeLayout(document.to<JsonObject>(), screenLayout_);
    if (!written.ok()) { sendSettingsError(written, 500); return; }
    String body;
    serializeJson(document, body);
    server_.send(200, "application/json", body);
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

void WifiWebUi::handlePasswordChange() {
    if (!preferencesReady_) {
        server_.send(503, "application/json", "{\"error\":\"nvs_unavailable\"}");
        return;
    }
    if (server_.arg("plain").length() > 512) {
        server_.send(413, "application/json", "{\"error\":\"password_request_too_large\"}");
        return;
    }

    DynamicJsonDocument request(512);
    if (deserializeJson(request, server_.arg("plain"))) {
        server_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }
    const String currentPassword = request["currentPassword"] | "";
    const String newPassword = request["newPassword"] | "";
    if (currentPassword != webPassword_) {
        server_.send(403, "application/json", "{\"error\":\"invalid_current_password\"}");
        return;
    }
    if (newPassword.length() < 8 || newPassword.length() > 63) {
        server_.send(400, "application/json", "{\"error\":\"invalid_password\"}");
        return;
    }
    if (preferences_.putString("webpass", newPassword) != newPassword.length() ||
        preferences_.getString("webpass", "") != newPassword) {
        server_.send(500, "application/json", "{\"error\":\"password_storage_failed\"}");
        return;
    }

    webPassword_ = newPassword;
    server_.send(200, "application/json", "{\"changed\":true}");
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
    if (!settings().beeperEnabled) {
        server_.send(409, "application/json", "{\"error\":\"beeper_disabled\"}");
        return;
    }
    if (!beeperTester_ || !beeperTester_()) {
        server_.send(500, "application/json", "{\"error\":\"beeper_start_failed\"}");
        return;
    }
    server_.send(200, "application/json", "{\"ok\":true}");
}

void WifiWebUi::sendSettingsError(const kc868a6::core::Result &result, int status,
                                 const kc868a6::core::ActionResult *action) {
    Serial.printf("ERROR: settings request failed: %s (%.*s)\n",
                  kc868a6::core::errorCodeName(result.code),
                  static_cast<int>(result.subject.size()), result.subject.data());
    StaticJsonDocument<256> response;
    response["error"] = result.code == kc868a6::core::ErrorCode::InvalidArgument
                            ? "invalid_settings"
                            : kc868a6::core::errorCodeName(result.code);
    response["field"] = String(result.subject.data(), result.subject.size());
    if (action) {
        response["applied"] = action->applied;
        response["confirmed"] = action->confirmed;
    }
    String body;
    serializeJson(response, body);
    server_.send(status, "application/json", body);
}

void WifiWebUi::handleSettings(bool save) {
    if (!configuration_.ready()) {
        sendSettingsError(configuration_.status(), 503);
        return;
    }
    if (save) {
        if (server_.arg("plain").length() > kc868a6::config::settingsJsonCapacity) {
            sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded,
                               "settings_request"}, 413);
            return;
        }
        DynamicJsonDocument request(kc868a6::config::settingsJsonCapacity);
        if (deserializeJson(request, server_.arg("plain"))) {
            server_.send(400, "application/json", "{\"error\":\"invalid_json\"}");
            return;
        }
        DeviceSettings candidate;
        const auto parsed = kc868a6::config::readSettingsJson(
            request.as<JsonVariantConst>(), settings(), candidate);
        if (!parsed.ok()) {
            sendSettingsError(parsed, 400);
            return;
        }
        const bool changed = !kc868a6::config::equalSettings(settings(), candidate);
        DynamicJsonDocument rules(8192);
        kc868a6::peripherals::UdpRulesConfig udpConfig;
        if (deserializeJson(rules, preferences_.getString("ifttt", "[]"))) {
            sendSettingsError({kc868a6::core::ErrorCode::InvalidArgument, "ifttt_stored_rules"}, 409);
            return;
        }
        const auto ports = kc868a6::peripherals::readUdpRules(
            rules.as<JsonVariantConst>(), candidate.udpListenPort, udpConfig);
        if (!ports.ok()) { sendSettingsError(ports, 409); return; }
        const auto stored = configuration_.update(candidate);
        if (!stored.ok()) {
            sendSettingsError(stored,
                stored.code == kc868a6::core::ErrorCode::ResourceConflict ? 409 : 500);
            return;
        }
        if (changed && settingsApplier_) settingsApplier_(settings());
    }

    DynamicJsonDocument response(kc868a6::config::settingsJsonCapacity);
    const auto serialized = kc868a6::config::writeSettingsJson(
        response.to<JsonObject>(), settings());
    if (!serialized.ok()) {
        sendSettingsError(serialized, 500);
        return;
    }
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
        if (server_.arg("plain").length() > 8192) {
            sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "ifttt"}, 413);
            return;
        }
        DynamicJsonDocument request(8192);
        if (deserializeJson(request, server_.arg("plain")) || !request.is<JsonArray>()) {
            server_.send(400, "application/json", "{\"error\":\"invalid_rules\"}");
            return;
        }

        JsonArray rules = request.as<JsonArray>();
        if (rules.size() > kc868a6::peripherals::iftttRuleCapacity) {
            server_.send(400, "application/json", "{\"error\":\"too_many_rules\"}");
            return;
        }
        kc868a6::peripherals::UdpRulesConfig udpConfig;
        const auto validUdp = kc868a6::peripherals::readUdpRules(
            request.as<JsonVariantConst>(), settings().udpListenPort, udpConfig);
        if (!validUdp.ok()) {
            sendSettingsError(validUdp, validUdp.code == kc868a6::core::ErrorCode::ResourceConflict ? 409 : 400);
            return;
        }

        static const char *allowedSources[] = {
            "temperature1", "temperature2", "bmeTemperature", "humidity", "airQuality",
            "buttonS2",
#if defined(USE_RADIO_NRF24)
            "rf24Packet",
#endif
            "digital1", "digital2", "digital3", "digital4",
            "digital5", "digital6", "digital7", "digital8",
            "analog1", "analog2", "analog3", "analog4"
        };
        const auto validateAction = [&](auto &&self, JsonVariantConst candidate,
                                        bool allowGroup) -> bool {
            if (!candidate.is<JsonObjectConst>()) return false;
            const char *action = candidate["action"] | "relay";
            if (strcmp(action, "group") == 0) {
                if (!allowGroup) return false;
                const char *mode = candidate["mode"] | "";
                JsonArrayConst actions = candidate["actions"].as<JsonArrayConst>();
                if ((strcmp(mode, "all") != 0 && strcmp(mode, "sequential") != 0) ||
                    actions.isNull() || actions.size() == 0 ||
                    actions.size() > kc868a6::peripherals::iftttActionCapacity) return false;
                for (JsonVariantConst child : actions) {
                    if (!self(self, child, false)) return false;
                }
                return true;
            }
            const int relay = candidate["relay"] | 0;
            const uint8_t page = candidate["page"] | 0;
#if defined(USE_RADIO_NRF24)
            const uint64_t payload = candidate["payload"] | 0ULL;
            const uint64_t packetId = candidate["packetId"] | 0ULL;
            const bool rf24ActionAllowed = payload <= UINT32_MAX;
            uint32_t savedPayload = 0;
            const bool rf24SavedActionAllowed =
                packetId <= UINT32_MAX &&
                findSavedRf24Packet(preferences_, static_cast<uint32_t>(packetId), savedPayload) &&
                savedPayload == payload;
#endif
            return (strcmp(action, "relay") == 0 && relay >= 1 && relay <= 6) ||
#if defined(USE_RADIO_NRF24)
                (strcmp(action, "rf24Send") == 0 && rf24ActionAllowed) ||
                (strcmp(action, "rf24Saved") == 0 && rf24SavedActionAllowed) ||
#endif
                (strcmp(action, "beeper") == 0) ||
                (strcmp(action, "capability") == 0 &&
                 kc868a6::web::executeJsonAction(registry_, candidate["command"], false).result.ok()) ||
                (strcmp(action, "oledPage") == 0 && page >= 1 && page <= 3);
        };
        for (JsonObject rule : rules) {
            const char *source = rule["source"] | "";
            const char *comparison = rule["comparison"] | "";
            const float threshold = rule["threshold"] | 0.0F;
            bool sourceAllowed = registry_.findProperty(source) != nullptr || strcmp(source, "udpPacket") == 0;
            for (const char *candidate : allowedSources) {
                if (strcmp(source, candidate) == 0) sourceAllowed = true;
            }
            const bool comparisonAllowed =
                strcmp(comparison, ">") == 0 || strcmp(comparison, "<") == 0 ||
                strcmp(comparison, ">=") == 0 || strcmp(comparison, "<=") == 0 ||
                strcmp(comparison, "==") == 0 || strcmp(comparison, "!=") == 0 ||
                strcmp(comparison, "changed") == 0;
            if (!rule["enabled"].is<bool>() || !sourceAllowed ||
                !(strcmp(source, "udpPacket") == 0 || comparisonAllowed) ||
                !validateAction(validateAction, rule, true) ||
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
    if (settings().bluetoothMode != 1 || !bluetoothScanner_) {
        server_.send(409, "application/json", "{\"error\":\"ble_scan_disabled\"}");
        return;
    }
    String devices;
    const auto scan = bluetoothScanner_(devices);
    if (!scan.ok()) { sendSettingsError(scan, 503); return; }
    server_.send(200, "application/json", devices);
}

void WifiWebUi::loadBluetoothTarget() {
    if (!preferencesReady_ || !blePresence_) {
        bleTargetReady_ = false;
        return;
    }
    kc868a6::peripherals::BleTargets targets;
    kc868a6::core::Result result;
    if (preferences_.isKey("ble_targets")) {
        DynamicJsonDocument document(kc868a6::peripherals::bleTargetsJsonCapacity);
        const auto parsed = deserializeJson(document, preferences_.getString("ble_targets"));
        result = parsed ? kc868a6::core::Result{kc868a6::core::ErrorCode::InvalidArgument, "ble_targets"} :
            kc868a6::peripherals::readBleTargets(document.as<JsonVariantConst>(), targets);
    } else {
        const String stored = preferences_.getString("ble_target", "");
        if (!stored.isEmpty()) {
            result = kc868a6::peripherals::BlePresence::parseMac({stored.c_str(), stored.length()}, targets.addresses[0]);
            if (result.ok()) { targets.count = 1; targets.legacyAddress = targets.addresses[0]; }
        }
    }
    if (result.ok()) result = blePresence_->setTargets(targets, millis());
    bleTargetReady_ = result.ok();
    if (!result.ok()) {
        Serial.printf("ERROR: BLE target load failed: %s\n", kc868a6::core::errorCodeName(result.code));
    }
}

void WifiWebUi::handleBluetoothTarget(bool save, bool multiple) {
    if (!preferencesReady_ || !blePresence_) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "ble_target"}, 503);
        return;
    }
    if (save) {
        DynamicJsonDocument request(kc868a6::peripherals::bleTargetsJsonCapacity);
        if (server_.arg("plain").length() > 512 ||
            deserializeJson(request, server_.arg("plain"))) {
            sendSettingsError({kc868a6::core::ErrorCode::InvalidArgument, "ble_target"}, 400);
            return;
        }
        kc868a6::peripherals::BleTargets targets;
        kc868a6::core::Result valid;
        if (multiple) {
            valid = kc868a6::peripherals::readBleTargets(request.as<JsonVariantConst>(), targets);
            if (!request.containsKey("legacyAddress")) targets.legacyAddress = blePresence_->targets().legacyAddress;
        } else if (!request.is<JsonObject>() || request.size() != 1 || !request["address"].is<const char *>()) {
            valid = {kc868a6::core::ErrorCode::InvalidArgument, "ble_target"};
        } else {
            const JsonString address = request["address"].as<JsonString>();
            std::array<char, 18> normalized{};
            valid = kc868a6::peripherals::BlePresence::normalizeTargetMac({address.c_str(), address.size()}, normalized);
            if (valid.ok() && !address.isNull() && address.size() != 0) {
                valid = kc868a6::peripherals::BlePresence::parseMac(normalized.data(), targets.addresses[0]);
                targets.count = valid.ok() ? 1 : 0;
                if (valid.ok()) targets.legacyAddress = targets.addresses[0];
            }
            // Legacy writes retain their single-target replacement semantics.
            if (blePresence_->targets().count > 1) valid = {kc868a6::core::ErrorCode::ResourceConflict, "use_ble_targets"};
        }
        if (!valid.ok()) { sendSettingsError(valid,
            valid.code == kc868a6::core::ErrorCode::ResourceConflict ? 409 : 400); return; }
        request.clear();
        valid = kc868a6::peripherals::writeBleTargets(request.to<JsonObject>(), targets);
        if (!valid.ok() || request.overflowed()) {
            sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "ble_targets"}, 500); return;
        }
        String stored;
        const size_t length = measureJson(request);
        if (!stored.reserve(length) || serializeJson(request, stored) != length || stored.length() != length) {
            sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "ble_targets"}, 500); return;
        }
        const auto written = preferences_.putString("ble_targets", stored);
        if (written != stored.length() ||
            !preferences_.isKey("ble_targets") ||
            preferences_.getString("ble_targets", "!") != stored) {
            bleTargetReady_ = false;
            sendSettingsError({kc868a6::core::ErrorCode::StorageWriteFailed, "ble_target"}, 500);
            return;
        }
        const auto applied = blePresence_->setTargets(targets, millis());
        bleTargetReady_ = applied.ok();
        if (!applied.ok()) { sendSettingsError(applied, 500); return; }
    }
    if (!bleTargetReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "ble_target"}, 503);
        return;
    }
    DynamicJsonDocument response(JSON_OBJECT_SIZE(8) + 2 * JSON_ARRAY_SIZE(8) +
        8 * JSON_OBJECT_SIZE(4) + 1536);
    const auto mac = blePresence_->selectedMac();
    response["address"].set(mac);
    response["property"] = "ble.presence/in_range";
    response["timeoutSeconds"] = settings().blePresenceTimeoutSeconds;
    if (multiple) {
        const auto result = kc868a6::peripherals::writeBleTargets(response.as<JsonObject>(), blePresence_->targets());
        if (!result.ok()) { sendSettingsError(result, 500); return; }
        response["capacity"] = kc868a6::peripherals::bleTargetCapacity;
        auto devices = response.createNestedArray("devices");
        const auto descriptors = blePresence_->properties();
        for (size_t i = 0; i < blePresence_->targets().count; ++i) {
            auto device = devices.createNestedObject();
            const auto address = kc868a6::peripherals::BlePresence::formatMac(blePresence_->targets().addresses[i]);
            device["address"].set(std::string_view{address.data(), 17});
            device["property"].set(descriptors.data[i + 1].id);
            kc868a6::core::PropertySnapshot snapshot;
            const auto read = blePresence_->readProperty(descriptors.data[i + 1].id, snapshot);
            if (!read.ok()) { sendSettingsError(read, 500); return; }
            device["in_range"] = std::get<bool>(snapshot.value);
            device["quality"] = kc868a6::web::qualityName(snapshot.quality);
        }
    }
    String body;
    const size_t length = measureJson(response);
    if (response.overflowed() || !body.reserve(length) ||
        serializeJson(response, body) != length || body.length() != length) {
        sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "ble_targets"}, 500); return;
    }
    server_.send(200, "application/json", body);
}

void WifiWebUi::loadCurrentInputs() {
    if (!currentInputs_ || !preferencesReady_) {
        currentReady_ = false;
        return;
    }
    if (!preferences_.isKey("current_cfg")) return;
    DynamicJsonDocument document(kc868a6::peripherals::currentJsonCapacity);
    kc868a6::peripherals::CurrentConfig config;
    const auto parsed = deserializeJson(document, preferences_.getString("current_cfg"));
    const auto result = parsed ? kc868a6::core::Result{kc868a6::core::ErrorCode::InvalidArgument, "current_cfg"} :
        kc868a6::peripherals::readCurrentConfig(document.as<JsonVariantConst>(), config);
    currentReady_ = result.ok();
    if (result.ok()) {
        currentInputs_->configure(config);
    } else {
        currentInputs_->unavailable(result.code);
        Serial.printf("ERROR: current calibration load failed: %s\n", kc868a6::core::errorCodeName(result.code));
    }
}

void WifiWebUi::handleCurrentInputs(bool save) {
    if (!currentInputs_ || !preferencesReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "current_cfg"}, 503);
        return;
    }
    DynamicJsonDocument document(kc868a6::peripherals::currentJsonCapacity);
    if (save) {
        if (server_.arg("plain").length() > kc868a6::peripherals::currentJsonCapacity) {
            sendSettingsError({kc868a6::core::ErrorCode::CapacityExceeded, "current_cfg"}, 413);
            return;
        }
        if (deserializeJson(document, server_.arg("plain"))) {
            sendSettingsError({kc868a6::core::ErrorCode::InvalidArgument, "current_json"}, 400);
            return;
        }
        kc868a6::peripherals::CurrentConfig candidate;
        const auto valid = kc868a6::peripherals::readCurrentConfig(document.as<JsonVariantConst>(), candidate);
        if (!valid.ok()) { sendSettingsError(valid, 400); return; }
        String stored;
        serializeJson(document, stored);
        if (preferences_.putString("current_cfg", stored) != stored.length() ||
            preferences_.getString("current_cfg") != stored) {
            currentReady_ = false;
            currentInputs_->unavailable(kc868a6::core::ErrorCode::StorageWriteFailed);
            sendSettingsError({kc868a6::core::ErrorCode::StorageWriteFailed, "current_cfg"}, 500);
            return;
        }
        const auto applied = currentInputs_->configure(candidate);
        currentReady_ = applied.ok();
        if (!applied.ok()) { sendSettingsError(applied, 500); return; }
    }
    if (!currentReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "current_cfg"}, 503);
        return;
    }
    document.clear();
    const auto written = kc868a6::peripherals::writeCurrentConfig(document.to<JsonObject>(), currentInputs_->config());
    if (!written.ok()) { sendSettingsError(written, 500); return; }
    String body;
    serializeJson(document, body);
    server_.send(200, "application/json", body);
}

void WifiWebUi::loadNetworkConfig() {
    if (!preferences_.isKey("net_cfg")) return;
    StaticJsonDocument<512> document;
    const auto parsed = deserializeJson(document, preferences_.getString("net_cfg"));
    const auto result = parsed ? kc868a6::core::Result{kc868a6::core::ErrorCode::InvalidArgument, "net_cfg"} :
        kc868a6::config::readNetworkConfig(document.as<JsonVariantConst>(), networkConfig_);
    networkReady_ = result.ok();
    if (!result.ok()) {
        Serial.printf("ERROR: network configuration load failed: %s\n", kc868a6::core::errorCodeName(result.code));
    }
}

void WifiWebUi::handleNetworkConfig(bool save) {
    if (!preferencesReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::StorageUnavailable, "network"}, 503);
        return;
    }
    StaticJsonDocument<512> document;
    if (save) {
        if (server_.arg("plain").length() > 256 || deserializeJson(document, server_.arg("plain"))) {
            sendSettingsError({kc868a6::core::ErrorCode::InvalidArgument, "network_json"}, 400);
            return;
        }
        kc868a6::config::NetworkConfig candidate;
        const auto valid = kc868a6::config::readNetworkConfig(document.as<JsonVariantConst>(), candidate);
        if (!valid.ok()) { sendSettingsError(valid, 400); return; }
        document.clear();
        const auto written = kc868a6::config::writeNetworkConfig(document.to<JsonObject>(), candidate);
        if (!written.ok()) { sendSettingsError(written, 500); return; }
        String stored;
        serializeJson(document, stored);
        if (preferences_.putString("net_cfg", stored) != stored.length() ||
            preferences_.getString("net_cfg") != stored) {
            networkReady_ = false;
            sendSettingsError({kc868a6::core::ErrorCode::StorageWriteFailed, "net_cfg"}, 500);
            return;
        }
        const bool changed = !networkReady_ || networkConfig_.hostname != candidate.hostname;
        networkConfig_ = candidate;
        networkReady_ = true;
        if (changed) {
            MDNS.end();
            mdnsReady_ = false;
            reconnectPending_ = true;
            reconnectAt_ = millis() + 1000;
        }
    }
    if (!networkReady_) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "network_config"}, 503);
        return;
    }
    document.clear();
    const auto written = kc868a6::config::writeNetworkConfig(document.to<JsonObject>(), networkConfig_);
    if (!written.ok()) { sendSettingsError(written, 500); return; }
    document["mdnsReady"] = mdnsReady_;
    document["reconnectPending"] = reconnectPending_;
    document["udpListenAddress"] = "0.0.0.0";
    document["udpListenPort"] = settings().udpListenPort;
    document["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    String body;
    serializeJson(document, body);
    server_.send(200, "application/json", body);
}

void WifiWebUi::startMdns() {
    MDNS.end();
    mdnsRetryAt_ = millis() + 5000;
    mdnsReady_ = MDNS.begin(networkConfig_.hostname.data());
    if (!mdnsReady_) {
        Serial.println("ERROR: mDNS responder start failed");
        return;
    }
    MDNS.addService("http", "tcp", 80);
}

void WifiWebUi::handleRf24Packets(bool save) {
#if defined(USE_RADIO_NRF24)
    if (!save) {
        DynamicJsonDocument response(4096);
        response["compiled"] = true;
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
#else
    if (save) {
        sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "rf24_not_compiled"}, 503);
    } else {
        server_.send(200, "application/json",
                     "{\"compiled\":false,\"available\":false,\"history\":[],\"saved\":[]}");
    }
#endif
}

void WifiWebUi::handleRf24Replay() {
#if defined(USE_RADIO_NRF24)
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
#else
    sendSettingsError({kc868a6::core::ErrorCode::Unavailable, "rf24_not_compiled"}, 503);
#endif
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
    MDNS.end();
    mdnsReady_ = false;
    WiFi.disconnect(false, false);
    if (connectToWifi(ssid, password)) {
        startMdns();
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
    if (WiFi.status() != WL_CONNECTED || setupAccessPoint_) {
        if (mdnsReady_) MDNS.end();
        mdnsReady_ = false;
    } else if (!mdnsReady_ && networkReady_ && !reconnectPending_ &&
               static_cast<int32_t>(millis() - mdnsRetryAt_) >= 0) {
        startMdns();
    }
}