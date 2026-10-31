#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>

#include <functional>

struct DeviceSettings {
    uint16_t sensorIntervalMs = 1000;
    uint32_t i2cClockHz = 100000;
    uint32_t rs485Baud = 9600;
    uint32_t uart1Baud = 9600;
    uint8_t uart1Mode = 0;
    uint16_t beeperFrequencyHz = 2000;
    uint16_t beeperDurationMs = 250;
    uint8_t gpio32SensorType = 2;
    uint8_t gpio33SensorType = 0;
    uint8_t displayMode = 0;
    uint8_t displayPageSeconds = 5;
    uint8_t displayBrightness = 160;
    uint8_t displayRotation = 0;
    uint8_t displayTextSize = 1;
    uint8_t displayFieldMask = 0x3F;
    uint8_t bluetoothMode = 1;
    bool beeperEnabled = false;
    bool displayEnabled = true;
    bool s2InputEnabled = false;
    bool temperature1Enabled = true;
    bool temperature2Enabled = true;
    bool bme680Enabled = true;
    bool ens160Enabled = true;
    bool compassEnabled = true;
    bool gpsEnabled = false;
    bool rtcEnabled = true;
    bool pn532Enabled = true;
};

class WifiWebUi {
public:
    using StatusProvider = std::function<String()>;
    using RelayWriter = std::function<bool(uint8_t, bool)>;
    using SettingsApplier = std::function<void(const DeviceSettings &)>;
    using BeeperTester = std::function<bool()>;
    using IftttUpdater = std::function<void(const String &)>;
    using BluetoothScanner = std::function<String()>;
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
    const DeviceSettings &settings() const { return settings_; }

private:
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
    DeviceSettings settings_;
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
    void handleScan();
    void handleWifiSave();
    void handleRelay();
    void handleBeeperTest();
    void handleSettings(bool save);
    void handleIfttt(bool save);
    void handleBluetoothScan();
    void handleRf24Packets(bool save);
    void handleRf24Replay();
    void loadSettings();
    void reconnectWithSavedCredentials();
};