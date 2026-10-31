#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>

#include <functional>

#include "config/config_service.h"
#include "core/peripheral_registry.h"
#include "display/screen_layout.h"
#include "peripherals/ble_presence.h"
#include "peripherals/current_inputs.h"
#include "config/network_config.h"

class WifiWebUi {
public:
    WifiWebUi(kc868a6::config::ConfigService &configuration,
              kc868a6::core::PeripheralRegistry &registry)
        : configuration_(configuration), registry_(registry) {}

    using StatusProvider = std::function<String()>;
    using RelayWriter = std::function<bool(uint8_t, bool)>;
    using SettingsApplier = std::function<void(const DeviceSettings &)>;
    using BeeperTester = std::function<bool()>;
    using IftttUpdater = std::function<void(const String &)>;
    using BluetoothScanner = std::function<kc868a6::core::Result(String &)>;
    using RadioPacketSender = std::function<bool(uint32_t)>;
    using RadioPacketHistoryProvider = std::function<String()>;

    void begin(StatusProvider statusProvider, RelayWriter relayWriter,
               SettingsApplier settingsApplier, BeeperTester beeperTester,
               IftttUpdater iftttUpdater = {},
               BluetoothScanner bluetoothScanner = {},
               RadioPacketSender radioPacketSender = {},
               RadioPacketHistoryProvider radioPacketHistoryProvider = {});
    void prepare();
    void loop();
    const DeviceSettings &settings() const { return configuration_.settings(); }
    const kc868a6::display::ScreenLayout &screenLayout() const { return screenLayout_; }
    bool screenReady() const { return screenReady_; }
    void setBluetoothPresence(kc868a6::peripherals::BlePresence &presence) { blePresence_ = &presence; }
    void setCurrentInputs(kc868a6::peripherals::CurrentInputs &inputs) { currentInputs_ = &inputs; }

private:
    kc868a6::config::ConfigService &configuration_;
    kc868a6::core::PeripheralRegistry &registry_;
    kc868a6::display::ScreenLayout screenLayout_ = kc868a6::display::defaultLayout();
    bool screenReady_ = true;
    kc868a6::peripherals::BlePresence *blePresence_ = nullptr;
    bool bleTargetReady_ = true;
    kc868a6::peripherals::CurrentInputs *currentInputs_ = nullptr;
    bool currentReady_ = true;
    kc868a6::config::NetworkConfig networkConfig_;
    bool networkReady_ = true;
    bool mdnsReady_ = false;
    uint32_t mdnsRetryAt_ = 0;
    WebServer server_{80};
    DNSServer dnsServer_;
    Preferences preferences_;
    StatusProvider statusProvider_;
    RelayWriter relayWriter_;
    SettingsApplier settingsApplier_;
    BeeperTester beeperTester_;
    IftttUpdater iftttUpdater_;
    BluetoothScanner bluetoothScanner_;
    RadioPacketSender radioPacketSender_;
    RadioPacketHistoryProvider radioPacketHistoryProvider_;
    String webPassword_;
    String apSsid_;
    bool setupAccessPoint_ = false;
    bool preferencesReady_ = false;
    bool dnsActive_ = false;
    bool reconnectPending_ = false;
    uint32_t reconnectAt_ = 0;

    bool connectToWifi(const String &ssid, const String &password);
    void startSetupAccessPoint();
    void startServer();
    bool requireAuthentication();
    void serveFile(const String &path);
    void handleStatus();
    void handleBoard();
    void handleCapabilities();
    void handleAction();
    void handleSettingsSchema();
    void handleScreen(bool save);
    void loadScreen();
    void handleScan();
    void handleWifiSave();
    void handleRelay();
    void handleBeeperTest();
    void handleSettings(bool save);
    void handleIfttt(bool save);
    void handleBluetoothScan();
    void handleBluetoothTarget(bool save, bool multiple = false);
    void loadBluetoothTarget();
    void loadCurrentInputs();
    void handleCurrentInputs(bool save);
    void loadNetworkConfig();
    void handleNetworkConfig(bool save);
    void startMdns();
    void handleRf24Packets(bool save);
    void handleRf24Replay();
    void sendSettingsError(const kc868a6::core::Result &result, int status,
                           const kc868a6::core::ActionResult *action = nullptr);
    void reconnectWithSavedCredentials();
};