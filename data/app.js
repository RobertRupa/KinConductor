const translations = {
  pl: {
    pageScreen: "Ekran", pageSettings: "Ustawienia", capabilityAction: "Akcja interfejsu",
    invalidThreshold: "Wpisz poprawny próg warunku.", invalidRf24Payload: "Payload RF24 musi być liczbą całkowitą 0–4294967295.",
    bleSelectId: "Wybierz ID", bleIdSaved: "Lista BLE zapisana · in_range",
    bleRemoveId: "Usuń ID", bleInvalidId: "Nieprawidłowy MAC BLE",
    bleDuplicateId: "To urządzenie jest już zapisane", bleTargetLimit: "Limit 8 zapisanych urządzeń BLE",
    bleSavedTargets: "Zapisane urządzenia BLE", bleAddId: "Dodaj ID",
    bleTargetsHelp: "Do 8 MAC. Każdy ma osobny in_range w IO/IFTTT/OLED. Puste pole nie usuwa urządzeń; użyj Usuń ID.",
    udpPacketSource: "UDP · pakiet na porcie reguły", udpRulePort: "Lokalny port odbioru",
    udpRemoteIp: "IP nadawcy (puste = dowolny)", udpRemotePort: "Port nadawcy (0 = dowolny)",
    udpTextMatch: "Dopasowanie treści", udpExact: "Równa", udpContains: "Zawiera",
    udpText: "Tekst pakietu (puste = dowolny)",
    udpRuleHelp: "Port przypisany do reguły, bind 0.0.0.0. Ten sam port współdzieli odbiornik. Nie używaj globalnego portu UDP ani 5353 (mDNS). Maks. 256 bajtów UTF-8.",
    udpBindHelp: "Nasłuch: 0.0.0.0 (wszystkie lokalne adresy). Nadawca używa IP płytki i tego portu. Adres/port docelowy ustawiasz w akcji UDP Send w IO/IFTTT.",
    networkIdentity: "Sieć · hostname i mDNS", hostnameHelp: "Bez .local; litery, cyfry i myślnik. Zmiana ponownie łączy Wi-Fi. Domyślnie: kinconductor.local.",
    currentCalibration: "Pomiar prądu · kalibracja wejść A1–A4",
    currentRepair: "Przygotuj nową konfigurację (wyłączone kanały)",
    currentHelp: "Domyślnie wyłączone. Wprowadź dwa zmierzone punkty ADC oraz odpowiadające im wartości sygnału i amperów. To skalowanie liniowe, nie AC RMS z surowego przebiegu.",
    currentWiring: "Potwierdź wersję wejść płytki. Pętla 4–20 mA może wymagać zewnętrznego odbiornika; konfiguracja nie zmienia wejścia napięciowego w prądowe. Nie podłączaj napięcia sieciowego do ADC.",
    interfaceGrid: "Grid interfejsów", addScreenPage: "Dodaj stronę",
    capabilityDescription: "Moduły, odczyty i akcje udostępnione przez interfejsy.",
    screenDescription: "Grid per strona i dane modułów. Do 8 stron i 16 widgetów łącznie.",
    screenModeNote: "Po zapisaniu układu wybierz tryb OLED „Grid interfejsów”.",
    settingsSchemaDescription: "Formularz generowany ze schematu konfiguracji firmware.",
    controller: "sterownik", connecting: "łączenie", overview: "CENTRUM STEROWANIA",
    dashboard: "Panel urządzenia", live: "Dane na żywo", activeRelays: "AKTYWNE PRZEKAŹNIKI",
    temperature: "TEMPERATURA", sensorOne: "czujnik 1", airQuality: "JAKOŚĆ POWIETRZA",
    waiting: "oczekiwanie", wifiSignal: "SYGNAŁ WI-FI", stationMode: "tryb sieciowy",
    outputs: "WYJŚCIA", relays: "Przekaźniki", channels: "KANAŁÓW", relay: "Przekaźnik",
    channelOne: "Kanał 1", channelTwo: "Kanał 2", channelThree: "Kanał 3",
    channelFour: "Kanał 4", channelFive: "Kanał 5", channelSix: "Kanał 6",
    inputs: "WEJŚCIA", digitalInputs: "Wejścia cyfrowe", triggered: "aktywne wejście",
    idle: "spoczynek", measurement: "POMIAR", analogInputs: "Wejścia analogowe",
    rawAdc: "ADC / SUROWE", environment: "ŚRODOWISKO", sensors: "Czujniki",
    autoRefresh: "ODŚWIEŻANIE 1 S", temperatureOne: "Temperatura 1",
    temperatureTwo: "Temperatura 2", humidity: "Wilgotność", gasResistance: "Opór gazu",
    location: "Lokalizacja GPS", compass: "Kompas · XYZ", localOnly: "Panel lokalny · odświeżanie automatyczne",
    networkSetup: "SIEĆ", wifiSetup: "Konfiguracja Wi-Fi", wifiHelp: "Wybierz sieć i zapisz dane połączenia.",
    availableNetworks: "Dostępne sieci", scan: "Skanuj", selectNetwork: "Wybierz sieć",
    ssid: "Nazwa sieci (SSID)", password: "Hasło", saveConnect: "Zapisz i połącz",
    online: "połączono", setupMode: "punkt konfiguracji", offline: "brak połączenia",
    on: "WŁĄCZONY", off: "WYŁĄCZONY", scanWait: "Skanowanie sieci…",
    noNetworks: "Nie znaleziono sieci.", saved: "Dane zapisane. Urządzenie łączy się z siecią…",
    saveError: "Nie udało się zapisać ustawień.", relayError: "Nie udało się zmienić stanu przekaźnika.",
    wifiError: "Nie udało się odczytać danych Wi-Fi.", gpsWaiting: "brak pozycji",
    gpsSatellites: "satelity", secure: "zabezpieczona", aqiGood: "dobra",
    aqiFair: "umiarkowana", aqiModerate: "średnia", aqiPoor: "zła",
    aqiUnhealthy: "bardzo zła", pageOverview: "Przegląd", pageSensors: "Czujniki",
    pageInterfaces: "Interfejsy", pageDiagnostics: "Diagnostyka", sensorSection: "KONFIGURACJA", sensorPageTitle: "Czujniki",
    sensorPageDescription: "Podgląd pomiarów i aktywacja kanałów pomiarowych.",
    sampling: "ODCZYT", sensorSettings: "Ustawienia sensorów", samplingInterval: "Interwał próbkowania",
    bmeDescription: "Temperatura, wilgotność, gaz", ensDescription: "AQI, TVOC, eCO₂",
    gpsDescription: "UART1 · lokalizacja", rtcDescription: "Zegar czasu rzeczywistego",
    nfcDescription: "Czytnik NFC · SPI", saveSettings: "Zapisz ustawienia", liveValues: "TELEMETRIA",
    sensorPreview: "Podgląd sensorów", interfaceSection: "MAGISTRALY I URZĄDZENIA",
    interfacePageTitle: "Interfejsy", interfacePageDescription: "Stan magistral, konfiguracja portów i wykryte peryferia.",
    portConfig: "PORTY", interfaceSettings: "Konfiguracja interfejsów", i2cClock: "Częstotliwość I²C",
    i2cStartupScan: "Adresy I²C wykryte przy starcie",
    diagnosticsSection: "DIAGNOSTYKA", diagnosticsDescription: "Peryferia wykryte podczas startu i bieżący stan interfejsów.",
    boardPinout: "Wyprowadzenia i zasoby", boardRole: "Rola", boardOwner: "Rezerwacja",
    boardLimits: "Ograniczenia", boardInputOnly: "tylko wejście", boardStrapping: "strapping",
    boardPlanNote: "Rezerwacje konfiguracji, nie detekcja sprzętu. P17 nie dodaje czwartego UART. GPIO12: pin strapping.",
    boardValid: "Plan zasobów poprawny", boardError: "Nie udało się odczytać profilu płytki.",
    boardConfigUnavailable: "Konfiguracja niedostępna; pokazano ostatnie ustawienia RAM lub defaults.",
    runtimeStatus: "Stan urządzenia", rf24RadioState: "nRF24", rf24LastPacket: "Ostatni pakiet",
    rf24Monitor: "RADIO · MONITOR", rf24Captured: "Odebrane pakiety RF24",
    rf24MonitorNote: "Monitor odbiera 32-bitowe ramki na kanale 76 i pipe E8E8F0F0E1.",
    rf24History: "Historia odbioru", rf24SavePacket: "Zapisz ramkę",
    rf24SelectCaptured: "Wybierz odebrany pakiet", rf24PacketName: "Nazwa",
    rf24Save: "Zapisz wybrany", rf24SavedPackets: "Zapisane pakiety",
    rf24SelectSaved: "Wybierz zapisany pakiet", rf24CaptureNone: "Brak odebranych pakietów.",
    rf24Replay: "Odtwórz", rf24SavedNone: "Brak zapisanych pakietów.", rf24SaveSuccess: "Pakiet zapisany w NVS.",
    rf24ReplaySuccess: "Wysłano zapisany pakiet.", rf24Error: "Operacja RF24 nie powiodła się.",
    uart1Mode: "UART1 · GPIO16/17", modeRs232: "RS-232", modeCoprocessor: "Koprocesor radiowy",
    uartBaud: "Prędkość UART1", rs485Baud: "Prędkość RS-485",
    uartWarning: "UART1 współdzieli piny z RS-232. Zmiana trybu wymaga odpowiedniego urządzenia i konwertera poziomów.",
    discovery: "DETEKCJA", connectedDevices: "Podłączone urządzenia", connected: "WYKRYTO",
    notDetected: "BRAK", noDeviceList: "Brak danych o peryferiach.", settingsSaved: "Konfiguracja zapisana.",
    peripheralNotDetected: "Nie wykryto przy starcie; sterowanie zablokowane.",
    settingsError: "Nie udało się zapisać konfiguracji.", disabled: "wyłączony",
    nvsUnavailable: "NVS jest niedostępne; reguły nie zostały zmienione.",
    eventStream: "ZDARZENIA", liveLog: "Strumień logów", noEvents: "Oczekiwanie na zdarzenia.",
    devI2c: "Magistrala I²C", devRelayExpander: "Ekspander przekaźników",
    devInputExpander: "Ekspander wejść", devOled: "Wyświetlacz SSD1306",
    devTemperature1: "DS18B20 · kanał 1", devTemperature2: "DS18B20 · kanał 2",
    devRtc: "Zegar DS3231", devBme680: "Czujnik BME680", devEns160: "Czujnik ENS160",
    devCompass: "Kompas QMC5883L", devPn532: "Czytnik PN532", devUart1: "UART1",
    devRs485: "Interfejs RS-485", devSpi: "Magistrala SPI", devRadio: "Moduł radiowy",
    devGps: "Odbiornik GPS", devButtonS2: "Przycisk S2", devTtl: "Port TTL",
    beeperConfig: "BEEPER · DA-1", enableBeeper: "Użyj DA-1 dla beepera",
    beeperNote: "PWM na DAC1 · wyłącza analogowe DA-1", beeperFrequency: "Częstotliwość",
    beeperDuration: "Czas testu", testBeeper: "Test beepera", buttonConfig: "WEJŚCIE PRZYCISKU",
    enableS2: "Użyj S2 jako wejścia", s2PinNote: "GPIO0 · przytrzymanie przy resecie uruchamia tryb flash",
    buttonState: "Stan S2", pressed: "wciśnięty", released: "zwolniony",
    beeperTested: "Wysłano sygnał testowy.", beeperDisabledError: "Najpierw włącz beeper i zapisz ustawienia.",
    beeperTestError: "Nie udało się uruchomić beepera.", pageAutomation: "IFTTT",
    sensorPortOne: "port czujnika 1", sensorPortTwo: "port czujnika 2", sensorOff: "Wyłączony",
    oledConfig: "EKRAN OLED", oledEnabled: "Włącz ekran", oledPageMode: "Strony ekranu",
    oledCycle: "Automatyczny cykl", oledStatus: "Status urządzenia", oledSensors: "Sensory",
    oledAir: "Jakość powietrza", oledGrid: "Siatka danych", oledPageTime: "Czas strony",
    oledBrightness: "Jasność / kontrast", oledRotation: "Orientacja", oledFontSize: "Skala fontu OLED",
    oledFontNormal: "1× · normalny", oledFontLarge: "2× · duży", oledGridFields: "POLA SIATKI OLED",
    oledGridTemp1: "Temperatura GPIO32", oledGridTemp2: "Temperatura GPIO33",
    oledGridHumidity: "Wilgotność", oledGridAqi: "AQI / eCO₂", oledGridRelays: "Przekaźniki",
    oledGridInputs: "Wejścia DI", automationSection: "REGUŁY AUTOMATYZACJI",
    automationTitle: "IFTTT", automationDescription: "Gdy warunek sensora lub wejścia zostanie spełniony, zmień stan przekaźnika.",
    newRule: "NOWA REGUŁA", conditionAction: "Warunek → akcja", ifSource: "Jeśli odczyt",
    sourceTemp1: "Temperatura · GPIO32", sourceTemp2: "Temperatura · GPIO33",
    sourceS2: "Przycisk S2", sourceRf24: "Odebrany pakiet RF24",
    sourceBmeTemp: "Temperatura BME680", sourceHumidity: "Wilgotność BME680",
    sourceAqi: "AQI ENS160", thenRelay: "Wtedy przekaźnik",
    bluetoothConfig: "BLUETOOTH LE", bluetoothMode: "Tryb Bluetooth", bluetoothOff: "Wyłączony",
    bluetoothScan: "Skanowanie BLE", bluetoothBeacon: "Beacon KC868-A6",
    scanBluetooth: "Skanuj pobliskie", bluetoothWarning: "Skan BLE i beacon są trybami rozłącznymi.",
    bluetoothNone: "Nie znaleziono urządzeń BLE.", bluetoothScanDisabled: "Wybierz tryb skanowania BLE i zapisz ustawienia.",
    thenAction: "Wtedy wykonaj", actionRelay: "Steruj przekaźnikiem", actionRf24: "Nadaj payload RF24",
    actionRf24Saved: "Odtwórz zapisany RF24",
    actionBeeper: "Uruchom buzzer", beeperActionHint: "Używa częstotliwości i czasu testu z ustawień DA-1.",
    rf24Payload: "Payload 32-bit (decimal)", rf24PipeNote: "nRF24 · CH76 · pipe E8E8F0F0E1",
    actionOled: "Pokaż stronę OLED", devBluetooth: "Bluetooth LE",
    turnOn: "Włącz", turnOff: "Wyłącz", addRule: "Dodaj regułę", savedRules: "ZAPISANE",
    automationRules: "Reguły IFTTT", saveRules: "Zapisz reguły", noRules: "Brak reguł.",
    ruleLimit: "Limit 4 reguł.", ruleSaved: "Reguły zapisane.", ruleSaveError: "Nie udało się zapisać reguł.",
    bluetoothScanError: "Skan BLE nie powiódł się."
  },
  en: {
    pageScreen: "Screen", pageSettings: "Settings", capabilityAction: "Interface action",
    invalidThreshold: "Enter a valid condition threshold.", invalidRf24Payload: "RF24 payload must be an integer from 0 to 4294967295.",
    bleSelectId: "Select ID", bleIdSaved: "BLE list saved · in_range",
    bleRemoveId: "Remove ID", bleInvalidId: "Invalid BLE MAC",
    bleDuplicateId: "This device is already saved", bleTargetLimit: "Limit of 8 saved BLE devices",
    bleSavedTargets: "Saved BLE devices", bleAddId: "Add ID",
    bleTargetsHelp: "Up to 8 MACs. Each has its own in_range in IO/IFTTT/OLED. An empty field does not remove devices; use Remove ID.",
    udpPacketSource: "UDP · packet on rule port", udpRulePort: "Local receive port",
    udpRemoteIp: "Sender IP (empty = any)", udpRemotePort: "Sender port (0 = any)",
    udpTextMatch: "Payload match", udpExact: "Equals", udpContains: "Contains",
    udpText: "Packet text (empty = any)",
    udpRuleHelp: "Rule-specific port, bind 0.0.0.0. Identical ports share a receiver. Do not use the global UDP port or 5353 (mDNS). Max. 256 UTF-8 bytes.",
    udpBindHelp: "Listen: 0.0.0.0 (all local addresses). Send to the board IP and this port. Destination address/port is configured in the UDP Send action in IO/IFTTT.",
    networkIdentity: "Network · hostname and mDNS", hostnameHelp: "Without .local; letters, digits and hyphens. Changes reconnect Wi-Fi. Default: kinconductor.local.",
    currentCalibration: "Current measurement · A1–A4 calibration",
    currentRepair: "Prepare replacement configuration (disabled channels)",
    currentHelp: "Disabled by default. Enter two measured ADC endpoints with corresponding signal and amperes values. Linear scaling, not AC RMS from a raw waveform.",
    currentWiring: "Check your board input revision. A 4–20 mA loop may require an external receiver; configuration does not turn voltage inputs into current inputs. Never connect mains voltage to ADC.",
    interfaceGrid: "Interface grid", addScreenPage: "Add page",
    capabilityDescription: "Modules, readings and actions provided by interfaces.",
    screenDescription: "Per-page grids and module data. Up to 8 pages and 16 widgets total.",
    screenModeNote: "After saving the layout, select OLED mode “Interface grid”.",
    settingsSchemaDescription: "Form generated from the firmware configuration schema.",
    controller: "controller", connecting: "connecting", overview: "CONTROL CENTER",
    dashboard: "Device dashboard", live: "Live data", activeRelays: "ACTIVE RELAYS",
    temperature: "TEMPERATURE", sensorOne: "sensor 1", airQuality: "AIR QUALITY",
    waiting: "waiting", wifiSignal: "WI-FI SIGNAL", stationMode: "network mode",
    outputs: "OUTPUTS", relays: "Relays", channels: "CHANNELS", relay: "Relay",
    channelOne: "Channel 1", channelTwo: "Channel 2", channelThree: "Channel 3",
    channelFour: "Channel 4", channelFive: "Channel 5", channelSix: "Channel 6",
    inputs: "INPUTS", digitalInputs: "Digital inputs", triggered: "active input",
    idle: "idle", measurement: "MEASUREMENT", analogInputs: "Analog inputs",
    rawAdc: "ADC / RAW", environment: "ENVIRONMENT", sensors: "Sensors",
    autoRefresh: "REFRESH 1 S", temperatureOne: "Temperature 1",
    temperatureTwo: "Temperature 2", humidity: "Humidity", gasResistance: "Gas resistance",
    location: "GPS location", compass: "Compass · XYZ", localOnly: "Local dashboard · automatic refresh",
    networkSetup: "NETWORK", wifiSetup: "Wi-Fi setup", wifiHelp: "Choose a network and save its credentials.",
    availableNetworks: "Available networks", scan: "Scan", selectNetwork: "Select a network",
    ssid: "Network name (SSID)", password: "Password", saveConnect: "Save and connect",
    online: "connected", setupMode: "setup access point", offline: "disconnected",
    on: "ON", off: "OFF", scanWait: "Scanning networks…",
    noNetworks: "No networks found.", saved: "Credentials saved. Connecting to the network…",
    saveError: "Could not save Wi-Fi settings.", relayError: "Could not change relay state.",
    wifiError: "Could not read Wi-Fi status.", gpsWaiting: "no position",
    gpsSatellites: "satellites", secure: "secured", aqiGood: "good",
    aqiFair: "fair", aqiModerate: "moderate", aqiPoor: "poor",
    aqiUnhealthy: "unhealthy", pageOverview: "Overview", pageSensors: "Sensors",
    pageInterfaces: "Interfaces", pageDiagnostics: "Diagnostics", sensorSection: "CONFIGURATION", sensorPageTitle: "Sensors",
    sensorPageDescription: "Live readings and measurement channel controls.",
    sampling: "SAMPLING", sensorSettings: "Sensor settings", samplingInterval: "Sampling interval",
    bmeDescription: "Temperature, humidity, gas", ensDescription: "AQI, TVOC, eCO₂",
    gpsDescription: "UART1 · location", rtcDescription: "Real-time clock",
    nfcDescription: "NFC reader · SPI", saveSettings: "Save settings", liveValues: "TELEMETRY",
    sensorPreview: "Sensor preview", interfaceSection: "BUSES AND DEVICES",
    interfacePageTitle: "Interfaces", interfacePageDescription: "Bus status, port configuration and detected peripherals.",
    portConfig: "PORTS", interfaceSettings: "Interface configuration", i2cClock: "I²C clock",
    i2cStartupScan: "I²C addresses detected at startup",
    diagnosticsSection: "DIAGNOSTICS", diagnosticsDescription: "Peripherals detected at startup and current interface status.",
    boardPinout: "Pinout and resources", boardRole: "Role", boardOwner: "Reservation",
    boardLimits: "Constraints", boardInputOnly: "input only", boardStrapping: "strapping",
    boardPlanNote: "Configuration reservations, not hardware detection. P17 does not add a fourth UART. GPIO12: strapping pin.",
    boardValid: "Resource plan valid", boardError: "Could not read the board profile.",
    boardConfigUnavailable: "Configuration unavailable; showing last RAM settings or defaults.",
    runtimeStatus: "Device status", rf24RadioState: "nRF24", rf24LastPacket: "Last packet",
    rf24Monitor: "RADIO · MONITOR", rf24Captured: "Received RF24 packets",
    rf24MonitorNote: "Monitoring 32-bit frames on channel 76 and pipe E8E8F0F0E1.",
    rf24History: "Receive history", rf24SavePacket: "Save packet",
    rf24SelectCaptured: "Select a received packet", rf24PacketName: "Name",
    rf24Save: "Save selected", rf24SavedPackets: "Saved packets",
    rf24SelectSaved: "Select a saved packet", rf24CaptureNone: "No packets received.",
    rf24Replay: "Replay", rf24SavedNone: "No saved packets.", rf24SaveSuccess: "Packet saved to NVS.",
    rf24ReplaySuccess: "Saved packet transmitted.", rf24Error: "RF24 operation failed.",
    uart1Mode: "UART1 · GPIO16/17", modeRs232: "RS-232", modeCoprocessor: "Radio coprocessor",
    uartBaud: "UART1 baud rate", rs485Baud: "RS-485 baud rate",
    uartWarning: "UART1 shares pins with RS-232. Switching modes requires the correct device and voltage-level interface.",
    discovery: "DISCOVERY", connectedDevices: "Connected devices", connected: "DETECTED",
    notDetected: "NOT FOUND", noDeviceList: "No peripheral data available.", settingsSaved: "Configuration saved.",
    peripheralNotDetected: "Not detected at startup; control locked.",
    settingsError: "Could not save configuration.", disabled: "disabled",
    nvsUnavailable: "NVS is unavailable; rules were not changed.",
    eventStream: "EVENTS", liveLog: "Live event stream", noEvents: "Waiting for events.",
    devI2c: "I²C bus", devRelayExpander: "Relay expander", devInputExpander: "Input expander",
    devOled: "SSD1306 display", devTemperature1: "DS18B20 · channel 1",
    devTemperature2: "DS18B20 · channel 2", devRtc: "DS3231 clock", devBme680: "BME680 sensor",
    devEns160: "ENS160 sensor", devCompass: "QMC5883L compass", devPn532: "PN532 reader",
    devUart1: "UART1", devRs485: "RS-485 interface", devSpi: "SPI bus",
    devRadio: "Radio module", devGps: "GPS receiver", devButtonS2: "S2 button", devTtl: "TTL serial port",
    beeperConfig: "BEEPER · DA-1", enableBeeper: "Use DA-1 for beeper",
    beeperNote: "PWM on DAC1 · disables analog DA-1", beeperFrequency: "Frequency",
    beeperDuration: "Test duration", testBeeper: "Test beeper", buttonConfig: "BUTTON INPUT",
    enableS2: "Use S2 as an input", s2PinNote: "GPIO0 · holding at reset enters flash mode",
    buttonState: "S2 state", pressed: "pressed", released: "released",
    beeperTested: "Test signal sent.", beeperDisabledError: "Enable the beeper and save settings first.",
    beeperTestError: "Could not start the beeper.", pageAutomation: "IFTTT",
    sensorPortOne: "sensor port 1", sensorPortTwo: "sensor port 2", sensorOff: "Disabled",
    oledConfig: "OLED SCREEN", oledEnabled: "Enable display", oledPageMode: "Screen pages",
    oledCycle: "Automatic cycle", oledStatus: "Device status", oledSensors: "Sensors",
    oledAir: "Air quality", oledGrid: "Data grid", oledPageTime: "Page duration",
    oledBrightness: "Brightness / contrast", oledRotation: "Orientation", oledFontSize: "OLED font size",
    oledFontNormal: "1× · normal", oledFontLarge: "2× · large", oledGridFields: "OLED GRID FIELDS",
    oledGridTemp1: "Temperature GPIO32", oledGridTemp2: "Temperature GPIO33",
    oledGridHumidity: "Humidity", oledGridAqi: "AQI / eCO₂", oledGridRelays: "Relays",
    oledGridInputs: "Digital inputs", automationSection: "AUTOMATION RULES",
    automationTitle: "IFTTT", automationDescription: "When a sensor or input condition is met, change a relay state.",
    newRule: "NEW RULE", conditionAction: "Condition → action", ifSource: "If reading",
    sourceTemp1: "Temperature · GPIO32", sourceTemp2: "Temperature · GPIO33",
    sourceS2: "S2 button", sourceRf24: "Received RF24 packet",
    sourceBmeTemp: "BME680 temperature", sourceHumidity: "BME680 humidity",
    sourceAqi: "ENS160 AQI", thenRelay: "Then relay",
    bluetoothConfig: "BLUETOOTH LE", bluetoothMode: "Bluetooth mode", bluetoothOff: "Disabled",
    bluetoothScan: "BLE scanner", bluetoothBeacon: "KC868-A6 beacon",
    scanBluetooth: "Scan nearby", bluetoothWarning: "BLE scan and beacon are exclusive modes.",
    bluetoothNone: "No BLE devices found.", bluetoothScanDisabled: "Select BLE scanner mode and save first.",
    thenAction: "Then do", actionRelay: "Control relay", actionRf24: "Transmit RF24 payload",
    actionRf24Saved: "Replay saved RF24 packet",
    actionBeeper: "Trigger beeper", beeperActionHint: "Uses the frequency and test duration configured for DA-1.",
    rf24Payload: "32-bit payload (decimal)", rf24PipeNote: "nRF24 · CH76 · pipe E8E8F0F0E1",
    actionOled: "Show OLED page", devBluetooth: "Bluetooth LE",
    turnOn: "Turn on", turnOff: "Turn off", addRule: "Add rule", savedRules: "SAVED",
    automationRules: "IFTTT rules", saveRules: "Save rules", noRules: "No rules yet.",
    ruleLimit: "Up to 4 rules.", ruleSaved: "Rules saved.", ruleSaveError: "Could not save rules.",
    bluetoothScanError: "BLE scan failed."
  }
};

let language = localStorage.getItem("kc868-lang") || "pl";
let statusTimer;
let toastTimer;
let deviceSettings = {};
let lastDeviceList = [];
let lastI2cDetected = [];
let iftttRules = [];
let rf24History = [];
let savedRf24Packets = [];
let nearbyBluetoothDevices = [];
let rf24Available = false;
let rf24Compiled = false;
let lastRf24RefreshAt = 0;
let lastBoardStatus = null;
let lastBoardRefreshAt = 0;
let boardLoading = false;
const $ = (selector) => document.querySelector(selector);
const t = (key) => translations[language][key] || key;
const deviceNameKeys = {
  i2c: "devI2c", relayExpander: "devRelayExpander", inputExpander: "devInputExpander",
  oled: "devOled", temperature1: "devTemperature1", temperature2: "devTemperature2",
  rtc: "devRtc", bme680: "devBme680", ens160: "devEns160", compass: "devCompass",
  pn532: "devPn532", uart1: "devUart1", rs485: "devRs485", spi: "devSpi",
  radio: "devRadio", gps: "devGps", buttonS2: "devButtonS2", ttl: "devTtl", bluetooth: "devBluetooth"
};
const iftttSourceNames = {
  temperature1: "sourceTemp1", temperature2: "sourceTemp2",
  buttonS2: "sourceS2", rf24Packet: "sourceRf24",
  bmeTemperature: "sourceBmeTemp", humidity: "sourceHumidity", airQuality: "sourceAqi"
};

function setLanguage(nextLanguage) {
  language = translations[nextLanguage] ? nextLanguage : "pl";
  localStorage.setItem("kc868-lang", language);
  document.documentElement.lang = language;
  $("#language").value = language;
  document.querySelectorAll("[data-i18n]").forEach((element) => {
    element.textContent = t(element.dataset.i18n);
  });
  document.querySelectorAll("[data-relay]").forEach((row) => {
    const button = row.querySelector(".toggle");
    button.setAttribute("aria-label", `${t("relay")} ${row.dataset.relay}`);
  });
  renderDevices(lastDeviceList, lastI2cDetected);
  renderRf24Monitor();
  renderBoardStatus();
  if (statusTimer) refreshStatus();
}

function showToast(message) {
  const toast = $("#toast");
  toast.textContent = message;
  toast.classList.add("visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove("visible"), 3000);
}

function valueOrDash(value, digits = 1) {
  return value !== null && value !== undefined && Number.isFinite(Number(value))
    ? Number(value).toFixed(digits)
    : "--";
}

async function refreshStatus() {
  try {
    const response = await fetch("/api/status", { cache: "no-store" });
    if (!response.ok) throw new Error("status");
    const data = await response.json();
    renderStatus(data);
    const activePage = document.querySelector(".page-tab.is-active")?.dataset.page;
    if (activePage === "diagnostics" && Date.now() - lastBoardRefreshAt >= 5000) {
      refreshBoardStatus();
    }
    if ((activePage === "interfaces" || activePage === "automation") &&
        Date.now() - lastRf24RefreshAt >= 1400) {
      lastRf24RefreshAt = Date.now();
      loadRf24Packets();
    }
  } catch {
    $("#connectionText").textContent = t("offline");
    $(".connection").className = "connection offline";
  }
}

async function refreshBoardStatus() {
  if (boardLoading) return;
  boardLoading = true;
  lastBoardRefreshAt = Date.now();
  try {
    const response = await fetch("/api/board", { cache: "no-store" });
    if (!response.ok) throw new Error("board_status");
    const data = await response.json();
    if (!Array.isArray(data.pins) || !Array.isArray(data.claims) ||
        typeof data.configurationReady !== "boolean" || typeof data.valid !== "boolean" ||
        typeof data.configurationError !== "string" || typeof data.error !== "string" ||
        typeof data.subject !== "string" || !Number.isInteger(data.inputAddress) ||
        !data.relays || !["address", "portMask", "reservedMask"].every(
          (key) => Number.isInteger(data.relays[key])) ||
        !data.pins.every((pin) => pin && Number.isInteger(pin.gpio) &&
          typeof pin.role === "string" && ["inputOnly", "strapping", "adc", "dac"].every(
            (key) => typeof pin[key] === "boolean")) ||
        !data.claims.every((claim) => claim && typeof claim.kind === "string" &&
          Number.isInteger(claim.number) && typeof claim.owner === "string")) {
      throw new Error("board_shape");
    }
    lastBoardStatus = data;
    renderBoardStatus();
  } catch {
    lastBoardStatus = null;
    $("#boardPins").replaceChildren();
    $("#boardExpanders").textContent = "--";
    $("#boardMessage").textContent = t("boardError");
  } finally {
    boardLoading = false;
  }
}

function renderBoardStatus() {
  if (!lastBoardStatus) return;
  const data = lastBoardStatus;
  $("#boardMessage").textContent = !data.configurationReady
    ? `${t("boardConfigUnavailable")} (${data.configurationError})`
    : data.valid ? t("boardValid") : `${data.error}: ${data.subject}`;
  const hex = (value) => `0x${Number(value).toString(16).padStart(2, "0")}`;
  $("#boardExpanders").textContent =
    `PCF8574 DI: ${hex(data.inputAddress)} P0-P7 | PCF8574 RLY: ${hex(data.relays.address)} P0-P5 (${hex(data.relays.portMask)}), P6-P7 ${hex(data.relays.reservedMask)}`;
  const rows = data.pins.map((pin) => {
    const owner = data.claims.find((claim) => claim.kind === "gpio" && claim.number === pin.gpio)?.owner;
    const flags = [
      pin.inputOnly ? t("boardInputOnly") : "",
      pin.strapping ? t("boardStrapping") : "",
      pin.adc ? "ADC1" : "", pin.dac ? "DAC" : ""
    ].filter(Boolean).join(", ");
    const row = document.createElement("tr");
    for (const value of [pin.gpio, pin.role, owner || "--", flags || "--"]) {
      const cell = document.createElement("td");
      cell.textContent = value;
      row.append(cell);
    }
    return row;
  });
  $("#boardPins").replaceChildren(...rows);
}

function renderHardware(hardware) {
  const ids = ["#headerCpu", "#headerRam", "#headerPsram", "#headerFlash"];
  if (!hardware || !hardware.cpu || !hardware.ram || !hardware.psram || !hardware.flash ||
      typeof hardware.cpu.model !== "string" ||
      ![hardware.cpu.cores, hardware.cpu.frequencyMHz, hardware.ram.heapBytes,
        hardware.ram.freeBytes, hardware.psram.totalBytes, hardware.psram.freeBytes,
        hardware.flash.totalBytes, hardware.flash.firmwareBytes].every(
        (value) => Number.isFinite(value) && value >= 0)) {
    ids.forEach((id) => { $(id).textContent = "--"; });
    return;
  }
  const kib = (value) => `${Math.round(value / 1024)} KiB`;
  $("#headerCpu").textContent = `${hardware.cpu.model} · ${hardware.cpu.cores}× ${hardware.cpu.frequencyMHz} MHz`;
  $("#headerRam").textContent = `${kib(hardware.ram.freeBytes)} / ${kib(hardware.ram.heapBytes)} heap`;
  $("#headerPsram").textContent = hardware.psram.totalBytes ? `${kib(hardware.psram.freeBytes)} / ${kib(hardware.psram.totalBytes)}` : "0";
  $("#headerFlash").textContent = `${(hardware.flash.totalBytes / 1048576).toFixed(0)} MiB · FW ${kib(hardware.flash.firmwareBytes)}`;
}

function renderStatus(data) {
  renderHardware(data.hardware);
  const network = data.network || {};
  const connected = Boolean(network.connected);
  $(".connection").className = `connection ${connected ? "online" : "offline"}`;
  $("#connectionText").textContent = connected ? t("online") : network.setup ? t("setupMode") : t("offline");
  $("#networkName").textContent = network.ssid || "--";
  $("#networkIp").textContent = network.ip || "--";
  $("#wifiRssi").textContent = connected ? network.rssi : "--";
  $("#wifiMode").textContent = network.setup ? t("setupMode") : t("stationMode");
  $("#udpListenerStatus").textContent = network.udpListenPort ?
    `${network.udpListenAddress}:${network.udpListenPort} · ${network.ip}:${network.udpListenPort}` :
    "UDP RX: off (port 0)";
  $("#lastUpdate").textContent = new Date().toLocaleTimeString(language === "pl" ? "pl-PL" : "en-GB");

  const relays = Array.isArray(data.relays) ? data.relays : [];
  let activeRelays = 0;
  document.querySelectorAll("[data-relay]").forEach((row, index) => {
    const on = Boolean(relays[index]);
    if (on) activeRelays += 1;
    row.classList.toggle("is-on", on);
    row.querySelector(".toggle").setAttribute("aria-checked", String(on));
    row.querySelector("[data-state]").textContent = on ? t("on") : t("off");
  });
  $("#relayCount").textContent = activeRelays;
  $("#relayTrack").style.width = `${(activeRelays / 6) * 100}%`;

  const inputs = Array.isArray(data.inputs) ? data.inputs : [];
  document.querySelectorAll("[data-input]").forEach((cell, index) => {
    const active = Boolean(inputs[index]);
    cell.classList.toggle("is-active", active);
    cell.querySelector("strong").textContent = active ? t("on") : t("off");
  });

  const analog = Array.isArray(data.analog) ? data.analog : [];
  document.querySelectorAll("[data-analog]").forEach((row, index) => {
    const reading = Number(analog[index] || 0);
    row.querySelector("strong").textContent = reading;
    row.querySelector(".meter i").style.width = `${Math.max(0, Math.min(100, reading / 4095 * 100))}%`;
  });

  const temperatures = Array.isArray(data.temperatures) ? data.temperatures : [];
  const sensorHumidity = Array.isArray(data.sensorHumidity) ? data.sensorHumidity : [];
  $("#mainTemperature").textContent = valueOrDash(temperatures[0]);
  $("#temperatureOne").innerHTML = `${valueOrDash(temperatures[0])}<small>°C</small>`;
  $("#temperatureTwo").innerHTML = `${valueOrDash(temperatures[1])}<small>°C</small>`;

  const air = data.air || {};
  $("#aqiValue").textContent = air.qualityAvailable ? air.aqi : "--";
  const aqiLabels = ["waiting", "aqiGood", "aqiFair", "aqiModerate", "aqiPoor", "aqiUnhealthy"];
  $("#aqiLabel").textContent = air.qualityAvailable
    ? t(aqiLabels[Number(air.aqi)] || "waiting")
    : t("waiting");
  $("#tvocValue").textContent = air.qualityAvailable ? air.tvoc : "--";
  $("#eco2Value").textContent = air.qualityAvailable ? air.eco2 : "--";
  $("#humidityValue").innerHTML = `${valueOrDash(air.humidity)}<small>%</small>`;
  $("#gasValue").innerHTML = `${air.gas ? Math.round(air.gas) : "--"}<small>Ω</small>`;

  const gps = data.gps || {};
  $("#locationValue").textContent = gps.valid
    ? `${Number(gps.latitude).toFixed(5)}, ${Number(gps.longitude).toFixed(5)}`
    : t("gpsWaiting");
  $("#satelliteValue").textContent = gps.valid ? `${gps.satellites} ${t("gpsSatellites")}` : "";
  const compass = Array.isArray(data.compass) ? data.compass : [];
  $("#compassValue").textContent = compass.length === 3 ? compass.join(" / ") : "--";
  const buttonS2 = data.buttonS2 || {};
  const buttonStateText = buttonS2.enabled
    ? buttonS2.pressed ? t("pressed") : t("released")
    : t("disabled");
  $("#s2DashboardState").textContent = buttonStateText;
  $("#s2ButtonState").textContent = buttonStateText;

  const sensorConfig = data.sensorConfig || {};
  const sensorTypes = [sensorConfig.gpio32SensorType, sensorConfig.gpio33SensorType];
  const sensorTypeLabels = ["", "DS18B20", "DHT22", "DHT11"];
  $("#sensorTemp1Name").textContent = sensorTypes[0] === 0 ? "GPIO32 · OFF" : `${sensorTypeLabels[sensorTypes[0]]} · GPIO32`;
  $("#sensorTemp2Name").textContent = sensorTypes[1] === 0 ? "GPIO33 · OFF" : `${sensorTypeLabels[sensorTypes[1]]} · GPIO33`;
  $("#sensorTemp1").textContent = sensorConfig.temperature1Enabled === false
    ? t("disabled") : `${valueOrDash(temperatures[0])} °C${Number.isFinite(sensorHumidity[0]) ? ` · ${valueOrDash(sensorHumidity[0])}%` : ""}`;
  $("#sensorTemp2").textContent = sensorConfig.temperature2Enabled === false
    ? t("disabled") : `${valueOrDash(temperatures[1])} °C${Number.isFinite(sensorHumidity[1]) ? ` · ${valueOrDash(sensorHumidity[1])}%` : ""}`;
  $("#sensorBmeTemp").textContent = sensorConfig.bme680Enabled === false
    ? t("disabled") : `${valueOrDash(air.temperature)} °C`;
  $("#sensorBmeHumidity").textContent = `${t("humidity")}: ${valueOrDash(air.humidity)} %`;
  $("#sensorAqi").textContent = sensorConfig.ens160Enabled === false || !air.qualityAvailable
    ? "--" : `${air.aqi} · ${t(aqiLabels[Number(air.aqi)] || "waiting")}`;
  $("#sensorVoc").textContent = sensorConfig.ens160Enabled === false || !air.qualityAvailable
    ? "--" : `TVOC ${air.tvoc ?? "--"} ppb · eCO₂ ${air.eco2 ?? "--"} ppm`;
  $("#sensorGps").textContent = sensorConfig.gpsEnabled === false || !gps.valid
    ? t("gpsWaiting") : `${Number(gps.latitude).toFixed(5)}, ${Number(gps.longitude).toFixed(5)}`;
  $("#sensorGpsSatellites").textContent = gps.valid ? `${gps.satellites} ${t("gpsSatellites")}` : "";
  $("#sensorCompass").textContent = sensorConfig.compassEnabled === false
    ? t("disabled") : compass.length === 3 ? compass.join(" / ") : "--";
  const rf24 = data.rf24 || {};
  setRf24Support(data.features?.rf24 === true);
  const beeper = data.beeper || {};
  $("#diagnosticRf24State").textContent = rf24.available ? "OK · CH76" : t("notDetected");
  $("#diagnosticRf24Packet").textContent = rf24.packetAvailable
    ? `${rf24.lastPacket} · 0x${Number(rf24.lastPacket).toString(16).toUpperCase()}` : "--";
  $("#diagnosticTemperature1").textContent = `${valueOrDash(temperatures[0])} °C`;
  $("#diagnosticTemperature2").textContent = `${valueOrDash(temperatures[1])} °C`;
  $("#diagnosticS2State").textContent = buttonStateText;
  $("#diagnosticBeeperState").textContent = !beeper.enabled
    ? t("disabled") : beeper.ready ? `OK · ${beeper.frequencyHz} Hz` : t("notDetected");
  renderDevices(data.devices, data.i2cDetected);
  renderEventLog(data.events);
}

function renderEventLog(events) {
  const list = $("#eventLogList");
  if (!list) return;
  const items = Array.isArray(events) ? events : [];
  $("#eventLogCount").textContent = String(items.length);
  list.replaceChildren();
  if (!items.length) {
    const empty = document.createElement("p");
    empty.className = "empty-state";
    empty.textContent = t("noEvents");
    list.append(empty);
    return;
  }
  for (const event of items) {
    const row = document.createElement("article");
    row.className = "event-log-entry";
    const time = document.createElement("time");
    time.dateTime = new Date(Date.now() - Number(event.ageMs || 0)).toISOString();
    time.textContent = new Date(Date.now() - Number(event.ageMs || 0))
      .toLocaleTimeString(language === "pl" ? "pl-PL" : "en-GB");
    const message = document.createElement("p");
    message.textContent = event.message || "";
    row.append(time, message);
    list.append(row);
  }
}

function renderDevices(devices, i2cDetected) {
  lastDeviceList = Array.isArray(devices) ? devices : [];
  lastI2cDetected = Array.isArray(i2cDetected) ? i2cDetected : [];
  const devicesById = new Map(lastDeviceList.map((device) => [device.id, device]));
  document.querySelectorAll("[data-peripheral-enable]").forEach((input) => {
    const peripheralId = input.dataset.peripheralEnable;
    const device = devicesById.get(peripheralId);
    const detected = Boolean(device?.detected ?? device?.connected);
    input.disabled = !detected;
    input.closest(".setting-row")?.classList.toggle("is-unavailable", !detected);
    const note = document.querySelector(`[data-peripheral-note="${peripheralId}"]`);
    if (note) {
      note.hidden = detected;
      note.textContent = t("peripheralNotDetected");
    }
  });
  const addresses = lastI2cDetected.length
    ? lastI2cDetected.map((address) => `0x${Number(address).toString(16).padStart(2, "0")}`).join(" · ")
    : "--";
  for (const selector of ["#i2cDetectedAddresses", "#diagnosticI2cAddresses"]) {
    const addressList = $(selector);
    if (addressList) addressList.textContent = addresses;
  }
  const connectedCount = lastDeviceList.filter((device) => device.connected).length;
  for (const [listSelector, countSelector] of [
    ["#deviceList", "#deviceCount"], ["#diagnosticDeviceList", "#diagnosticDeviceCount"]
  ]) {
    const list = $(listSelector);
    if (!list) continue;
    list.replaceChildren();
    const count = $(countSelector);
    if (count) count.textContent = `${connectedCount} / ${lastDeviceList.length}`;
    if (!lastDeviceList.length) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = t("noDeviceList");
      list.append(empty);
      continue;
    }
    for (const device of lastDeviceList) {
      const card = document.createElement("article");
      card.className = `device-card${device.connected ? " is-connected" : ""}`;
      const dot = document.createElement("i");
      dot.className = "device-dot";
      const content = document.createElement("div");
      const name = document.createElement("strong");
      name.textContent = device.id === "temperature1" || device.id === "temperature2"
        ? device.name : t(deviceNameKeys[device.id] || device.name || "");
      const interfaceName = document.createElement("small");
      interfaceName.textContent = device.interface || "";
      content.append(name, interfaceName);
      const state = document.createElement("span");
      state.className = "device-state";
      state.textContent = device.connected ? t("connected") : t("notDetected");
      card.append(dot, content, state);
      list.append(card);
    }
  }
}

function populateSettingsForms(settings) {
  const sensorForm = $("#sensorSettingsForm");
  const interfaceForm = $("#interfaceSettingsForm");
  if (!sensorForm || !interfaceForm) return;
  sensorForm.elements.sensorIntervalMs.value = settings.sensorIntervalMs;
  sensorForm.elements.gpio32SensorType.value = settings.gpio32SensorType;
  sensorForm.elements.gpio33SensorType.value = settings.gpio33SensorType;
  interfaceForm.elements.i2cClockHz.value = settings.i2cClockHz;
  interfaceForm.elements.bluetoothMode.value = settings.bluetoothMode;
  interfaceForm.elements.uart1Mode.value = settings.uart1Mode;
  interfaceForm.elements.uart1Baud.value = settings.uart1Baud;
  interfaceForm.elements.rs485Baud.value = settings.rs485Baud;
  interfaceForm.elements.beeperFrequencyHz.value = settings.beeperFrequencyHz;
  interfaceForm.elements.beeperDurationMs.value = settings.beeperDurationMs;
  interfaceForm.elements.beeperEnabled.checked = Boolean(settings.beeperEnabled);
  for (const name of ["beeperPin", "fanPin", "fanFrequencyHz", "udpListenPort", "blePresenceTimeoutSeconds"]) {
    interfaceForm.elements[name].value = settings[name];
  }
  interfaceForm.elements.fanEnabled.checked = Boolean(settings.fanEnabled);
  interfaceForm.elements.s2InputEnabled.checked = Boolean(settings.s2InputEnabled);
  interfaceForm.elements.displayEnabled.checked = Boolean(settings.displayEnabled);
  interfaceForm.elements.displayMode.value = settings.displayMode;
  interfaceForm.elements.displayPageSeconds.value = settings.displayPageSeconds;
  interfaceForm.elements.displayBrightness.value = settings.displayBrightness;
  interfaceForm.elements.displayRotation.value = settings.displayRotation;
  interfaceForm.elements.displayTextSize.value = settings.displayTextSize;
  const oledFieldNames = ["oledFieldTemp1", "oledFieldTemp2", "oledFieldHumidity",
    "oledFieldAir", "oledFieldRelays", "oledFieldInputs"];
  oledFieldNames.forEach((name, index) => {
    interfaceForm.elements[name].checked = (settings.displayFieldMask & (1 << index)) !== 0;
  });
  for (const name of ["bme680Enabled",
    "ens160Enabled", "compassEnabled", "gpsEnabled", "rtcEnabled", "pn532Enabled"]) {
    sensorForm.elements[name].checked = Boolean(settings[name]);
  }
  updateIftttActionFields();
}

function setRf24Support(compiled) {
  rf24Compiled = compiled === true;
  document.querySelectorAll("[data-rf24-only]").forEach(element => {
    element.hidden = !rf24Compiled;
  });
  const source = $("#iftttSource");
  const existing = source.querySelector("option[value='rf24Packet']");
  if (rf24Compiled && !existing) {
    const option = document.createElement("option");
    option.value = "rf24Packet";
    option.textContent = t("sourceRf24");
    source.append(option);
  } else if (!rf24Compiled && existing) {
    existing.remove();
    if (!source.value) source.value = "buttonS2";
    updateIftttSourceFields();
  }
  for (const option of document.querySelectorAll("#iftttAction option")) {
    if (option.value === "rf24Send" || option.value === "rf24Saved") {
      option.hidden = !rf24Compiled;
      if (!rf24Compiled) option.disabled = true;
    }
  }
  if (!rf24Compiled) {
    rf24Available = false;
    rf24History = [];
    savedRf24Packets = [];
    updateIftttActionFields();
  }
}

function renderRf24Monitor() {
  const state = $("#rf24MonitorState");
  if (state) state.textContent = rf24Available ? "READY · CH76" : t("notDetected");

  const historyList = $("#rf24HistoryList");
  if (historyList) {
    historyList.replaceChildren();
    if (!rf24History.length) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = t("rf24CaptureNone");
      historyList.append(empty);
    }
    for (const packet of rf24History) {
      const row = document.createElement("article");
      row.className = "rf24-packet-row";
      const details = document.createElement("div");
      const payload = document.createElement("strong");
      payload.textContent = String(packet.payload >>> 0);
      const metadata = document.createElement("small");
      metadata.textContent = `#${packet.sequence} · 0x${(packet.payload >>> 0).toString(16).toUpperCase()} · ${Math.floor(packet.ageMs / 1000)} s`;
      details.append(payload, metadata);
      row.append(details);
      historyList.append(row);
    }
  }

  const captureSelect = $("#rf24CaptureSelect");
  if (captureSelect) {
    const previous = captureSelect.value;
    captureSelect.replaceChildren();
    for (const packet of rf24History) {
      const option = document.createElement("option");
      option.value = String(packet.sequence);
      option.textContent = `#${packet.sequence} · ${packet.payload >>> 0} (0x${(packet.payload >>> 0).toString(16).toUpperCase()})`;
      captureSelect.append(option);
    }
    if (rf24History.some((packet) => String(packet.sequence) === previous)) {
      captureSelect.value = previous;
    }
    $("#rf24SaveButton").disabled = !rf24Available || !rf24History.length || savedRf24Packets.length >= 12;
  }

  const savedList = $("#rf24SavedList");
  if (savedList) {
    savedList.replaceChildren();
    if (!savedRf24Packets.length) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = t("rf24SavedNone");
      savedList.append(empty);
    }
    for (const packet of savedRf24Packets) {
      const row = document.createElement("article");
      row.className = "rf24-packet-row";
      const details = document.createElement("div");
      const name = document.createElement("strong");
      name.textContent = packet.name;
      const metadata = document.createElement("small");
      metadata.textContent = `${packet.payload >>> 0} · 0x${(packet.payload >>> 0).toString(16).toUpperCase()}`;
      details.append(name, metadata);
      const replay = document.createElement("button");
      replay.type = "button";
      replay.className = "secondary-button";
      replay.textContent = t("rf24Replay");
      replay.addEventListener("click", () => replaySavedRf24Packet(packet.id));
      row.append(details, replay);
      savedList.append(row);
    }
  }

  const savedSelect = $("#iftttRf24SavedPacket");
  if (savedSelect) {
    const previous = savedSelect.value;
    savedSelect.replaceChildren();
    const placeholder = document.createElement("option");
    placeholder.value = "";
    placeholder.textContent = t("rf24SelectSaved");
    savedSelect.append(placeholder);
    for (const packet of savedRf24Packets) {
      const option = document.createElement("option");
      option.value = String(packet.id);
      option.textContent = packet.name;
      savedSelect.append(option);
    }
    if (savedRf24Packets.some((packet) => String(packet.id) === previous)) {
      savedSelect.value = previous;
    }
  }
  if ($("#iftttAction")) updateIftttActionFields();
}

async function loadRf24Packets() {
  try {
    const response = await fetch("/api/rf24/packets", { cache: "no-store" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const result = await response.json();
    if (typeof result.compiled !== "boolean" || typeof result.available !== "boolean" ||
        !Array.isArray(result.history) || !Array.isArray(result.saved)) throw new Error("Invalid RF24 response");
    setRf24Support(result.compiled);
    rf24Available = rf24Compiled && result.available;
    rf24History = rf24Compiled ? result.history : [];
    savedRf24Packets = rf24Compiled ? result.saved : [];
    renderRf24Monitor();
  } catch (error) {
    setRf24Support(false);
    renderRf24Monitor();
    $("#rf24Message").textContent = `${t("rf24Error")} ${error.message}`;
    $("#iftttMessage").textContent = `${t("rf24Error")} ${error.message}`;
  }
}

async function replaySavedRf24Packet(packetId) {
  const message = $("#rf24Message");
  try {
    const response = await fetch("/api/rf24/replay", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: packetId })
    });
    if (!response.ok) throw new Error("replay");
    message.textContent = t("rf24ReplaySuccess");
  } catch {
    message.textContent = t("rf24Error");
  }
}

async function loadDeviceSettings() {
  try {
    const response = await fetch("/api/settings", { cache: "no-store" });
    if (!response.ok) throw new Error("settings");
    deviceSettings = await response.json();
    populateSettingsForms(deviceSettings);
  } catch {
    showToast(t("settingsError"));
  }
}

async function saveDeviceSettings(form, messageElement, sensorSettings) {
  const payload = { ...deviceSettings };
  if (sensorSettings) {
    payload.sensorIntervalMs = Number(form.elements.sensorIntervalMs.value);
    payload.gpio32SensorType = Number(form.elements.gpio32SensorType.value);
    payload.gpio33SensorType = Number(form.elements.gpio33SensorType.value);
    payload.temperature1Enabled = payload.gpio32SensorType !== 0;
    payload.temperature2Enabled = payload.gpio33SensorType !== 0;
    for (const name of ["bme680Enabled",
      "ens160Enabled", "compassEnabled", "gpsEnabled", "rtcEnabled", "pn532Enabled"]) {
      payload[name] = form.elements[name].checked;
    }
  } else {
    payload.i2cClockHz = Number(form.elements.i2cClockHz.value);
    payload.uart1Mode = Number(form.elements.uart1Mode.value);
    payload.uart1Baud = Number(form.elements.uart1Baud.value);
    payload.rs485Baud = Number(form.elements.rs485Baud.value);
    payload.beeperFrequencyHz = Number(form.elements.beeperFrequencyHz.value);
    payload.beeperDurationMs = Number(form.elements.beeperDurationMs.value);
    payload.beeperEnabled = form.elements.beeperEnabled.checked;
    for (const name of ["beeperPin", "fanPin", "fanFrequencyHz", "udpListenPort", "blePresenceTimeoutSeconds"]) {
      payload[name] = Number(form.elements[name].value);
    }
    payload.fanEnabled = form.elements.fanEnabled.checked;
    payload.s2InputEnabled = form.elements.s2InputEnabled.checked;
    payload.displayEnabled = form.elements.displayEnabled.checked;
    payload.displayMode = Number(form.elements.displayMode.value);
    payload.displayPageSeconds = Number(form.elements.displayPageSeconds.value);
    payload.displayBrightness = Number(form.elements.displayBrightness.value);
    payload.displayRotation = Number(form.elements.displayRotation.value);
    payload.displayTextSize = Number(form.elements.displayTextSize.value);
    payload.displayFieldMask = ["oledFieldTemp1", "oledFieldTemp2", "oledFieldHumidity",
      "oledFieldAir", "oledFieldRelays", "oledFieldInputs"].reduce(
        (mask, name, index) => mask | (form.elements[name].checked ? (1 << index) : 0), 0);
    payload.bluetoothMode = Number(form.elements.bluetoothMode.value);
  }

  const submitButton = form.querySelector("button[type=submit]");
  submitButton.disabled = true;
  messageElement.textContent = "";
  try {
    deviceSettings = await window.CapabilityUI.request("/api/settings", payload);
    populateSettingsForms(deviceSettings);
    messageElement.textContent = t("settingsSaved");
    await refreshStatus();
  } catch (error) {
    messageElement.textContent = `${t("settingsError")}: ${error.message}`;
  } finally {
    submitButton.disabled = false;
  }
}

function switchPage(page) {
  document.querySelectorAll(".page-tab").forEach((tab) => {
    const active = tab.dataset.page === page;
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", String(active));
  });
  document.querySelectorAll(".page-view").forEach((view) => {
    const active = view.id === `${page}Page`;
    view.hidden = !active;
    view.classList.toggle("is-active", active);
  });
  if (page !== "overview") loadDeviceSettings();
  if (page === "interfaces") loadRf24Packets();
  if (page === "automation") loadIftttRules();
  refreshStatus();
}

function iftttSourceLabel(source) {
  if (source === "udpPacket") return t("udpPacketSource");
  if (iftttSourceNames[source]) return t(iftttSourceNames[source]);
  if (source.startsWith("digital")) return `DI ${source.slice(7)}`;
  if (source.startsWith("analog")) return `AI ${source.slice(6)} · ADC`;
  return source;
}

function renderIftttRules() {
  const list = $("#iftttRuleList");
  list.replaceChildren();
  $("#iftttRuleCount").textContent = `${iftttRules.length} / 4`;
  if (iftttRules.length === 0) {
    const empty = document.createElement("p");
    empty.className = "empty-state";
    empty.textContent = t("noRules");
    list.append(empty);
  }

  iftttRules.forEach((rule, index) => {
    const row = document.createElement("article");
    row.className = "ifttt-rule";
    const enabled = document.createElement("input");
    enabled.type = "checkbox";
    enabled.checked = Boolean(rule.enabled);
    enabled.setAttribute("aria-label", `Enable rule ${index + 1}`);
    enabled.addEventListener("change", () => { rule.enabled = enabled.checked; });
    const description = document.createElement("span");
    if (rule.action === "capability") {
      description.textContent = `${iftttSourceLabel(rule.source)} ${rule.comparison} ${rule.threshold} → ${rule.command?.id || "--"} ${JSON.stringify(rule.command?.parameters || {})}`;
    } else if (rule.action === "beeper") {
      description.textContent = `${iftttSourceLabel(rule.source)} ${rule.comparison} ${rule.threshold} → ${t("actionBeeper")}`;
    } else if (rule.action === "rf24Saved") {
      const packet = savedRf24Packets.find((saved) => Number(saved.id) === Number(rule.packetId));
      description.textContent = `${iftttSourceLabel(rule.source)} ${rule.comparison} ${rule.threshold} → RF24 ${packet?.name || rule.payload} (${t("rf24PipeNote")})`;
    } else if (rule.action === "rf24Send") {
      description.textContent = `${iftttSourceLabel(rule.source)} ${rule.comparison} ${rule.threshold} → RF24 ${rule.payload} (${t("rf24PipeNote")})`;
    } else if (rule.action === "oledPage") {
      description.textContent = `${iftttSourceLabel(rule.source)} ${rule.comparison} ${rule.threshold} → OLED page ${rule.page}`;
    } else {
      const action = rule.on ? t("turnOn") : t("turnOff");
      description.textContent = `${iftttSourceLabel(rule.source)} ${rule.comparison} ${rule.threshold} → Relay ${rule.relay} ${action}`;
    }
    const remove = document.createElement("button");
    if (rule.source === "udpPacket") {
      const arrow = description.textContent.indexOf(" → ");
      description.textContent = `UDP :${rule.udp?.port} · ${rule.udp?.remoteIp || "*"}:${rule.udp?.remotePort || "*"} · ${rule.udp?.match} ${JSON.stringify(rule.udp?.text || "*")}${description.textContent.slice(arrow)}`;
    }
    remove.type = "button";
    remove.className = "remove-rule";
    remove.textContent = "×";
    remove.setAttribute("aria-label", `Remove rule ${index + 1}`);
    remove.addEventListener("click", () => {
      iftttRules.splice(index, 1);
      renderIftttRules();
    });
    row.append(enabled, description, remove);
    list.append(row);
  });
  $("#addIftttRule").disabled = iftttRules.length >= 4;
}

async function loadIftttRules() {
  try {
    await loadRf24Packets();
    const response = await fetch("/api/ifttt", { cache: "no-store" });
    if (response.status === 503) {
      $("#iftttMessage").textContent = t("nvsUnavailable");
      return;
    }
    if (!response.ok) {
      const error = await response.json().catch(() => ({}));
      if (error.error === "nvs_unavailable") throw new Error(t("nvsUnavailable"));
      if (error.error === "beeper_disabled") throw new Error(t("beeperDisabledError"));
      throw new Error(error.error || `HTTP ${response.status}`);
    }
    iftttRules = await response.json();
    renderIftttRules();
  } catch (error) {
    $("#iftttMessage").textContent = `${t("ruleSaveError")} (${error.message})`;
  }
}

async function saveIftttRules() {
  const button = $("#saveIftttRules");
  button.disabled = true;
  try {
    const response = await fetch("/api/ifttt", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(iftttRules)
    });
    if (response.status === 503) {
      $("#iftttMessage").textContent = t("nvsUnavailable");
      return;
    }
    if (response.status === 409) {
      const error = await response.json();
      $("#iftttMessage").textContent = `${error.error}${error.field ? ` (${error.field})` : ""}`;
      return;
    }
    if (!response.ok) {
      const error = await response.json().catch(() => ({}));
      if (error.error === "nvs_unavailable") throw new Error(t("nvsUnavailable"));
      if (error.error === "beeper_disabled") throw new Error(t("beeperDisabledError"));
      throw new Error(error.error || `HTTP ${response.status}`);
    }
    iftttRules = await response.json();
    $("#iftttMessage").textContent = t("ruleSaved");
    renderIftttRules();
  } catch (error) {
    $("#iftttMessage").textContent = `${t("ruleSaveError")} (${error.message})`;
  } finally {
    button.disabled = false;
  }
}

async function scanNetworks() {
  const select = $("#networkSelect");
  select.innerHTML = `<option value="">${t("scanWait")}</option>`;
  $("#scanButton").disabled = true;
  try {
    const response = await fetch("/api/scan", { cache: "no-store" });
    const result = await response.json();
    select.innerHTML = `<option value="">${t("selectNetwork")}</option>`;
    for (const network of result.networks || []) {
      if (!network.ssid) continue;
      const option = document.createElement("option");
      option.value = network.ssid;
      option.textContent = `${network.ssid} · ${network.rssi} dBm${network.secure ? ` · ${t("secure")}` : ""}`;
      select.append(option);
    }
    if (!select.options.length || select.options.length === 1) {
      select.innerHTML = `<option value="">${t("noNetworks")}</option>`;
    }
  } catch {
    select.innerHTML = `<option value="">${t("noNetworks")}</option>`;
  } finally {
    $("#scanButton").disabled = false;
  }
}

async function scanNearbyBluetooth() {
  const button = $("#bluetoothScanButton");
  const message = $("#bluetoothScanMessage");
  if (Number($("#interfaceSettingsForm").elements.bluetoothMode.value) !== 1) {
    message.textContent = t("bluetoothScanDisabled");
    return;
  }
  button.disabled = true;
  message.textContent = t("scanWait");
  try {
    const result = await window.CapabilityUI.request("/api/bluetooth/scan");
    if (result.error) throw new Error(`${result.error}${result.field ? ` (${result.field})` : ""}`);
    if (!Array.isArray(result.devices)) throw new Error("Invalid BLE response");
    nearbyBluetoothDevices = result.devices;
    const list = $("#bluetoothDeviceList");
    list.replaceChildren();
    if (!nearbyBluetoothDevices.length) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = t("bluetoothNone");
      list.append(empty);
    }
    for (const device of nearbyBluetoothDevices) {
      const card = document.createElement("article");
      card.className = "device-card is-connected";
      const dot = document.createElement("i");
      dot.className = "device-dot";
      const content = document.createElement("div");
      const name = document.createElement("strong");
      name.textContent = device.name || "(unnamed)";
      const address = document.createElement("small");
      address.textContent = device.address || "";
      content.append(name, address);
      const rssi = document.createElement("span");
      rssi.className = "device-state";
      rssi.textContent = `${device.rssi} dBm`;
      const select = document.createElement("button");
      select.type = "button";
      select.className = "secondary-button";
      select.textContent = t("bleSelectId");
      select.addEventListener("click", () => { $("#bleTarget").value = device.address; });
      card.append(dot, content, rssi, select);
      list.append(card);
    }

    message.textContent = `${nearbyBluetoothDevices.length} BLE`;
  } catch (error) {
    $("#bluetoothDeviceList").replaceChildren();
    message.textContent = `${t("bluetoothScanError")} ${error.message}`;
  } finally {
    button.disabled = false;
  }
}

async function loadBleTarget() {
  try {
    const targets = await window.CapabilityUI.request("/api/bluetooth/targets");
    renderSavedBleTargets(targets);
  } catch (error) {
    $("#bleSavedTargets").replaceChildren();
    $("#bleSavedCount").textContent = "-- / 8";
    $("#bleTargetMessage").textContent = error.message;
  }
}
function validateBleTargetsResponse(targets) {
  if (targets.schemaVersion !== 1 || targets.capacity !== 8 || !Array.isArray(targets.addresses) ||
      !Array.isArray(targets.devices) || targets.addresses.length > 8 ||
      new Set(targets.addresses).size !== targets.addresses.length ||
      targets.devices.length !== targets.addresses.length ||
      !targets.devices.every((device, index) => /^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test(device.address) &&
        device.address === targets.addresses[index] &&
        device.property === `ble.presence/${device.address}/in_range` &&
        typeof device.in_range === "boolean" && typeof device.quality === "string")) {
    throw new Error("Invalid saved BLE response");
  }
  return targets;
}
function renderSavedBleTargets(response) {
  const targets = validateBleTargetsResponse(response);
  const list = $("#bleSavedTargets");
  list.replaceChildren();
  for (const device of targets.devices) {
    const card = document.createElement("article");
    card.className = "device-card";
    const content = document.createElement("div");
    const address = document.createElement("strong");
    address.textContent = device.address;
    const state = document.createElement("small");
    state.textContent = `in_range: ${device.quality === "valid" ? (device.in_range ? "1" : "0") : device.quality}`;
    content.append(address, state);
    const remove = document.createElement("button");
    remove.type = "button";
    remove.className = "secondary-button";
    remove.textContent = t("bleRemoveId");
    remove.addEventListener("click", async () => {
      remove.disabled = true;
      try {
        const latest = validateBleTargetsResponse(await window.CapabilityUI.request("/api/bluetooth/targets"));
        const saved = await window.CapabilityUI.request("/api/bluetooth/targets", {
          schemaVersion: 1, addresses: latest.addresses.filter(address => address !== device.address)
        });
        renderSavedBleTargets(saved);
        await window.CapabilityUI.refresh();
        $("#bleTargetMessage").textContent = t("bleIdSaved");
      } catch (error) { $("#bleTargetMessage").textContent = error.message; }
      finally { remove.disabled = false; }
    });
    card.append(content, remove);
    list.append(card);
  }
  $("#bleSavedCount").textContent = `${targets.addresses.length} / 8`;
}
$("#bleTargetSave").addEventListener("click", async () => {
  const button = $("#bleTargetSave");
  button.disabled = true;
  try {
    const address = $("#bleTarget").value.trim().toUpperCase();
    if (!/^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test(address) ||
        address === "00:00:00:00:00:00" || address === "FF:FF:FF:FF:FF:FF") throw new Error(t("bleInvalidId"));
    const latest = validateBleTargetsResponse(await window.CapabilityUI.request("/api/bluetooth/targets"));
    if (latest.addresses.includes(address)) throw new Error(t("bleDuplicateId"));
    if (latest.addresses.length >= 8) throw new Error(t("bleTargetLimit"));
    const targets = await window.CapabilityUI.request("/api/bluetooth/targets", {
      schemaVersion: 1, addresses: [...latest.addresses, address]
    });
    renderSavedBleTargets(targets);
    $("#bleTarget").value = "";
    await window.CapabilityUI.refresh();
    $("#bleTargetMessage").textContent = t("bleIdSaved");
  } catch (error) { $("#bleTargetMessage").textContent = error.message; }
  finally { button.disabled = false; }
});

$("#language").addEventListener("change", (event) => setLanguage(event.target.value));
document.querySelectorAll(".page-tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    switchPage(tab.dataset.page);
    if (tab.dataset.page === "interfaces") loadBleTarget();
  });
});
$("#iftttBuilderForm").addEventListener("submit", (event) => event.preventDefault());
function updateIftttActionFields() {
  const action = $("#iftttAction").value;
  const beeperOption = $("#iftttAction option[value='beeper']");
  const rf24Option = $("#iftttAction option[value='rf24Send']");
  const savedRf24Option = $("#iftttAction option[value='rf24Saved']");
  beeperOption.disabled = !deviceSettings.beeperEnabled;
  rf24Option.disabled = !rf24Compiled || !rf24Available;
  savedRf24Option.disabled = !rf24Compiled || !rf24Available || savedRf24Packets.length === 0;
  if (action === "beeper" && beeperOption.disabled) $("#iftttAction").value = "relay";
  if (action === "rf24Send" && rf24Option.disabled) $("#iftttAction").value = "relay";
  if (action === "rf24Saved" && savedRf24Option.disabled) $("#iftttAction").value = "relay";
  const selectedAction = $("#iftttAction").value;
  const radioAction = selectedAction === "rf24Send";
  const savedRadioAction = selectedAction === "rf24Saved";
  const oledAction = selectedAction === "oledPage";
  const beeperAction = selectedAction === "beeper";
  const capabilityAction = selectedAction === "capability";
  $("#iftttCapabilityFields").hidden = !capabilityAction;
  $("#relayActionFields").hidden = radioAction || savedRadioAction || oledAction || beeperAction || capabilityAction;
  $("#rf24ActionFields").hidden = !radioAction;
  $("#rf24SavedActionFields").hidden = !savedRadioAction;
  $("#oledActionFields").hidden = !oledAction;
  $("#beeperActionFields").hidden = !beeperAction;
}
$("#iftttAction").addEventListener("change", updateIftttActionFields);
function updateIftttSourceFields() {
  const udp = $("#iftttSource").value === "udpPacket";
  $("#iftttUdpFields").hidden = !udp;
  $("#iftttNumericCondition").hidden = udp;
}
$("#iftttSource").addEventListener("change", updateIftttSourceFields);
function udpRuleFromForm() {
  const port = Number($("#iftttUdpPort").value);
  const remotePort = Number($("#iftttUdpRemotePort").value);
  const remoteIp = $("#iftttUdpRemoteIp").value.trim();
  const text = $("#iftttUdpText").value;
  if ($("#iftttUdpPort").value === "" || !Number.isInteger(port) || port < 1 || port > 65535 ||
      port === 5353 || port === Number(deviceSettings.udpListenPort) ||
      $("#iftttUdpRemotePort").value === "" || !Number.isInteger(remotePort) || remotePort < 0 ||
      remotePort > 65535 || (remoteIp && (!/^\d{1,3}(\.\d{1,3}){3}$/.test(remoteIp) ||
        remoteIp.split(".").some((part) => Number(part) > 255))) ||
      new TextEncoder().encode(text).length > 256 || /[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]/.test(text)) {
    throw new Error("Invalid UDP trigger port/address/payload");
  }
  return { port, remotePort, remoteIp, match: $("#iftttUdpMatch").value, text };
}
$("#addIftttRule").addEventListener("click", () => {
  if (iftttRules.length >= 4) return;
  const action = $("#iftttAction").value;
  if ($("#iftttSource").value !== "udpPacket" && $("#iftttComparison").value !== "changed" &&
      ($("#iftttThreshold").value === "" || !Number.isFinite(Number($("#iftttThreshold").value)))) {
    $("#iftttMessage").textContent = t("invalidThreshold");
    return;
  }
  if (action === "rf24Send" && ($("#iftttRfPayload").value === "" ||
      !Number.isInteger(Number($("#iftttRfPayload").value)) ||
      Number($("#iftttRfPayload").value) < 0 || Number($("#iftttRfPayload").value) > 4294967295)) {
    $("#iftttMessage").textContent = t("invalidRf24Payload");
    return;
  }
  let udp;
  if ($("#iftttSource").value === "udpPacket") {
    try { udp = udpRuleFromForm(); }
    catch (error) { $("#iftttMessage").textContent = error.message; return; }
  }
  let command;
  if (action === "capability") {
    try {
      command = window.CapabilityUI.getIftttCommand();
    } catch (error) {
      $("#iftttMessage").textContent = error.message;
      return;
    }
  }
  const selectedPacket = action === "rf24Saved"
    ? savedRf24Packets.find((packet) => Number(packet.id) === Number($("#iftttRf24SavedPacket").value))
    : null;
  if (action === "rf24Saved" && !selectedPacket) {
    $("#iftttMessage").textContent = t("rf24SelectSaved");
    return;
  }
  iftttRules.push({
    enabled: true,
    source: $("#iftttSource").value,
    ...(udp ? { udp } : {}),
    comparison: $("#iftttComparison").value,
    threshold: udp ? 0 : Number($("#iftttThreshold").value),
    action,
    ...(command ? { command } : {}),
    relay: action === "relay" ? Number($("#iftttRelay").value) : 0,
    on: $("#iftttRelayState").value === "true",
    payload: action === "rf24Send" ? Number($("#iftttRfPayload").value) : selectedPacket?.payload ?? 0,
    packetId: selectedPacket?.id ?? 0,
    page: action === "oledPage" ? Number($("#iftttOledPage").value) : 0
  });
  renderIftttRules();
});
$("#saveIftttRules").addEventListener("click", saveIftttRules);
$("#rf24SaveForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const message = $("#rf24Message");
  const button = $("#rf24SaveButton");
  const packet = rf24History.find((item) => Number(item.sequence) === Number($("#rf24CaptureSelect").value));
  if (!packet) {
    message.textContent = t("rf24CaptureNone");
    return;
  }
  button.disabled = true;
  try {
    const response = await fetch("/api/rf24/packets", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: $("#rf24PacketName").value.trim(), payload: packet.payload >>> 0 })
    });
    if (!response.ok) throw new Error("save_rf24");
    $("#rf24PacketName").value = "";
    await loadRf24Packets();
    message.textContent = t("rf24SaveSuccess");
  } catch {
    message.textContent = t("rf24Error");
  } finally {
    button.disabled = false;
    renderRf24Monitor();
  }
});
$("#sensorSettingsForm").addEventListener("submit", (event) => {
  event.preventDefault();
  saveDeviceSettings(event.currentTarget, $("#sensorSettingsMessage"), true);
});
$("#interfaceSettingsForm").addEventListener("submit", (event) => {
  event.preventDefault();
  saveDeviceSettings(event.currentTarget, $("#interfaceSettingsMessage"), false);
});
$("#wifiButton").addEventListener("click", () => {
  $("#wifiDialog").showModal();
  scanNetworks();
});
$("#bluetoothScanButton").addEventListener("click", scanNearbyBluetooth);
$("#iftttAction").dispatchEvent(new Event("change"));
$("#scanButton").addEventListener("click", scanNetworks);
$("#beeperTestButton").addEventListener("click", async () => {
  const button = $("#beeperTestButton");
  const message = $("#beeperTestMessage");
  button.disabled = true;
  message.textContent = "";
  try {
    const response = await fetch("/api/beeper/test", { method: "POST" });
    if (response.status === 409) {
      message.textContent = t("beeperDisabledError");
      return;
    }
    if (!response.ok) throw new Error("beeper");
    message.textContent = t("beeperTested");
  } catch {
    message.textContent = t("beeperTestError");
  } finally {
    button.disabled = false;
  }
});
$("#networkSelect").addEventListener("change", (event) => {
  if (event.target.value) $("#ssidInput").value = event.target.value;
});

$("#wifiForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const message = $("#wifiMessage");
  const payload = {
    ssid: $("#ssidInput").value.trim(),
    password: $("#passwordInput").value
  };
  try {
    const response = await fetch("/api/wifi", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    if (!response.ok) throw new Error("save");
    message.textContent = t("saved");
    $("#passwordInput").value = "";
    setTimeout(() => location.reload(), 5000);
  } catch {
    message.textContent = t("saveError");
  }
});

document.querySelectorAll("[data-relay] .toggle").forEach((button) => {
  button.addEventListener("click", async () => {
    const row = button.closest("[data-relay]");
    const on = button.getAttribute("aria-checked") !== "true";
    button.disabled = true;
    try {
      const response = await fetch("/api/relay", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ channel: Number(row.dataset.relay), on })
      });
      if (!response.ok) throw new Error("relay");
      await refreshStatus();
    } catch {
      showToast(t("relayError"));
    } finally {
      button.disabled = false;
    }
  });
});

setLanguage(language);
loadDeviceSettings();
refreshStatus();
statusTimer = setInterval(refreshStatus, 1500);