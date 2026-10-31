#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_BME680.h>
#include <DHT.h>
#include <Adafruit_PN532.h>
#include <Adafruit_SSD1306.h>
#include <DallasTemperature.h>
#include <LoRa.h>
#include <NimBLEDevice.h>
#include <RTClib.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <OneWire.h>
#include <Wire.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#include "kc868_a6_pins.h"
#include "wifi_web_ui.h"
#include "core/peripheral_registry.h"
#include "peripherals/analog_inputs.h"
#include "web/analog_status.h"
#include "config/preferences_settings_store.h"
#include "board/settings_resources.h"
#include "peripherals/board_io.h"
#include "peripherals/sensor_readings.h"
#include "web/capabilities.h"
#include "peripherals/fan_control.h"
#include "core/ifttt_condition.h"
#include "peripherals/esp32_udp_rules.h"
#include "web/ble_scan.h"
#include "peripherals/ble_presence.h"
#include "peripherals/esp32_ble_presence_scanner.h"
#include "peripherals/transport.h"
#include "peripherals/esp32_transport.h"
#include "peripherals/current_inputs.h"

#if defined(USE_RADIO_NRF24)
#include <RF24.h>
#endif

namespace {

constexpr uint8_t screenWidth = 128;
constexpr uint8_t screenHeight = 64;
constexpr uint32_t reportIntervalMs = 1000;
constexpr uint8_t eventLogCapacity = 24;
constexpr uint32_t s2DebounceMs = 120;

#if defined(USE_RADIO_NRF24)
constexpr uint64_t nrf24PipeAddress = 0xE8E8F0F0E1LL;
constexpr uint8_t rf24HistoryCapacity = 16;
struct CapturedRf24Packet {
    uint32_t payload = 0;
    uint32_t receivedAtMs = 0;
    uint32_t sequence = 0;
};
#endif

struct DeviceLogEvent {
    uint32_t timestampMs = 0;
    char message[88] = {};
};

Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire, -1);
Adafruit_BME680 bme680;
RTC_DS3231 rtc;
TinyGPSPlus gps;
Adafruit_PN532 pn532(kc868a6::gpio::pn532SpiCs, &SPI);
OneWire oneWire1(kc868a6::gpio::sensor1);
OneWire oneWire2(kc868a6::gpio::sensor2);
DallasTemperature temperature1(&oneWire1);
DallasTemperature temperature2(&oneWire2);
DHT dht32(kc868a6::gpio::sensor1, DHT22);
DHT dht33(kc868a6::gpio::sensor2, DHT22);
#if defined(USE_GPS)
constexpr auto defaultUart1Mode = kc868a6::config::Uart1Mode::Gps;
#elif defined(USE_ZIGBEE_THREAD_COPROCESSOR)
constexpr auto defaultUart1Mode = kc868a6::config::Uart1Mode::Coprocessor;
#else
constexpr auto defaultUart1Mode = kc868a6::config::Uart1Mode::Serial;
#endif
kc868a6::config::PreferencesSettingsStore settingsStore;
kc868a6::board::BoardSettingsValidator boardValidator{
    kc868a6::board::compiledBoardOptions()};
kc868a6::config::ConfigService configuration{
    settingsStore, kc868a6::config::defaultSettings(defaultUart1Mode), &boardValidator};
kc868a6::core::PeripheralRegistry peripheralRegistry;
WifiWebUi webUi{configuration, peripheralRegistry};

class Esp32AnalogInputHardware final
    : public kc868a6::peripherals::IAnalogInputHardware {
public:
    kc868a6::core::Result begin() override {
        analogReadResolution(12);
        for (uint8_t pin : kc868a6::gpio::analogInputs) pinMode(pin, INPUT);
        return {};
    }

    kc868a6::core::Result read(uint8_t pin, uint16_t &value) override {
        value = analogRead(pin);
        return {};
    }
};

Esp32AnalogInputHardware analogHardware;
kc868a6::peripherals::AnalogInputs analogInputs{
    analogHardware, {kc868a6::gpio::analogInputs[0], kc868a6::gpio::analogInputs[1],
                     kc868a6::gpio::analogInputs[2], kc868a6::gpio::analogInputs[3]}};
kc868a6::core::ActionResult writeRelayHardware(uint8_t channel, bool on);
bool readExpander(uint8_t address, uint8_t &state);
class Esp32BoardIoHardware final : public kc868a6::peripherals::IBoardIoHardware {
public:
    kc868a6::core::Result readInputs(uint8_t &bits) override {
        return readExpander(kc868a6::i2c::inputExpanderAddress, bits)
            ? kc868a6::core::Result{}
            : kc868a6::core::Result{kc868a6::core::ErrorCode::OperationFailed, "inputs"};
    }
    kc868a6::core::Result readRelays(uint8_t &bits) override {
        return readExpander(kc868a6::i2c::relayExpanderAddress, bits)
            ? kc868a6::core::Result{}
            : kc868a6::core::Result{kc868a6::core::ErrorCode::OperationFailed, "relays"};
    }
    kc868a6::core::ActionResult writeRelay(uint8_t channel, bool on) override {
        return writeRelayHardware(channel, on);
    }
};
Esp32BoardIoHardware ioHardware;
kc868a6::peripherals::BoardIo boardIo{ioHardware};
kc868a6::peripherals::SensorReadings sensorReadings;

class Esp32FanHardware final : public kc868a6::peripherals::IFanHardware {
public:
    kc868a6::core::Result configure(bool enabled, uint8_t pin, uint16_t frequency) override {
        if (attached_) {
            const bool stopped = ledcWrite(pin_, 0);
            const bool detached = ledcDetach(pin_);
            attached_ = !detached;
            if (!stopped || !detached) {
                return {kc868a6::core::ErrorCode::OperationFailed, "fan_detach"};
            }
            digitalWrite(pin_, LOW);
        }
        pin_ = pin;
        if (!enabled) return {};
        dacDisable(pin_);
        attached_ = ledcAttachChannel(pin_, frequency, 8, 4);
        if (!attached_ || !ledcWrite(pin_, 0)) {
            return {kc868a6::core::ErrorCode::OperationFailed, "fan_ledc"};
        }
        return {};
    }
    kc868a6::core::Result write(uint8_t percent) override {
        if (!attached_) return {kc868a6::core::ErrorCode::Unavailable, "fan_ledc"};
        return ledcWrite(pin_, (percent * 255U + 50U) / 100U)
            ? kc868a6::core::Result{}
            : kc868a6::core::Result{kc868a6::core::ErrorCode::OperationFailed, "fan_write"};
    }
private:
    bool attached_ = false;
    uint8_t pin_ = 25;
};
Esp32FanHardware fanHardware;
kc868a6::peripherals::FanControl fanControl{fanHardware};
kc868a6::peripherals::Esp32BlePresenceScanner bleScanner;
kc868a6::peripherals::BlePresence blePresence{bleScanner};
kc868a6::peripherals::Esp32TransportHardware transportHardware{Serial1, Serial2};
kc868a6::peripherals::Transport transport{transportHardware};
kc868a6::peripherals::CurrentInputs currentInputs{peripheralRegistry};
kc868a6::peripherals::Esp32UdpRulesHardware udpRulesHardware;
kc868a6::peripherals::UdpRules udpRules{udpRulesHardware};

#if defined(USE_RADIO_NRF24)
RF24 nrf24(kc868a6::gpio::nrf24Ce, kc868a6::gpio::spiCs);
#endif

bool displayReady = false;
bool bme680Ready = false;
bool ens160Ready = false;
bool rtcReady = false;
bool pn532Ready = false;
bool qmc5883lReady = false;
bool relayExpanderReady = false;
bool inputExpanderReady = false;
bool temperature1Ready = false;
bool temperature2Ready = false;
float sensorHumidity[2] = {NAN, NAN};
bool radioReady = false;
bool bluetoothReady = false;
uint8_t configuredBluetoothMode = 255;
#if defined(USE_RADIO_NRF24)
uint32_t lastRf24Packet = 0;
bool rf24PacketAvailable = false;
CapturedRf24Packet rf24History[rf24HistoryCapacity] = {};
uint8_t rf24HistoryNext = 0;
uint8_t rf24HistoryCount = 0;
uint32_t rf24Sequence = 0;
#endif
DeviceLogEvent eventLog[eventLogCapacity] = {};
uint8_t eventLogNext = 0;
uint8_t eventLogCount = 0;
uint8_t detectedI2cAddresses[112] = {};
uint8_t detectedI2cCount = 0;
bool beeperPwmAttached = false;
uint8_t configuredBeeperPin = 26;
bool beeperActive = false;
uint16_t configuredBeeperFrequencyHz = 0;
bool s2Pressed = false;
bool s2CandidatePressed = false;
uint32_t s2CandidateSinceMs = 0;
uint32_t beeperStopAt = 0;
uint8_t relayOutputBits = 0xFF;
uint8_t latestInputState = 0xFF;
const auto &analogReadings = analogInputs.rawValues();
float temperatureValues[2] = {NAN, NAN};
float airTemperature = NAN;
float airHumidity = NAN;
uint32_t airGasResistance = 0;
uint8_t airQualityIndex = 0;
uint16_t airTvoc = 0;
uint16_t airEco2 = 0;
bool airQualityAvailable = false;
float gpsLatitude = NAN;
float gpsLongitude = NAN;
uint32_t gpsSatellites = 0;
int16_t compassX = 0;
int16_t compassY = 0;
int16_t compassZ = 0;
uint32_t lastReportMs = 0;
uint32_t lastDhtReadMs[2] = {};
uint32_t lastDisplayPageMs = 0;
uint8_t currentDisplayPage = 1;
uint8_t iftttDisplayPage = 0;
String iftttRulesJson = "[]";
kc868a6::core::ConditionState iftttConditions[4] = {};
uint32_t lastIftttMs = 0;

void configureBluetooth(uint8_t mode);
kc868a6::core::Result scanBluetoothDevices(String &response);
#if defined(USE_RADIO_NRF24)
bool sendNrf24Packet(uint32_t payload);
#endif

void recordEvent(const String &message) {
    DeviceLogEvent &event = eventLog[eventLogNext];
    event.timestampMs = millis();
    snprintf(event.message, sizeof(event.message), "%s", message.c_str());
    eventLogNext = (eventLogNext + 1) % eventLogCapacity;
    if (eventLogCount < eventLogCapacity) ++eventLogCount;
}

void reportCoreError(const kc868a6::core::Result &result) {
    if (result.ok()) return;
    const String subject = result.subject.empty()
                               ? String()
                               : String(result.subject.data(), result.subject.size());
    const String message =
        String("Core: ") + kc868a6::core::errorCodeName(result.code) + " (" +
        subject + ")";
    Serial.println("ERROR: " + message);
    recordEvent(message);
}

#if defined(USE_RADIO_NRF24)
void captureRf24Packet(uint32_t payload) {
    rf24History[rf24HistoryNext] = {payload, millis(), ++rf24Sequence};
    rf24HistoryNext = (rf24HistoryNext + 1) % rf24HistoryCapacity;
    if (rf24HistoryCount < rf24HistoryCapacity) ++rf24HistoryCount;
}

String radioMonitorJson() {
    DynamicJsonDocument document(1536);
    document["available"] = radioReady;
    document["compiled"] = true;
    document["channel"] = 76;
    document["pipe"] = "E8E8F0F0E1";
    JsonArray packets = document["history"].to<JsonArray>();
    for (uint8_t offset = 0; offset < rf24HistoryCount; ++offset) {
        const uint8_t index = (rf24HistoryNext + rf24HistoryCapacity - 1 - offset) %
                              rf24HistoryCapacity;
        const CapturedRf24Packet &packet = rf24History[index];
        JsonObject item = packets.createNestedObject();
        item["payload"] = packet.payload;
        item["sequence"] = packet.sequence;
        item["ageMs"] = millis() - packet.receivedAtMs;
    }
    String result;
    serializeJson(document, result);
    return result;
}
#endif

bool writeExpander(uint8_t address, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readExpander(uint8_t address, uint8_t &value) {
    if (Wire.requestFrom(static_cast<int>(address), 1) != 1) {
        return false;
    }
    value = Wire.read();
    return true;
}

bool probeI2c(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void scanI2cDevicesAtStartup() {
    detectedI2cCount = 0;
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (probeI2c(address) && detectedI2cCount < sizeof(detectedI2cAddresses)) {
            detectedI2cAddresses[detectedI2cCount++] = address;
        }
    }
    Serial.printf("I2C startup scan: %u device(s)\n", detectedI2cCount);
}

bool wasI2cAddressDetected(uint8_t address) {
    for (uint8_t index = 0; index < detectedI2cCount; ++index) {
        if (detectedI2cAddresses[index] == address) return true;
    }
    return false;
}

bool writeI2cRegister(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readI2cRegisters(uint8_t address, uint8_t reg, uint8_t *data,
                      size_t length) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(static_cast<int>(address), static_cast<int>(length)) !=
            length) {
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        data[index] = Wire.read();
    }
    return true;
}

void initializeRelays() {
    const uint8_t offState = kc868a6::relayActiveLow ? 0xFF : 0x00;
    relayExpanderReady = writeExpander(kc868a6::i2c::relayExpanderAddress,
                                       offState);
    if (!relayExpanderReady) {
        Serial.println("ERROR: relay PCF8574 not found at 0x24");
        return;
    }
    relayOutputBits = offState;
    Serial.println("Relay outputs initialized OFF");
}

kc868a6::core::ActionResult writeRelayHardware(uint8_t channel, bool on) {
    if (channel >= kc868a6::relayCount) return {{kc868a6::core::ErrorCode::InvalidArgument, "channel"}};

    const uint8_t mask = static_cast<uint8_t>(1U << channel);
    uint8_t nextState = relayOutputBits;
    uint8_t hardwareState = 0;
    if (!readExpander(kc868a6::i2c::relayExpanderAddress, hardwareState)) {
        recordEvent("GPIO: relay read failed; write aborted.");
        return {{kc868a6::core::ErrorCode::OperationFailed, "relay_read"}};
    }
    relayOutputBits = hardwareState;
    nextState = hardwareState;
    nextState |= kc868a6::board::relayReservedPortMask;
    if (kc868a6::relayActiveLow == on) {
        nextState &= static_cast<uint8_t>(~mask);
    } else {
        nextState |= mask;
    }
    relayExpanderReady = writeExpander(kc868a6::i2c::relayExpanderAddress,
                                       nextState);
    if (!relayExpanderReady) {
        return {{kc868a6::core::ErrorCode::OperationFailed, "relay_write"}};
    }
    relayOutputBits = nextState;
    uint8_t verifiedState = 0;
    if (readExpander(kc868a6::i2c::relayExpanderAddress, verifiedState)) {
        relayOutputBits = verifiedState;
        relayExpanderReady = true;
        const bool verifiedOn = kc868a6::relayActiveLow
                                    ? (verifiedState & mask) == 0
                                    : (verifiedState & mask) != 0;
        if (verifiedOn != on) {
            recordEvent("GPIO: błąd weryfikacji przekaźnika " + String(channel + 1));
            return {{kc868a6::core::ErrorCode::OperationFailed, "relay_readback"}, true, false};
        }
    } else {
        recordEvent("GPIO: relay write applied but readback failed.");
        return {{kc868a6::core::ErrorCode::OperationFailed, "relay_readback"}, true, false};
    }
    recordEvent("GPIO: przekaźnik " + String(channel + 1) +
                (on ? " włączony" : " wyłączony"));
    return {{}, true, true};
}

bool setRelay(uint8_t channel, bool on) {
    std::array<kc868a6::core::Parameter, 2> parameters{{
        {"channel", static_cast<uint32_t>(channel + 1)}, {"on", on},
    }};
    const auto action = peripheralRegistry.execute("io.main/relay/set", parameters);
    reportCoreError(action.result);
    return action.result.ok() && action.applied;
}

String deviceStatusJson() {
    DynamicJsonDocument document(6144);
    JsonObject configStatus = document["configuration"].to<JsonObject>();
    if (configStatus.isNull() ||
        !configStatus["ready"].set(configuration.ready()) ||
        !configStatus["error"].set(
            kc868a6::core::errorCodeName(configuration.status().code))) {
        reportCoreError({kc868a6::core::ErrorCode::CapacityExceeded,
                         "configuration_status"});
        return String();
    }
    document["relays"].to<JsonArray>();
    JsonArray relays = document["relays"].as<JsonArray>();
    for (uint8_t channel = 0; channel < kc868a6::relayCount; ++channel) {
        const bool pinHigh = (relayOutputBits & (1U << channel)) != 0;
        relays.add(kc868a6::relayActiveLow ? !pinHigh : pinHigh);
    }

    JsonArray inputs = document["inputs"].to<JsonArray>();
    for (uint8_t channel = 0; channel < kc868a6::digitalInputCount; ++channel) {
        inputs.add((latestInputState & (1U << channel)) == 0);
    }

    JsonArray analog = document["analog"].to<JsonArray>();
    const auto analogStatus = kc868a6::web::appendAnalogStatus(analog, analogInputs);
    if (!analogStatus.ok()) {
        reportCoreError(analogStatus);
        return String();
    }

    JsonArray temperatures = document["temperatures"].to<JsonArray>();
    temperatures.add(temperatureValues[0] > -100 ? temperatureValues[0] : NAN);
    temperatures.add(temperatureValues[1] > -100 ? temperatureValues[1] : NAN);
    JsonArray sensorHumidityValues = document["sensorHumidity"].to<JsonArray>();
    sensorHumidityValues.add(sensorHumidity[0]);
    sensorHumidityValues.add(sensorHumidity[1]);

    JsonObject air = document["air"].to<JsonObject>();
    air["temperature"] = airTemperature;
    air["humidity"] = airHumidity;
    air["gas"] = airGasResistance;
    air["aqi"] = airQualityIndex;
    air["tvoc"] = airTvoc;
    air["eco2"] = airEco2;
    air["qualityAvailable"] = airQualityAvailable;

    JsonObject location = document["gps"].to<JsonObject>();
    location["valid"] = isfinite(gpsLatitude) && isfinite(gpsLongitude);
    location["latitude"] = gpsLatitude;
    location["longitude"] = gpsLongitude;
    location["satellites"] = gpsSatellites;

    JsonArray compass = document["compass"].to<JsonArray>();
    compass.add(compassX);
    compass.add(compassY);
    compass.add(compassZ);

    const DeviceSettings &settings = webUi.settings();
    JsonArray i2cDetected = document["i2cDetected"].to<JsonArray>();
    for (uint8_t index = 0; index < detectedI2cCount; ++index) {
        i2cDetected.add(detectedI2cAddresses[index]);
    }
    JsonArray devices = document["devices"].to<JsonArray>();
    const auto addDevice = [&devices](const char *id, const char *name,
                                      const char *interfaceName, bool connected) {
        JsonObject device = devices.createNestedObject();
        device["id"] = id;
        device["name"] = name;
        device["interface"] = interfaceName;
        device["connected"] = connected;
        device["detected"] = connected;
        return device;
    };
    addDevice("i2c", "I2C bus", "GPIO4 / GPIO15", true);
    addDevice("relayExpander", "Relay expander", "I2C 0x24", relayExpanderReady);
    addDevice("inputExpander", "Input expander", "I2C 0x22", inputExpanderReady);
    addDevice("oled", "SSD1306 OLED", "I2C 0x3C · SDA GPIO4 / SCL GPIO15", displayReady)
        ["detected"] = wasI2cAddressDetected(kc868a6::i2c::oledAddress);
    addDevice("temperature1", settings.gpio32SensorType == 3 ? "DHT11" :
              settings.gpio32SensorType == 2 ? "DHT22" : "DS18B20",
              "GPIO32", temperature1Ready && isfinite(temperatureValues[0]));
    addDevice("temperature2", settings.gpio33SensorType == 3 ? "DHT11" :
              settings.gpio33SensorType == 2 ? "DHT22" : "DS18B20",
              "GPIO33", temperature2Ready && isfinite(temperatureValues[1]));
    addDevice("rtc", "DS3231 RTC", "I2C 0x68", rtcReady);
    addDevice("bme680", "BME680", "I2C 0x77 · SDA GPIO4 / SCL GPIO15", bme680Ready);
    addDevice("ens160", "ENS160", "I2C 0x53 · SDA GPIO4 / SCL GPIO15", ens160Ready);
    addDevice("compass", "QMC5883L", "I2C 0x0D · SDA GPIO4 / SCL GPIO15", qmc5883lReady);
    addDevice("pn532", "PN532 NFC",
              "SPI · CS13 / SCK18 / MISO19 / MOSI23", pn532Ready)
        ["detected"] = pn532Ready || !settings.pn532Enabled;
    addDevice("uart1", "UART 1", "GPIO16 / GPIO17", true);
    addDevice("rs485", "RS-485", "UART 2 · GPIO14/27", true);
    addDevice("spi", "SPI bus", "GPIO18/19/23", true);
    addDevice("buttonS2", "S2 button", "GPIO0 · BOOT", settings.s2InputEnabled);
    const char *bluetoothInterface = settings.bluetoothMode == 1
                                         ? "BLE scanner"
                                         : settings.bluetoothMode == 2 ? "BLE beacon" : "off";
    addDevice("bluetooth", "Bluetooth LE", bluetoothInterface, bluetoothReady);
#if defined(USE_RADIO_LORA)
    addDevice("radio", "SX1278 LoRa", "SPI · CS GPIO5", radioReady);
#elif defined(USE_RADIO_NRF24)
    addDevice("radio", "nRF24L01", "SPI · CE GPIO22", radioReady);
#else
    addDevice("radio", "Radio module", "SPI · not selected", false);
#endif
    addDevice("gps", "GPS receiver", "UART 1",
              settings.gpsEnabled && settings.uart1Mode == 1 &&
                  gps.location.isValid());

    JsonObject configuration = document["sensorConfig"].to<JsonObject>();
    configuration["temperature1Enabled"] = settings.temperature1Enabled;
    configuration["temperature2Enabled"] = settings.temperature2Enabled;
    configuration["bme680Enabled"] = settings.bme680Enabled;
    configuration["ens160Enabled"] = settings.ens160Enabled;
    configuration["compassEnabled"] = settings.compassEnabled;
    configuration["gpsEnabled"] = settings.gpsEnabled;
    configuration["rtcEnabled"] = settings.rtcEnabled;
    configuration["pn532Enabled"] = settings.pn532Enabled;
    configuration["s2InputEnabled"] = settings.s2InputEnabled;
    configuration["gpio32SensorType"] = settings.gpio32SensorType;
    configuration["gpio33SensorType"] = settings.gpio33SensorType;
    configuration["displayEnabled"] = settings.displayEnabled;
    configuration["displayMode"] = settings.displayMode;
    configuration["displayPageSeconds"] = settings.displayPageSeconds;

    JsonObject button = document["buttonS2"].to<JsonObject>();
    button["enabled"] = settings.s2InputEnabled;
    button["pressed"] = settings.s2InputEnabled && s2Pressed;

    JsonObject beeper = document["beeper"].to<JsonObject>();
    beeper["enabled"] = settings.beeperEnabled;
    beeper["active"] = beeperActive;
    beeper["ready"] = beeperPwmAttached;
    beeper["frequencyHz"] = settings.beeperFrequencyHz;
    beeper["durationMs"] = settings.beeperDurationMs;

#if defined(USE_RADIO_NRF24)
    document["features"]["rf24"] = true;
    JsonObject radio = document["rf24"].to<JsonObject>();
    radio["available"] = radioReady;
    radio["packetAvailable"] = rf24PacketAvailable;
    radio["lastPacket"] = lastRf24Packet;
    radio["receivedCount"] = rf24Sequence;
    radio["pipe"] = "E8E8F0F0E1";
#else
    document["features"]["rf24"] = false;
#endif
    document["memory"]["freeHeap"] = ESP.getFreeHeap();
    document["memory"]["minFreeHeap"] = ESP.getMinFreeHeap();
    auto hardware = document["hardware"].to<JsonObject>();
    hardware["cpu"]["model"] = ESP.getChipModel();
    hardware["cpu"]["cores"] = ESP.getChipCores();
    hardware["cpu"]["frequencyMHz"] = ESP.getCpuFreqMHz();
    hardware["cpu"]["revision"] = ESP.getChipRevision();
    hardware["ram"]["heapBytes"] = ESP.getHeapSize();
    hardware["ram"]["freeBytes"] = ESP.getFreeHeap();
    hardware["ram"]["minFreeBytes"] = ESP.getMinFreeHeap();
    hardware["psram"]["totalBytes"] = ESP.getPsramSize();
    hardware["psram"]["freeBytes"] = ESP.getFreePsram();
    hardware["flash"]["totalBytes"] = ESP.getFlashChipSize();
    hardware["flash"]["frequencyHz"] = ESP.getFlashChipSpeed();
    hardware["flash"]["firmwareBytes"] = ESP.getSketchSize();
    hardware["flash"]["otaSlotBytes"] = ESP.getFreeSketchSpace();
    hardware["firmwareVersion"] = KINCONDUCTOR_VERSION;
    document["beeper"]["pin"] = settings.beeperPin;

    JsonObject bluetooth = document["bluetooth"].to<JsonObject>();
    bluetooth["mode"] = settings.bluetoothMode;
    bluetooth["ready"] = bluetoothReady;

    JsonArray events = document["events"].to<JsonArray>();
    for (uint8_t offset = 0; offset < eventLogCount; ++offset) {
        const uint8_t index = (eventLogNext + eventLogCapacity - 1 - offset) %
                              eventLogCapacity;
        JsonObject event = events.createNestedObject();
        event["ageMs"] = millis() - eventLog[index].timestampMs;
        event["message"] = eventLog[index].message;
    }

    String result;
    if (document.overflowed()) {
        Serial.println("ERROR: device status exceeds JSON capacity");
        return "";
    }
    serializeJson(document, result);
    return result;
}

void initializeInputs() {
    inputExpanderReady = writeExpander(kc868a6::i2c::inputExpanderAddress,
                                       0xFF);
    if (!inputExpanderReady) {
        Serial.println("ERROR: input PCF8574 not found at 0x22");
        return;
    }
    Serial.println("Digital input expander initialized");
}

void configureTemperaturePin(uint8_t sensorType, uint8_t pin,
                             DallasTemperature &oneWireSensor, DHT &dhtSensor,
                             bool &ready) {
    ready = false;
    if (sensorType == 1) {
        oneWireSensor.begin();
        oneWireSensor.setWaitForConversion(false);
        ready = oneWireSensor.getDeviceCount() > 0;
    } else if (sensorType == 2 || sensorType == 3) {
        pinMode(pin, INPUT);
        dhtSensor = DHT(pin, sensorType == 3 ? DHT11 : DHT22);
        dhtSensor.begin();
        ready = true;
        const uint8_t sensorIndex = pin == kc868a6::gpio::sensor1 ? 0 : 1;
        lastDhtReadMs[sensorIndex] = millis() - 2100;
    }
}

void configureBeeper(const DeviceSettings &settings) {
    if (beeperPwmAttached &&
        (!settings.beeperEnabled || configuredBeeperPin != settings.beeperPin ||
         configuredBeeperFrequencyHz != settings.beeperFrequencyHz)) {
        ledcWriteTone(configuredBeeperPin, 0);
        ledcWrite(configuredBeeperPin, 0);
        if (!ledcDetach(configuredBeeperPin)) {
            Serial.println("ERROR: beeper LEDC detach failed");
            recordEvent("Buzzer: LEDC detach failed.");
            return;
        }
        digitalWrite(configuredBeeperPin, LOW);
        beeperPwmAttached = false;
        beeperActive = false;
    }
    if (!settings.beeperEnabled) {
        configuredBeeperFrequencyHz = 0;
        beeperActive = false;
        return;
    }
    if (!beeperPwmAttached) {
        configuredBeeperPin = settings.beeperPin;
        dacDisable(configuredBeeperPin);
        beeperPwmAttached = ledcAttachChannel(configuredBeeperPin,
                                       settings.beeperFrequencyHz, 8, 6);
        if (!beeperPwmAttached) {
            Serial.println("ERROR: could not attach LEDC to beeper");
            recordEvent("Buzzer: błąd inicjalizacji LEDC.");
            return;
        }
        configuredBeeperFrequencyHz = settings.beeperFrequencyHz;
    }
    if (!beeperActive) ledcWrite(configuredBeeperPin, 0);
}

bool setDisplayContrast(uint8_t contrast) {
    Wire.beginTransmission(kc868a6::i2c::oledAddress);
    Wire.write(0x00);
    Wire.write(0x81);
    Wire.write(contrast);
    return Wire.endTransmission() == 0;
}

void initializeDisplay() {
    displayReady = webUi.settings().displayEnabled &&
                   display.begin(SSD1306_SWITCHCAPVCC,
                                 kc868a6::i2c::oledAddress);
    if (displayReady) {
        setDisplayContrast(webUi.settings().displayBrightness);
        display.setRotation(webUi.settings().displayRotation);
        display.setTextSize(webUi.settings().displayTextSize);
        display.setTextWrap(false);
    }
}

void initializeOptionalDevices() {
    initializeDisplay();
    if (displayReady) {
        display.clearDisplay();
        display.setTextSize(webUi.settings().displayTextSize);
        display.setTextWrap(false);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("KC868-A6 ready");
        display.display();
    }

    configureTemperaturePin(webUi.settings().gpio32SensorType,
                            kc868a6::gpio::sensor1, temperature1, dht32,
                            temperature1Ready);
    configureTemperaturePin(webUi.settings().gpio33SensorType,
                            kc868a6::gpio::sensor2, temperature2, dht33,
                            temperature2Ready);

    rtcReady = rtc.begin(&Wire);
    bme680Ready = bme680.begin(kc868a6::i2c::bme680Address, &Wire);
    if (bme680Ready) {
        bme680.setTemperatureOversampling(BME680_OS_8X);
        bme680.setHumidityOversampling(BME680_OS_2X);
        bme680.setPressureOversampling(BME680_OS_4X);
        bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme680.setGasHeater(320, 150);
    }

    uint8_t ens160PartId[2] = {};
    ens160Ready = readI2cRegisters(kc868a6::i2c::ens160Address, 0x00,
                                   ens160PartId, sizeof(ens160PartId)) &&
                  ens160PartId[0] == 0x60 && ens160PartId[1] == 0x01;
    if (ens160Ready && !writeI2cRegister(kc868a6::i2c::ens160Address,
                                         0x10, 0x02)) {
        ens160Ready = false;
    }

    qmc5883lReady = probeI2c(kc868a6::i2c::qmc5883lAddress);
    if (qmc5883lReady) {
        writeI2cRegister(kc868a6::i2c::qmc5883lAddress, 0x0B, 0x01);
        writeI2cRegister(kc868a6::i2c::qmc5883lAddress, 0x09, 0x1D);
    }

    if (webUi.settings().pn532Enabled) {
        pn532.begin();
        pn532Ready = pn532.getFirmwareVersion() != 0;
        if (pn532Ready) pn532.SAMConfig();
    } else {
        pn532Ready = false;
    }

    Serial.printf("OLED:%s RTC:%s BME680:%s ENS160:%s QMC5883L:%s PN532(SPI):%s\n",
                  displayReady ? "OK" : "--", rtcReady ? "OK" : "--",
                  bme680Ready ? "OK" : "--", ens160Ready ? "OK" : "--",
                  qmc5883lReady ? "OK" : "--", pn532Ready ? "OK" : "--");
}

void initializePeripherals() {
    Serial.begin(115200);

    Wire.begin(kc868a6::gpio::i2cSda, kc868a6::gpio::i2cScl);
    scanI2cDevicesAtStartup();
    initializeRelays();
    initializeInputs();

#if defined(USE_GPS)
    Serial1.begin(9600, SERIAL_8N1, kc868a6::gpio::rs232Rx,
                  kc868a6::gpio::rs232Tx);
#elif defined(USE_ZIGBEE_THREAD_COPROCESSOR)
    Serial1.begin(115200, SERIAL_8N1, kc868a6::gpio::rs232Rx,
                  kc868a6::gpio::rs232Tx);
#else
    Serial1.begin(9600, SERIAL_8N1, kc868a6::gpio::rs232Rx,
                  kc868a6::gpio::rs232Tx);
#endif
    Serial2.begin(9600, SERIAL_8N1, kc868a6::gpio::rs485Rx,
                  kc868a6::gpio::rs485Tx);

    const auto registered = peripheralRegistry.add(
        {&analogInputs, &analogInputs, &analogInputs, nullptr});
    reportCoreError(registered);
    reportCoreError(peripheralRegistry.add({&boardIo, &boardIo, &boardIo, &boardIo}));
    reportCoreError(peripheralRegistry.add({&sensorReadings, &sensorReadings, &sensorReadings, nullptr}));
    reportCoreError(peripheralRegistry.add({&fanControl, &fanControl, &fanControl, &fanControl}));
    reportCoreError(peripheralRegistry.add({&blePresence, &blePresence, &blePresence, nullptr}));
    reportCoreError(peripheralRegistry.add({&transport, &transport, &transport, &transport}));
    reportCoreError(peripheralRegistry.add({&currentInputs, &currentInputs, &currentInputs, nullptr}));
    reportCoreError(peripheralRegistry.add({&udpRules, &udpRules, &udpRules, nullptr}));
    if (registered.ok()) reportCoreError(peripheralRegistry.begin(millis()));

    pinMode(kc868a6::gpio::sensor1, INPUT);
    pinMode(kc868a6::gpio::sensor2, INPUT);
    dacWrite(kc868a6::gpio::dac1, 0);
    dacWrite(kc868a6::gpio::dac2, 0);

    SPI.begin(kc868a6::gpio::spiSck, kc868a6::gpio::spiMiso,
              kc868a6::gpio::spiMosi, kc868a6::gpio::spiCs);

#if defined(USE_RADIO_LORA)
    LoRa.setPins(kc868a6::gpio::spiCs, kc868a6::gpio::loraReset,
                 kc868a6::gpio::loraDio0);
#ifndef LORA_FREQUENCY_HZ
#define LORA_FREQUENCY_HZ 433000000UL
#endif
    radioReady = LoRa.begin(LORA_FREQUENCY_HZ);
    if (!radioReady) {
        Serial.println("ERROR: SX1278 not found; check module frequency/wiring");
    }
#elif defined(USE_RADIO_NRF24)
    radioReady = nrf24.begin();
    if (!radioReady) {
        Serial.println("ERROR: nRF24L01 not found");
    } else {
        nrf24.setPALevel(RF24_PA_LOW);
        nrf24.setDataRate(RF24_1MBPS);
        nrf24.setChannel(76);
        nrf24.setPayloadSize(sizeof(uint32_t));
        nrf24.openReadingPipe(1, nrf24PipeAddress);
        nrf24.startListening();
    }
#endif

    initializeOptionalDevices();
}

void applyDeviceSettings(const DeviceSettings &settings) {
    recordEvent("GPIO: zapis konfiguracji czujników GPIO32=" +
                String(settings.gpio32SensorType) + ", GPIO33=" +
                String(settings.gpio33SensorType) + ".");
    Wire.setClock(settings.i2cClockHz);
    Serial1.end();
    Serial1.begin(settings.uart1Baud, SERIAL_8N1,
                  kc868a6::gpio::rs232Rx, kc868a6::gpio::rs232Tx);
    Serial2.end();
    Serial2.begin(settings.rs485Baud, SERIAL_8N1,
                  kc868a6::gpio::rs485Rx, kc868a6::gpio::rs485Tx);
    kc868a6::peripherals::TransportConfig transportConfig;
    transportConfig.udpListenPort = settings.udpListenPort;
    transportConfig.serial1Reserved = settings.uart1Mode != 0;
    reportCoreError(transport.configure(transportConfig, millis()));
    reportCoreError(blePresence.setTimeoutMs(settings.blePresenceTimeoutSeconds * 1000UL));

    configureTemperaturePin(settings.gpio32SensorType, kc868a6::gpio::sensor1,
                            temperature1, dht32, temperature1Ready);
    configureTemperaturePin(settings.gpio33SensorType, kc868a6::gpio::sensor2,
                            temperature2, dht33, temperature2Ready);
    if (settings.displayEnabled && !displayReady) initializeDisplay();
    if (!settings.displayEnabled) displayReady = false;
    configureBluetooth(settings.bluetoothMode);
    if (displayReady && settings.displayEnabled) {
        setDisplayContrast(settings.displayBrightness);
        display.setRotation(settings.displayRotation);
        display.setTextSize(settings.displayTextSize);
        display.setTextWrap(false);
    }

    pinMode(kc868a6::gpio::s2Button,
            settings.s2InputEnabled ? INPUT_PULLUP : INPUT);
    if (!settings.s2InputEnabled) {
        s2Pressed = false;
        s2CandidatePressed = false;
    }
    reportCoreError(fanControl.configure(false, settings.fanPin, settings.fanFrequencyHz, millis()));
    configureBeeper(settings);
    reportCoreError(fanControl.configure(settings.fanEnabled, settings.fanPin,
                                         settings.fanFrequencyHz, millis()));
}

bool startBeeperTest() {
    const DeviceSettings &settings = webUi.settings();
    if (!settings.beeperEnabled) {
        recordEvent("Buzzer: test pominięty, DA-1 wyłączone.");
        return false;
    }
    configureBeeper(settings);
    if (!beeperPwmAttached) {
        recordEvent("Buzzer: test nie wystartował, brak LEDC GPIO26.");
        return false;
    }

    ledcWriteTone(configuredBeeperPin, settings.beeperFrequencyHz);
    beeperActive = true;
    beeperStopAt = millis() + settings.beeperDurationMs;
    recordEvent("Buzzer: test uruchomiony.");
    return true;
}

void configureBluetooth(uint8_t mode) {
    if (configuredBluetoothMode == mode && (mode == 0 || bluetoothReady)) return;
    blePresence.setScanAvailable(false, millis());
    if (NimBLEDevice::isInitialized() && !NimBLEDevice::deinit(true)) {
        bluetoothReady = false;
        reportCoreError({kc868a6::core::ErrorCode::OperationFailed, "ble_deinit"});
        return;
    }
    bluetoothReady = false;
    configuredBluetoothMode = mode;
    if (mode == 0) return;

    bluetoothReady = NimBLEDevice::init("KC868-A6");
    if (!bluetoothReady) {
        reportCoreError({kc868a6::core::ErrorCode::OperationFailed, "ble_init"});
        return;
    }
    if (mode == 2) {
        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        advertising->setName("KC868-A6");
        if (!advertising->start()) {
            reportCoreError({kc868a6::core::ErrorCode::OperationFailed, "ble_advertise"});
            bluetoothReady = false;
        }
    }
    blePresence.setScanAvailable(mode == 1 && bluetoothReady, millis());
}

kc868a6::core::Result scanBluetoothDevices(String &response) {
    if (webUi.settings().bluetoothMode != 1 || !bluetoothReady) {
        return {kc868a6::core::ErrorCode::Unavailable, "ble_scan_disabled"};
    }
    kc868a6::peripherals::BleScanSnapshot scan;
    auto result = blePresence.scanResults(millis(), scan);
    if (!result.ok()) return result;
    DynamicJsonDocument document(kc868a6::web::bleScanJsonCapacity);
    result = kc868a6::web::writeBleScan(document.to<JsonObject>(), scan);
    if (!result.ok()) return result;
    const size_t length = measureJson(document);
    if (!response.reserve(length) || serializeJson(document, response) != length || response.length() != length) {
        return {kc868a6::core::ErrorCode::CapacityExceeded, "ble_scan_response"};
    }
    return {};
}

#if defined(USE_RADIO_NRF24)
bool sendNrf24Packet(uint32_t payload) {
    if (!radioReady) return false;
    nrf24.stopListening();
    nrf24.openWritingPipe(nrf24PipeAddress);
    const bool sent = nrf24.write(&payload, sizeof(payload));
    nrf24.startListening();
    return sent;
}
#endif

double iftttValue(const char *source) {
    if (peripheralRegistry.findProperty(source)) {
        kc868a6::core::PropertySnapshot snapshot;
        const auto result = peripheralRegistry.readProperty(source, millis(), snapshot);
        if (!result.ok()) reportCoreError(result);
        if (!result.ok() || snapshot.quality != kc868a6::core::Quality::Valid) return NAN;
        return std::visit([](const auto &value) -> double {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_arithmetic_v<T>) return static_cast<double>(value);
            return NAN;
        }, snapshot.value);
    }
    if (strcmp(source, "temperature1") == 0) return temperatureValues[0];
    if (strcmp(source, "temperature2") == 0) return temperatureValues[1];
    if (strcmp(source, "bmeTemperature") == 0) return airTemperature;
    if (strcmp(source, "humidity") == 0) {
        if (isfinite(sensorHumidity[0])) return sensorHumidity[0];
        if (isfinite(sensorHumidity[1])) return sensorHumidity[1];
        return airHumidity;
    }
    if (strcmp(source, "airQuality") == 0) {
        return airQualityAvailable ? airQualityIndex : NAN;
    }
    if (strcmp(source, "buttonS2") == 0) {
        return webUi.settings().s2InputEnabled ? (s2Pressed ? 1.0F : 0.0F) : NAN;
    }
    if (strcmp(source, "rf24Packet") == 0) {
#if defined(USE_RADIO_NRF24)
        return rf24PacketAvailable ? static_cast<double>(lastRf24Packet) : NAN;
#else
        return NAN;
#endif
    }
    if (strncmp(source, "digital", 7) == 0) {
        const int channel = atoi(source + 7) - 1;
        if (channel >= 0 && channel < kc868a6::digitalInputCount) {
            return (latestInputState & (1U << channel)) == 0 ? 1.0F : 0.0F;
        }
    }
    if (strncmp(source, "analog", 6) == 0) {
        const int channel = atoi(source + 6) - 1;
        if (channel >= 0 && channel < kc868a6::analogInputCount) {
            kc868a6::core::PropertySnapshot snapshot;
            const auto descriptors = analogInputs.properties();
            const auto result = peripheralRegistry.readProperty(
                descriptors.data[channel].id, millis(), snapshot);
            if (!result.ok()) reportCoreError(result);
            if (result.ok() && snapshot.quality == kc868a6::core::Quality::Valid) {
                if (const auto *value = std::get_if<uint32_t>(&snapshot.value)) {
                    return *value;
                }
            }
            return NAN;
        }
    }
    return NAN;
}

void updateIftttRules(const String &rules) {
    iftttRulesJson = rules;
    for (auto &condition : iftttConditions) condition = {};
    DynamicJsonDocument document(8192);
    if (deserializeJson(document, rules) || !document.is<JsonArray>()) {
        udpRules.configure({});
        reportCoreError({kc868a6::core::ErrorCode::InvalidArgument, "ifttt_stored_rules"});
        return;
    }
    kc868a6::peripherals::UdpRulesConfig udpConfig;
    const auto parsed = kc868a6::peripherals::readUdpRules(
        document.as<JsonVariantConst>(), webUi.settings().udpListenPort, udpConfig);
    reportCoreError(parsed);
    udpRules.configure(parsed.ok() ? udpConfig : kc868a6::peripherals::UdpRulesConfig{});
    const uint8_t count = document.as<JsonArray>().size();
    recordEvent("IFTTT: aktywne reguły " + String(count) + ".");
}

void evaluateIftttRules() {
    reportCoreError(udpRules.tick(millis()));
    DynamicJsonDocument document(8192);
    if (deserializeJson(document, iftttRulesJson) || !document.is<JsonArray>()) {
        reportCoreError({kc868a6::core::ErrorCode::InvalidArgument, "ifttt_runtime_rules"});
        return;
    }
    uint8_t ruleIndex = 0;
    for (JsonObject rule : document.as<JsonArray>()) {
        if (ruleIndex >= 4) break;
        uint8_t fires = 0;
        const uint8_t udpMatches = udpRules.consume(ruleIndex);
        if (rule["enabled"] | false) {
            if (strcmp(rule["source"] | "", "udpPacket") == 0) {
                fires = udpMatches;
            } else {
                const double value = iftttValue(rule["source"] | "");
                const double threshold = rule["threshold"] | 0.0;
                fires = iftttConditions[ruleIndex].fires(value, threshold, rule["comparison"] | "") ? 1 : 0;
            }
        } else {
            iftttConditions[ruleIndex] = {};
        }
        for (uint8_t firing = 0; firing < fires; ++firing) {
            const String ruleName = "Reguła " + String(ruleIndex + 1);
            recordEvent("IFTTT: " + ruleName + " wyzwolona.");
            const char *action = rule["action"] | "relay";
            bool actionSucceeded = true;
            if (strcmp(action, "capability") == 0) {
                const auto result = kc868a6::web::executeJsonAction(
                    peripheralRegistry, rule["command"]);
                reportCoreError(result.result);
                actionSucceeded = result.result.ok() && result.applied;
            } else if (strcmp(action, "rf24Send") == 0 ||
                strcmp(action, "rf24Saved") == 0) {
#if defined(USE_RADIO_NRF24)
                actionSucceeded = sendNrf24Packet(rule["payload"] | 0U);
#else
                actionSucceeded = false;
                reportCoreError({kc868a6::core::ErrorCode::Unavailable, "rf24_not_compiled"});
#endif
            } else if (strcmp(action, "beeper") == 0) {
                actionSucceeded = startBeeperTest();
            } else if (strcmp(action, "oledPage") == 0) {
                const uint8_t page = rule["page"] | 0;
                if (page >= 1 && page <= 3) iftttDisplayPage = page;
                else actionSucceeded = false;
            } else {
                const int relay = rule["relay"] | 0;
                if (relay >= 1 && relay <= kc868a6::relayCount) {
                    actionSucceeded = setRelay(static_cast<uint8_t>(relay - 1),
                                               rule["on"] | false);
                } else {
                    actionSucceeded = false;
                }
            }
            if (!actionSucceeded) {
                recordEvent("IFTTT: akcja reguły " + String(ruleIndex + 1) +
                            " nie powiodła się.");
            }
        }
        ++ruleIndex;
    }
}

void renderOled(uint8_t inputState) {
    const DeviceSettings &settings = webUi.settings();
    if (!displayReady || !settings.displayEnabled) return;
    if (settings.displayMode == 5 && iftttDisplayPage == 0) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextWrap(false);
        if (!webUi.screenReady()) {
            display.setCursor(0, 0);
            display.print("Screen config error");
            display.display();
            return;
        }
        const auto &layout = webUi.screenLayout();
        const uint8_t active = (millis() / (settings.displayPageSeconds * 1000UL)) % layout.pageCount;
        for (size_t index = 0; index < layout.widgetCount; ++index) {
            const auto &widget = layout.widgets[index];
            if (widget.page != active) continue;
            const auto cell = kc868a6::display::widgetCell(layout.pages[active], widget,
                                                         display.width(), display.height());
            kc868a6::core::PropertySnapshot snapshot;
            const auto read = peripheralRegistry.readProperty(widget.property.data(), millis(), snapshot);
            reportCoreError(read);
            String value = "--";
            if (read.ok() && snapshot.quality == kc868a6::core::Quality::Valid) {
                value = std::visit([](const auto &item) -> String {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, std::string_view>) {
                        return String(item.data(), item.size());
                    } else if constexpr (std::is_same_v<T, bool>) {
                        return item ? "ON" : "OFF";
                    } else {
                        return String(item);
                    }
                }, snapshot.value);
            }
            const auto *property = peripheralRegistry.findProperty(widget.property.data());
            const std::string_view unit = widget.unit[0] ? std::string_view(widget.unit.data()) :
                property ? property->unit : std::string_view{};
            if (!unit.empty()) value += " " + String(unit.data(), unit.size());
            GFXcanvas1 canvas(cell.width, cell.height);
            if (!canvas.getBuffer()) {
                reportCoreError({kc868a6::core::ErrorCode::CapacityExceeded, "screen_canvas"});
                continue;
            }
            canvas.setTextColor(1);
            canvas.setTextWrap(false);
            canvas.setCursor(0, 0);
            canvas.print(widget.label.data());
            canvas.setFont(widget.font == 1 ? &FreeSans9pt7b :
                           widget.font == 2 ? &FreeMono9pt7b : nullptr);
            canvas.setTextSize(widget.textSize);
            int16_t x1, y1;
            uint16_t width, height;
            canvas.getTextBounds(value, 0, 0, &x1, &y1, &width, &height);
            canvas.setCursor(-x1, 9 - y1);
            canvas.print(value);
            display.drawBitmap(cell.x, cell.y, canvas.getBuffer(), cell.width, cell.height,
                               SSD1306_WHITE);
        }
        display.display();
        return;
    }

    uint8_t page = iftttDisplayPage != 0 ? iftttDisplayPage : settings.displayMode;
    if (page == 0) {
        if (millis() - lastDisplayPageMs >=
            static_cast<uint32_t>(settings.displayPageSeconds) * 1000) {
            currentDisplayPage = currentDisplayPage % 3 + 1;
            lastDisplayPageMs = millis();
        }
        page = currentDisplayPage;
    }

    display.clearDisplay();
    uint8_t textSize = settings.displayTextSize;
    if (textSize > 1 && (display.width() < 96 || page == 4)) textSize = 1;
    display.setTextSize(textSize);
    display.setTextWrap(false);
    const int16_t screenWidth = display.width();
    const int16_t screenHeight = display.height();
    const int16_t lineHeight = 8 * textSize;
    size_t charactersPerLine = screenWidth / (6 * textSize);
    if (charactersPerLine == 0) charactersPerLine = 1;
    int16_t cursorY = 0;
    const auto writeText = [&](const String &text) {
        size_t offset = 0;
        while (offset < text.length() && cursorY + lineHeight <= screenHeight) {
            const size_t count = min(charactersPerLine, text.length() - offset);
            display.setCursor(0, cursorY);
            display.print(text.substring(offset, offset + count));
            offset += count;
            cursorY += lineHeight;
        }
    };
    const auto formatValue = [](float value, uint8_t digits) {
        return isfinite(value)
                   ? String(value, static_cast<unsigned int>(digits))
                   : String("--");
    };

    if (page == 1) {
        if (textSize == 2) {
            writeText("A6 STATUS");
            writeText("DI " + String(inputState, HEX));
            writeText("AI1 " + String(analogReadings[0]));
            writeText("RLY " + String(static_cast<uint8_t>(~relayOutputBits), HEX));
        } else {
            writeText("KC868-A6 / STATUS");
            writeText("DI " + String(inputState, HEX));
            writeText("AI1 " + String(analogReadings[0]));
            writeText("RLY " + String(static_cast<uint8_t>(~relayOutputBits), HEX));
        }
    } else if (page == 2) {
        if (textSize == 2) {
            writeText("1 " + formatValue(temperatureValues[0], 0) + "/" +
                      formatValue(sensorHumidity[0], 0) + "%");
            writeText("2 " + formatValue(temperatureValues[1], 0) + "/" +
                      formatValue(sensorHumidity[1], 0) + "%");
            writeText("BME " + formatValue(airTemperature, 0) + "C");
        } else {
            writeText("SENSORS");
            writeText("T1 " + formatValue(temperatureValues[0], 1) + "C");
            writeText("RH1 " + formatValue(sensorHumidity[0], 0) + "%");
            writeText("T2 " + formatValue(temperatureValues[1], 1) + "C");
            writeText("RH2 " + formatValue(sensorHumidity[1], 0) + "%");
            writeText("BME " + formatValue(airTemperature, 1) + "C");
        }
    } else if (page == 3) {
        if (textSize == 2) {
            writeText("AQI " + String(airQualityIndex));
            writeText("VOC " + String(airTvoc));
            writeText("CO2 " + String(airEco2));
            writeText("RH " + formatValue(airHumidity, 0) + "%");
        } else {
            writeText("AIR QUALITY");
            writeText("AQI " + String(airQualityIndex));
            writeText("TVOC " + String(airTvoc));
            writeText("eCO2 " + String(airEco2));
            writeText("RH " + formatValue(airHumidity, 0) + "%");
        }
    } else {
        uint8_t slot = 0;
        const auto addField = [&](uint8_t mask, const String &text) {
            if ((settings.displayFieldMask & mask) == 0 || slot >= 6) return;
            writeText(text);
            ++slot;
        };
        addField(0x01, "T32 " + formatValue(temperatureValues[0], 1) + "C");
        addField(0x02, "T33 " + formatValue(temperatureValues[1], 1) + "C");
        addField(0x04, "RH " + formatValue(airHumidity, 0) + "%");
        addField(0x08, "AQI " + String(airQualityIndex));
        addField(0x10, "Relay " + String(static_cast<uint8_t>(~relayOutputBits), HEX));
        addField(0x20, "DI " + String(inputState, HEX));
        if (slot == 0) writeText("Grid is empty");
    }
    display.display();
}

void reportInputsAndSensors() {
    uint8_t inputState = latestInputState;
    if (!readExpander(kc868a6::i2c::inputExpanderAddress, inputState)) {
        Serial.println("ERROR: input PCF8574 not found at 0x22");
    } else {
        const uint8_t changedInputs = latestInputState ^ inputState;
        for (uint8_t channel = 0; channel < kc868a6::digitalInputCount; ++channel) {
            if ((changedInputs & (1U << channel)) != 0) {
                const bool active = (inputState & (1U << channel)) == 0;
                recordEvent("GPIO: DI" + String(channel + 1) +
                            (active ? " aktywne" : " nieaktywne"));
            }
        }
        latestInputState = inputState;
    }

    Serial.print("Digital inputs (raw, active-low): 0b");
    for (int bit = kc868a6::digitalInputCount - 1; bit >= 0; --bit) {
        Serial.print((inputState >> bit) & 0x01);
    }
    Serial.println();

    Serial.print("Analog raw:");
    reportCoreError(peripheralRegistry.tick(millis()));
    for (uint8_t channel = 0; channel < kc868a6::analogInputCount; ++channel) {
        Serial.printf(" GPIO%u=%u", kc868a6::gpio::analogInputs[channel],
                      analogReadings[channel]);
    }
    Serial.println();

    const DeviceSettings &settings = webUi.settings();
    if (settings.gpio32SensorType == 1 && temperature1Ready) {
        temperature1.requestTemperatures();
        temperatureValues[0] = temperature1.getTempCByIndex(0);
        sensorHumidity[0] = NAN;
    } else if (settings.gpio32SensorType >= 2 && temperature1Ready &&
               millis() - lastDhtReadMs[0] >= 2000) {
        lastDhtReadMs[0] = millis();
        temperatureValues[0] = dht32.readTemperature();
        sensorHumidity[0] = dht32.readHumidity();
    } else if (settings.gpio32SensorType == 0) {
        temperatureValues[0] = NAN;
        sensorHumidity[0] = NAN;
    }
    if (settings.gpio33SensorType == 1 && temperature2Ready) {
        temperature2.requestTemperatures();
        temperatureValues[1] = temperature2.getTempCByIndex(0);
        sensorHumidity[1] = NAN;
    } else if (settings.gpio33SensorType >= 2 && temperature2Ready &&
               millis() - lastDhtReadMs[1] >= 2000) {
        lastDhtReadMs[1] = millis();
        temperatureValues[1] = dht33.readTemperature();
        sensorHumidity[1] = dht33.readHumidity();
    } else if (settings.gpio33SensorType == 0) {
        temperatureValues[1] = NAN;
        sensorHumidity[1] = NAN;
    }
    Serial.printf("GPIO%u T=%.2fC RH=%.1f%% | GPIO%u T=%.2fC RH=%.1f%%\n",
                  kc868a6::gpio::sensor1, temperatureValues[0], sensorHumidity[0],
                  kc868a6::gpio::sensor2, temperatureValues[1], sensorHumidity[1]);

    if (settings.uart1Mode == 1 && settings.gpsEnabled && gps.location.isValid()) {
        gpsLatitude = gps.location.lat();
        gpsLongitude = gps.location.lng();
        gpsSatellites = gps.satellites.value();
        Serial.printf("GPS lat=%.6f lon=%.6f sats=%u\n",
                      gpsLatitude, gpsLongitude, gpsSatellites);
    }
    const bool bmeSample = settings.bme680Enabled && bme680Ready && bme680.performReading();
    if (bmeSample) {
        airTemperature = bme680.temperature;
        airHumidity = bme680.humidity;
        airGasResistance = bme680.gas_resistance;
        Serial.printf("BME680 T=%.2fC RH=%.2f%% P=%.2fhPa gas=%u\n",
                      bme680.temperature, bme680.humidity,
                      bme680.pressure / 100.0F, bme680.gas_resistance);
    } else if (!settings.bme680Enabled) {
        airTemperature = NAN;
        airHumidity = NAN;
        airGasResistance = 0;
        airQualityAvailable = false;
    }
    uint8_t ens160Status = 0;
    uint8_t ens160Data[5] = {};
    const bool ensSample = settings.ens160Enabled && ens160Ready &&
        readI2cRegisters(kc868a6::i2c::ens160Address,
                                        0x20, &ens160Status, 1) &&
        (ens160Status & 0x02) != 0 &&
        readI2cRegisters(kc868a6::i2c::ens160Address, 0x21,
                         ens160Data, sizeof(ens160Data));
    if (ensSample) {
        const uint16_t tvoc = ens160Data[1] | (ens160Data[2] << 8);
        const uint16_t eco2 = ens160Data[3] | (ens160Data[4] << 8);
        airQualityIndex = ens160Data[0];
        airTvoc = tvoc;
        airEco2 = eco2;
        airQualityAvailable = true;
        Serial.printf("ENS160 AQI=%u TVOC=%u ppb eCO2=%u ppm\n",
                      ens160Data[0], tvoc, eco2);
    } else if (!settings.ens160Enabled) {
        airQualityAvailable = false;
    }

    uint8_t compassData[6] = {};
    const bool compassSample = settings.compassEnabled && qmc5883lReady && readI2cRegisters(
                             kc868a6::i2c::qmc5883lAddress, 0x00,
                             compassData, sizeof(compassData));
    if (compassSample) {
        const int16_t x = static_cast<int16_t>(compassData[0] |
                                               (compassData[1] << 8));
        const int16_t y = static_cast<int16_t>(compassData[2] |
                                               (compassData[3] << 8));
        const int16_t z = static_cast<int16_t>(compassData[4] |
                                               (compassData[5] << 8));
        compassX = x;
        compassY = y;
        compassZ = z;
        Serial.printf("QMC5883L x=%d y=%d z=%d\n", x, y, z);
    }

    if (settings.pn532Enabled && pn532Ready) {
        uint8_t uid[7] = {};
        uint8_t uidLength = 0;
        if (pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid,
                                      &uidLength, 50)) {
            Serial.print("NFC UID:");
            for (uint8_t index = 0; index < uidLength; ++index) {
                Serial.printf(" %02X", uid[index]);
            }
            Serial.println();
        }
    }

    if (settings.rtcEnabled && rtcReady) {
        const DateTime now = rtc.now();
        Serial.printf("RTC %04u-%02u-%02u %02u:%02u:%02u\n", now.year(),
                      now.month(), now.day(), now.hour(), now.minute(),
                      now.second());
    }

    const double sensorValues[] = {
        temperatureValues[0], sensorHumidity[0], temperatureValues[1], sensorHumidity[1],
        airTemperature, airHumidity, static_cast<double>(airGasResistance),
        static_cast<double>(airQualityIndex), static_cast<double>(airTvoc),
        static_cast<double>(airEco2), gpsLatitude, gpsLongitude,
        static_cast<double>(gpsSatellites), static_cast<double>(compassX),
        static_cast<double>(compassY), static_cast<double>(compassZ),
    };
    const bool sensorEnabled[] = {
        settings.temperature1Enabled, settings.temperature1Enabled,
        settings.temperature2Enabled, settings.temperature2Enabled,
        settings.bme680Enabled, settings.bme680Enabled, settings.bme680Enabled,
        settings.ens160Enabled, settings.ens160Enabled, settings.ens160Enabled,
        settings.gpsEnabled && settings.uart1Mode == 1,
        settings.gpsEnabled && settings.uart1Mode == 1,
        settings.gpsEnabled && settings.uart1Mode == 1,
        settings.compassEnabled, settings.compassEnabled, settings.compassEnabled,
    };
    const bool available[] = {
        temperature1Ready, settings.gpio32SensorType >= 2 && temperature1Ready,
        temperature2Ready, settings.gpio33SensorType >= 2 && temperature2Ready,
        bmeSample, bmeSample, bmeSample,
        ensSample, ensSample, ensSample,
        gps.location.isValid() && gps.location.age() < 30000,
        gps.location.isValid() && gps.location.age() < 30000,
        gps.location.isValid() && gps.location.age() < 30000,
        compassSample, compassSample, compassSample,
    };
    for (size_t index = 0; index < kc868a6::peripherals::SensorReadings::count; ++index) {
        const auto quality = !sensorEnabled[index] ? kc868a6::core::Quality::Disabled
            : !available[index] ? kc868a6::core::Quality::Unavailable : kc868a6::core::Quality::Valid;
        uint32_t sampledAt = millis();
        if (index < 2 && settings.gpio32SensorType >= 2) sampledAt = lastDhtReadMs[0];
        if (index >= 2 && index < 4 && settings.gpio33SensorType >= 2) sampledAt = lastDhtReadMs[1];
        if (index >= 10 && index < 13 && gps.location.isValid()) sampledAt -= gps.location.age();
        reportCoreError(sensorReadings.publish(index, sensorValues[index], quality, sampledAt));
    }
    renderOled(inputState);
}

}

void setup() {
    webUi.setBluetoothPresence(blePresence);
    webUi.setCurrentInputs(currentInputs);
    webUi.prepare();
    configureBluetooth(webUi.settings().bluetoothMode);
    initializePeripherals();
    Serial.println("KC868-A6 PlatformIO firmware ready");
    webUi.begin(deviceStatusJson, setRelay, applyDeviceSettings, startBeeperTest,
                updateIftttRules, scanBluetoothDevices
#if defined(USE_RADIO_NRF24)
                , sendNrf24Packet, radioMonitorJson
#endif
                );
}

void loop() {
    webUi.loop();
    reportCoreError(blePresence.tick(millis()));
    reportCoreError(transport.tick(millis()));
    if (millis() - lastIftttMs >= 100) {
        lastIftttMs = millis();
        evaluateIftttRules();
    }
#if defined(USE_RADIO_NRF24)
    while (radioReady && nrf24.available()) {
        nrf24.read(&lastRf24Packet, sizeof(lastRf24Packet));
        rf24PacketAvailable = true;
        captureRf24Packet(lastRf24Packet);
    }
#endif
    if (webUi.settings().s2InputEnabled) {
        const bool rawS2Pressed = digitalRead(kc868a6::gpio::s2Button) == LOW;
        if (rawS2Pressed != s2CandidatePressed) {
            s2CandidatePressed = rawS2Pressed;
            s2CandidateSinceMs = millis();
        } else if (rawS2Pressed != s2Pressed &&
                   millis() - s2CandidateSinceMs >= s2DebounceMs) {
            s2Pressed = rawS2Pressed;
            recordEvent(String("GPIO: S2 ") +
                        (s2Pressed ? "wciśnięty" : "zwolniony"));
        }
    }
    if (beeperActive && static_cast<int32_t>(millis() - beeperStopAt) >= 0) {
        ledcWriteTone(configuredBeeperPin, 0);
        ledcWrite(configuredBeeperPin, 0);
        beeperActive = false;
        recordEvent("Buzzer: test zakończony.");
    }
    if (webUi.settings().uart1Mode == 1 && webUi.settings().gpsEnabled) {
        while (Serial1.available()) {
            gps.encode(Serial1.read());
        }
        if (gps.location.isValid()) {
            gpsLatitude = gps.location.lat();
            gpsLongitude = gps.location.lng();
            gpsSatellites = gps.satellites.value();
        }
    } else if (webUi.settings().uart1Mode == 2) {
        while (Serial1.available()) {
            Serial.write(Serial1.read());
        }
    }
    if (millis() - lastReportMs >= webUi.settings().sensorIntervalMs) {
        lastReportMs = millis();
        reportInputsAndSensors();
    }
}