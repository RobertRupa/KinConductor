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

#include "kc868_a6_pins.h"
#include "wifi_web_ui.h"

#if defined(USE_RADIO_NRF24)
#include <RF24.h>
#endif

namespace {

constexpr uint8_t screenWidth = 128;
constexpr uint8_t screenHeight = 64;
constexpr uint32_t reportIntervalMs = 1000;
constexpr uint64_t nrf24PipeAddress = 0xE8E8F0F0E1LL;
constexpr uint8_t rf24HistoryCapacity = 16;
constexpr uint8_t eventLogCapacity = 24;
constexpr uint32_t s2DebounceMs = 120;

struct CapturedRf24Packet {
    uint32_t payload = 0;
    uint32_t receivedAtMs = 0;
    uint32_t sequence = 0;
};

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
WifiWebUi webUi;

#if defined(USE_RADIO_NRF24) && defined(USE_RADIO_LORA)
#error "Select only one radio: nRF24 and SX1278 share the board CS pin"
#endif

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
uint32_t lastRf24Packet = 0;
bool rf24PacketAvailable = false;
CapturedRf24Packet rf24History[rf24HistoryCapacity] = {};
uint8_t rf24HistoryNext = 0;
uint8_t rf24HistoryCount = 0;
uint32_t rf24Sequence = 0;
DeviceLogEvent eventLog[eventLogCapacity] = {};
uint8_t eventLogNext = 0;
uint8_t eventLogCount = 0;
uint8_t detectedI2cAddresses[112] = {};
uint8_t detectedI2cCount = 0;
bool beeperPwmAttached = false;
bool beeperActive = false;
uint16_t configuredBeeperFrequencyHz = 0;
bool s2Pressed = false;
bool s2CandidatePressed = false;
uint32_t s2CandidateSinceMs = 0;
uint32_t beeperStopAt = 0;
uint8_t relayOutputBits = 0xFF;
uint8_t latestInputState = 0xFF;
uint16_t analogReadings[kc868a6::analogInputCount] = {};
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
bool iftttPreviousMatches[4] = {};

void configureBluetooth(uint8_t mode);
String scanBluetoothDevices();
bool sendNrf24Packet(uint32_t payload);

void recordEvent(const String &message) {
    DeviceLogEvent &event = eventLog[eventLogNext];
    event.timestampMs = millis();
    snprintf(event.message, sizeof(event.message), "%s", message.c_str());
    eventLogNext = (eventLogNext + 1) % eventLogCapacity;
    if (eventLogCount < eventLogCapacity) ++eventLogCount;
}

void captureRf24Packet(uint32_t payload) {
    rf24History[rf24HistoryNext] = {payload, millis(), ++rf24Sequence};
    rf24HistoryNext = (rf24HistoryNext + 1) % rf24HistoryCapacity;
    if (rf24HistoryCount < rf24HistoryCapacity) ++rf24HistoryCount;
}

String radioMonitorJson() {
    DynamicJsonDocument document(1536);
    document["available"] =
#if defined(USE_RADIO_NRF24)
        radioReady;
#else
        false;
#endif
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

bool setRelay(uint8_t channel, bool on) {
    if (channel >= kc868a6::relayCount) return false;

    const uint8_t mask = static_cast<uint8_t>(1U << channel);
    uint8_t nextState = relayOutputBits;
    uint8_t hardwareState = 0;
    if (readExpander(kc868a6::i2c::relayExpanderAddress, hardwareState)) {
        relayOutputBits = hardwareState;
        nextState = hardwareState;
    }
    if (kc868a6::relayActiveLow == on) {
        nextState &= static_cast<uint8_t>(~mask);
    } else {
        nextState |= mask;
    }
    relayExpanderReady = writeExpander(kc868a6::i2c::relayExpanderAddress,
                                       nextState);
    if (!relayExpanderReady) {
        return false;
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
            return false;
        }
    }
    recordEvent("GPIO: przekaźnik " + String(channel + 1) +
                (on ? " włączony" : " wyłączony"));
    return true;
}

String deviceStatusJson() {
    DynamicJsonDocument document(6144);
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
    for (uint8_t value : analogReadings) analog.add(value);

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
    addDevice("temperature1", settings.gpio32SensorType == 2 ? "DHT22" : "DS18B20",
              "GPIO32", temperature1Ready && isfinite(temperatureValues[0]));
    addDevice("temperature2", settings.gpio33SensorType == 2 ? "DHT22" : "DS18B20",
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

    JsonObject radio = document["rf24"].to<JsonObject>();
    radio["available"] = radioReady;
    radio["packetAvailable"] = rf24PacketAvailable;
    radio["lastPacket"] = lastRf24Packet;
    radio["receivedCount"] = rf24Sequence;
    radio["pipe"] = "E8E8F0F0E1";

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
    } else if (sensorType == 2) {
        pinMode(pin, INPUT);
        dhtSensor.begin();
        ready = true;
        const uint8_t sensorIndex = pin == kc868a6::gpio::sensor1 ? 0 : 1;
        lastDhtReadMs[sensorIndex] = millis() - 2100;
    }
}

void configureBeeper(const DeviceSettings &settings) {
    if (!settings.beeperEnabled) {
        if (beeperPwmAttached) {
            ledcWriteTone(kc868a6::gpio::dac1, 0);
            ledcWrite(kc868a6::gpio::dac1, 0);
            ledcDetach(kc868a6::gpio::dac1);
            beeperPwmAttached = false;
        }
        configuredBeeperFrequencyHz = 0;
        beeperActive = false;
        dacWrite(kc868a6::gpio::dac1, 0);
        return;
    }

    if (beeperPwmAttached &&
        configuredBeeperFrequencyHz != settings.beeperFrequencyHz) {
        ledcWriteTone(kc868a6::gpio::dac1, 0);
        ledcDetach(kc868a6::gpio::dac1);
        beeperPwmAttached = false;
        beeperActive = false;
    }
    if (!beeperPwmAttached) {
        beeperPwmAttached = ledcAttach(kc868a6::gpio::dac1,
                                       settings.beeperFrequencyHz, 8);
        if (!beeperPwmAttached) {
            Serial.println("ERROR: could not attach LEDC to beeper GPIO26");
            recordEvent("Buzzer: błąd inicjalizacji LEDC GPIO26.");
            return;
        }
        configuredBeeperFrequencyHz = settings.beeperFrequencyHz;
    }
    if (!beeperActive) ledcWrite(kc868a6::gpio::dac1, 0);
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

    analogReadResolution(12);
    for (uint8_t pin : kc868a6::gpio::analogInputs) {
        pinMode(pin, INPUT);
    }

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
    configureBeeper(settings);
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

    ledcWriteTone(kc868a6::gpio::dac1, settings.beeperFrequencyHz);
    beeperActive = true;
    beeperStopAt = millis() + settings.beeperDurationMs;
    recordEvent("Buzzer: test uruchomiony.");
    return true;
}

void configureBluetooth(uint8_t mode) {
    if (NimBLEDevice::isInitialized()) NimBLEDevice::deinit(true);
    bluetoothReady = false;
    if (mode == 0) return;

    bluetoothReady = NimBLEDevice::init("KC868-A6");
    if (!bluetoothReady) return;
    if (mode == 1) {
        NimBLEDevice::getScan()->setActiveScan(true);
    } else if (mode == 2) {
        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        advertising->setName("KC868-A6");
        advertising->start();
    }
}

String scanBluetoothDevices() {
    DynamicJsonDocument document(3072);
    JsonArray devices = document["devices"].to<JsonArray>();
    if (webUi.settings().bluetoothMode != 1 || !bluetoothReady) {
        document["error"] = "ble_scan_disabled";
    } else {
        NimBLEScan *scanner = NimBLEDevice::getScan();
        scanner->setMaxResults(24);
        const NimBLEScanResults results = scanner->getResults(2500, false);
        for (int index = 0; index < results.getCount() && index < 24; ++index) {
            const NimBLEAdvertisedDevice *advertised = results.getDevice(index);
            if (!advertised) continue;
            JsonObject device = devices.createNestedObject();
            const std::string name = advertised->getName();
            const std::string address = advertised->getAddress().toString();
            device["name"] = name.empty() ? "(unnamed)" : name.c_str();
            device["address"] = address.c_str();
            device["rssi"] = advertised->getRSSI();
        }
        scanner->clearResults();
    }
    String response;
    serializeJson(document, response);
    return response;
}

bool sendNrf24Packet(uint32_t payload) {
#if defined(USE_RADIO_NRF24)
    if (!radioReady) return false;
    nrf24.stopListening();
    nrf24.openWritingPipe(nrf24PipeAddress);
    const bool sent = nrf24.write(&payload, sizeof(payload));
    nrf24.startListening();
    return sent;
#else
    (void)payload;
    return false;
#endif
}

double iftttValue(const char *source) {
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
        return rf24PacketAvailable ? static_cast<double>(lastRf24Packet) : NAN;
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
            return analogReadings[channel];
        }
    }
    return NAN;
}

bool iftttConditionMatches(double value, double threshold, const char *comparison) {
    if (!isfinite(value)) return false;
    if (strcmp(comparison, ">") == 0) return value > threshold;
    if (strcmp(comparison, "<") == 0) return value < threshold;
    if (strcmp(comparison, ">=") == 0) return value >= threshold;
    if (strcmp(comparison, "<=") == 0) return value <= threshold;
    if (strcmp(comparison, "==") == 0) return value == threshold;
    if (strcmp(comparison, "!=") == 0) return value != threshold;
    return false;
}

void updateIftttRules(const String &rules) {
    iftttRulesJson = rules;
    memset(iftttPreviousMatches, 0, sizeof(iftttPreviousMatches));
    DynamicJsonDocument document(2048);
    const uint8_t count = deserializeJson(document, rules) || !document.is<JsonArray>()
                              ? 0
                              : document.as<JsonArray>().size();
    recordEvent("IFTTT: aktywne reguły " + String(count) + ".");
}

void evaluateIftttRules() {
    DynamicJsonDocument document(2048);
    if (deserializeJson(document, iftttRulesJson) || !document.is<JsonArray>()) return;
    uint8_t ruleIndex = 0;
    for (JsonObject rule : document.as<JsonArray>()) {
        if (ruleIndex >= 4) break;
        bool matches = false;
        if (rule["enabled"] | false) {
            const float value = iftttValue(rule["source"] | "");
            const double threshold = rule["threshold"] | 0.0;
            matches = iftttConditionMatches(
                value, threshold, rule["comparison"] | "");
        }
        if (matches && !iftttPreviousMatches[ruleIndex]) {
            const String ruleName = "Reguła " + String(ruleIndex + 1);
            recordEvent("IFTTT: " + ruleName + " wyzwolona.");
            const char *action = rule["action"] | "relay";
            bool actionSucceeded = true;
            if (strcmp(action, "rf24Send") == 0 ||
                strcmp(action, "rf24Saved") == 0) {
                actionSucceeded = sendNrf24Packet(rule["payload"] | 0U);
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
        iftttPreviousMatches[ruleIndex] = matches;
        ++ruleIndex;
    }
}

void renderOled(uint8_t inputState) {
    const DeviceSettings &settings = webUi.settings();
    if (!displayReady || !settings.displayEnabled) return;

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
    for (uint8_t channel = 0; channel < kc868a6::analogInputCount; ++channel) {
        analogReadings[channel] = analogRead(kc868a6::gpio::analogInputs[channel]);
        Serial.printf(" GPIO%u=%u", kc868a6::gpio::analogInputs[channel],
                      analogReadings[channel]);
    }
    Serial.println();

    const DeviceSettings &settings = webUi.settings();
    if (settings.gpio32SensorType == 1 && temperature1Ready) {
        temperature1.requestTemperatures();
        temperatureValues[0] = temperature1.getTempCByIndex(0);
        sensorHumidity[0] = NAN;
    } else if (settings.gpio32SensorType == 2 && temperature1Ready &&
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
    } else if (settings.gpio33SensorType == 2 && temperature2Ready &&
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
    if (settings.bme680Enabled && bme680Ready && bme680.performReading()) {
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
    if (settings.ens160Enabled && ens160Ready &&
        readI2cRegisters(kc868a6::i2c::ens160Address,
                                        0x20, &ens160Status, 1) &&
        (ens160Status & 0x02) != 0 &&
        readI2cRegisters(kc868a6::i2c::ens160Address, 0x21,
                         ens160Data, sizeof(ens160Data))) {
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
    if (settings.compassEnabled && qmc5883lReady && readI2cRegisters(
                             kc868a6::i2c::qmc5883lAddress, 0x00,
                             compassData, sizeof(compassData))) {
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

    evaluateIftttRules();
    renderOled(inputState);
}

}

void setup() {
    webUi.prepare();
    configureBluetooth(webUi.settings().bluetoothMode);
    initializePeripherals();
    Serial.println("KC868-A6 PlatformIO firmware ready");
    webUi.begin(deviceStatusJson, setRelay, applyDeviceSettings, startBeeperTest,
                updateIftttRules, scanBluetoothDevices, sendNrf24Packet,
                radioMonitorJson);
}

void loop() {
    webUi.loop();
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
        ledcWriteTone(kc868a6::gpio::dac1, 0);
        ledcWrite(kc868a6::gpio::dac1, 0);
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