# KinConductor — wykonane przyrosty refaktoryzacji

[Aktywny plan i pozostałe zadania](./REFACTOR_TODO.md) · [Dokumentacja](./docs/README.md)

Archiwum zweryfikowanych przyrostów. Historyczne liczby i nazwy profili
nie są aktualnymi poleceniami wdrożenia. Ograniczenia testów pozostawiono
przy wynikach; zakończony przyrost nie oznacza ukończenia całego planu.

### Postęp realizacji — pierwszy przyrost 2026-10-31

| Zadanie | Status częściowy | Zrealizowano / pozostało |
| --- | --- | --- |
| BASE-01 | Rozpoczęte | Build baseline pięciu profili: SUCCESS, Arduino-ESP32 3.3.12 / IDF libraries 5.5.5. Dodano native characterization ADC/JSON. Pozostają fixtures config/rules/RF24 i testy pozostałych zachowań. |
| CORE-01 | Rozpoczęte | C++17 lifecycle, typowane properties/quality, trigger descriptors i synchroniczne akcje; fake peripheral w testach. Pozostają pełne schemas/ranges/enums/config i pozostałe kontrakty planu. |
| CORE-02 | Rozpoczęte | Bounded registry (8 bindingów), explicit interfaces bez RTTI, atomowa walidacja rejestracji, revision, begin/tick/stop/remove i action dispatch. Pozostają App ownership, EventBus i osobny scheduler. |
| IO-01 | Rozpoczęte tylko ADC | AnalogInputs ma injected hardware, 4 kanały 12-bit, snapshots i error recovery. Legacy JSON zachowuje pełny zakres; IFTTT odczytuje ADC przez registry, OLED używa cache. Relays/DI/S2/beeper nadal w main. |

Pliki: [kontrakty](./include/core/peripheral.h),
[rejestr](./include/core/peripheral_registry.h),
[implementacja rejestru](./src/core/peripheral_registry.cpp),
[ADC](./include/peripherals/analog_inputs.h),
[implementacja ADC](./src/peripherals/analog_inputs.cpp),
[legacy serializer](./src/web/analog_status.cpp) i
[testy native](./test/test_core.cpp).

Pierwszy przyrost jest ograniczoną ekstrakcją/charakterystyką: nie oznacza
ukończenia zależnego IO-01 przed BOARD-01/CFG-01. Nie uruchamia nowych
urządzeń, endpointów ani schedulera. Obecny timer wywołuje registry tick
w dotychczasowym interwale pomiarów; docelowy scheduler nadal jest zadaniem.
Deskryptory triggerów nie publikują jeszcze discrete events. Błędy rdzenia
są raportowane do Serial i istniejącego logu zdarzeń przez composition root.

Walidacja native: `bash test/run_native.sh` — 9 grup testów, ASan/UBSan,
`-fno-rtti`, `-fno-exceptions`, warnings-as-errors; dokładny JSON dla
0/255/256/2048/4095, lifecycle/limits/duplicate IDs, action types i
timestamp rollover. To testy hostowe z fake hardware, nie pomiar napięć,
runtime heap ani testy fizycznej płytki.

Walidacja firmware po pierwszym przyroście:
`pio run -e kc868-a6 -e kc868-a6-lora -e kc868-a6-nrf24 -e kc868-a6-gps -e kc868-a6-coprocessor`
— **5/5 SUCCESS**. Pomiary tego samego toolchain przed/po:

| Profil | Przyrost statycznego RAM | Przyrost flash według size check | Firmware.bin po zmianie |
| --- | --- | --- | --- |
| kc868-a6 | +344 B | +5000 B | 1 452 768 B |
| kc868-a6-lora | +344 B | +5120 B | 1 453 632 B |
| kc868-a6-nrf24 | +344 B | +5212 B | 1 456 400 B |
| kc868-a6-gps | +344 B | +5000 B | 1 452 832 B |
| kc868-a6-coprocessor | +344 B | +5004 B | 1 452 944 B |

Size check i rozmiar pliku bin to różne metryki; slot OTA zajęty w około
92,4–92,6%. Statyczny RAM nie jest pomiarem wolnego heap podczas pracy.
Nie zmieniono partycji ani zależności. Nowy serializer ADC zgłasza brak
pojemności JSON; composition root loguje błąd, a istniejący handler statusu
zwraca HTTP 500 zamiast częściowej poprawnie wyglądającej tablicy.

### Postęp realizacji — drugi przyrost: konfiguracja

| Zadanie | Status częściowy | Zrealizowano / pozostało |
| --- | --- | --- |
| BASE-01 | Rozpoczęte | 9 dodatkowych grup native: exact settings JSON, schema/limity, profile UART, import legacy, wersje i błędy zapisu/readback. Rules/RF24 i testy hardware pozostają. |
| CORE-01 | Rozpoczęte | Jeden typowany schemat 27 istniejących pól, walidacja przed narrowing i wspólne kodeki JSON API/NVS. Pełne per-module IConfigurable nadal planowane. |
| CFG-01 | Rozpoczęte | ConfigService, injected ISettingsStore, adapter Preferences, rekord schemaVersion=1, brak publikacji RAM/apply po błędzie zapisu. ResourceManager, candidate/active z recovery sprzętu i pełna migracja aplikacji pozostają. |

Pliki: [model i schema](./include/config/device_settings.h),
[serwis](./include/config/config_service.h),
[kodek API/rekordu](./src/config/settings_json.cpp),
[adapter NVS](./src/config/preferences_settings_store.cpp),
[testy konfiguracji](./test/test_config.cpp) i
[native Preferences fake](./test/support/Preferences.h).
WifiWebUi otrzymuje serwis przez konstruktor; nie posiada już modelu ani
własnego loadera ustawień sprzętu. Bez zmian: pola odpowiedzi settings,
domyślne profile UART, limity i wyliczanie temperatureEnabled z selectorów.
Świadoma korekta API: odrzucanie unknown fields, null, błędnych typów,
liczb ujemnych/ułamkowych i overflow zamiast fallback/narrowing.
No-op nie zapisuje NVS ani nie resetuje sensorów/UART przez callback apply.

Import legacy jest odczytem: nie kasuje starych kluczy. Pierwszy zmieniony
zapis tworzy autorytatywny `device_cfg`; późniejsze zapisy nie aktualizują
legacy, więc downgrade starego firmware zobaczy starsze ustawienia.
Rekord uszkodzony lub z nieobsługiwaną wersją nie jest automatycznie
zastępowany ani zamieniany na stare legacy. Po błędzie inicjalizacji
firmware jawnie loguje tryb zdegradowany i korzysta z defaults profilu;
settings API jest niedostępne (503). Status dodaje `configuration.ready`
i `configuration.error`, bez usuwania dotychczasowych pól.

Zapis jest pojedynczym `putString` z kontrolą długości i dokładnego readback.
Nie jest transakcją obejmującą sprzęt: SettingsApplier nadal zwraca void.
Błąd zapisu/readback zwraca 500, blokuje kolejne settings requests (503),
nie publikuje candidate do RAM i nie wywołuje apply. Jeżeli readback zawiódł
po commit, nowy rekord może już istnieć; restart/reload może go aktywować.
Nie deklarować rollback ani odporności na fizyczne zaniki zasilania bez testu
NVS na płytce. Pozostałe storage Wi-Fi/auth/IFTTT/RF24 nie są jeszcze przeniesione.

Walidacja native: łącznie **18 grup PASS**, ASan/UBSan, warnings-as-errors,
bez RTTI/wyjątków. Production adapter testowany z fake Preferences,
nie jest to test implementacji NVS w ESP-IDF. Testy sprawdzają wszystkie
minimum/maximum pól liczbowych, brightness=255, brak zmian po invalid,
niezapisywanie no-op, odczyt legacy i autorytatywnego rekordu, unsupported
version, failed open/write/readback i odzyskanie po reload.

Walidacja końcowego firmware: **5/5 SUCCESS** dla tych samych profili.
Przyrost względem pierwszego przyrostu: **+944 B statycznego RAM** w każdym
profilu i około 4,2 KiB linked flash (size check).

| Profil | Statyczny RAM | Flash według size check | Firmware.bin |
| --- | --- | --- | --- |
| kc868-a6 | 66 600 B | 1 432 611 B | 1 457 248 B |
| kc868-a6-lora | 66 600 B | 1 433 283 B | 1 458 064 B |
| kc868-a6-nrf24 | 66 856 B | 1 435 291 B | 1 460 752 B |
| kc868-a6-gps | 66 600 B | 1 432 659 B | 1 457 296 B |
| kc868-a6-coprocessor | 66 600 B | 1 432 771 B | 1 457 408 B |

Wszystkie bin mieszczą się w niezmienionych slotach OTA 1 572 864 B
(92,65–92,87% zajętości). Brak zmiany zależności/partycji, flashowania
i pomiarów runtime heap; margines flash nadal jest niewielki.

### Postęp realizacji — trzeci przyrost: profil płytki i zasoby

| Zadanie | Status częściowy | Zrealizowano / pozostało |
| --- | --- | --- |
| BOARD-01 | Rozpoczęte | Profil 26 GPIO, input-only/ADC/DAC/strapping, bounded ResourceManager dla GPIO/UART/I2C adresów i rezerwacji SPI CS. LEDC/RMT/I2S, dynamiczne release i pełny hardware manifest pozostają. |
| CFG-01 | Rozpoczęte | Injected ISettingsValidator: sprawdzanie zasobów po walidacji schematu, przed save oraz przy load. Konflikt nie publikuje candidate; brak transakcyjnego hardware apply pozostaje. |
| DIAG-01 | Rozpoczęte | `/api/board`, tabela GPIO/role/rezerwacje/ograniczenia i adresy/maski PCF8574 w zakładce Diagnostyka. Pełny diagram, heap/stack/queues i metryki magistral pozostają. |
| BASE-01 | Rozpoczęte | 7 grup native dla board/resources/config/API, 4 izolowane testy UI i test manifestów radio; pozostałe characterization/hardware wciąż planowane. |

Pliki: [profil](./include/board/board_profile.h),
[ResourceManager](./include/board/resource_manager.h),
[plan i walidator](./src/board/settings_resources.cpp),
[serializer diagnostyki](./src/web/board_status.cpp),
[testy płytki](./test/test_board.cpp) i [testy UI](./test/test_web.js).
Istniejące stałe pinów nie zmieniły wartości; header nie wymaga Arduino.
Rezerwacje są logicznym planem konfiguracji, nie wykryciem ani pomiarem
zajętości sprzętu. Shared I2C/SPI należą do bus owner, klienci zajmują
adres/CS. Metadata i owner strings mają czas życia dłuższy od rezerwacji.
Plan jest publikowany dopiero po pełnej walidacji; błąd zachowuje poprzednią
tabelę. ResourceManager ma limit 48 wpisów i jawny błąd overflow.

Sprawdzone: czwarty UART jest niedozwolony; P17 na UART1/2 wymaga zwolnienia
RS232/RS485; P17 RX13 koliduje z PN532 SPI CS13; PN532 I2C 0x24 koliduje
z przekaźnikami; beeper GPIO26 koliduje z funkcjonalnym DAC1; input-only
GPIO34/35/36/39 odrzucają output; LoRa i nRF24 równocześnie są błędem build.
Strapping jest jawnie opisany, a nie automatycznie blokowany; potrzebne testy
resetu i warunków elektrycznych. Nie uruchomiono nowych tras P17, DAC ani
PN532 I2C. Obecne profile GPS/coprocessor nadal używają UART1 GPIO16/17.
Legacy RTC/BME680/ENS160/compass rezerwują adresy nawet przy wyłączonym
reporting, ponieważ istniejąca inicjalizacja nadal dotyka tych urządzeń.
Przeniesienie/gating tych driverów będzie osobnym, testowanym przyrostem.

Walidator działa przy load i przed NVS save. Konflikt request zwraca 409,
bez zapisu/apply; load konfliktowego rekordu pozostawia defaults i stan
config unavailable. `/api/board` pozostaje czytelne także przy tym błędzie,
pokazując stan konfiguracji oddzielnie od poprawności planu RAM/defaults.
UI odrzuca malformed JSON shape, usuwa stare rezerwacje po błędzie
i nie nakłada requestów. PL/EN etykiety i pinout pochodzą z API, nie
duplikowanej listy GPIO w JavaScript.

Walidacja: **25 grup native PASS** (ASan/UBSan, warnings-as-errors,
bez RTTI/wyjątków), **4 testy DOM PASS**, JS syntax i radio manifest checks,
**5/5 firmware SUCCESS**. Browser/layout oraz fizyczne NVS/GPIO nie były
testowane; brak dostępnego klienta narzędzi browser. Nowy JSON ma bounded
capacity wyliczoną przez ArduinoJson macros dla liczby pinów/rezerwacji,
uwzględniając inną wielkość slotu na host i ESP32.

| Profil | Statyczny RAM | Flash według size check | Firmware.bin |
| --- | --- | --- | --- |
| kc868-a6 | 66 624 B | 1 436 931 B | 1 461 760 B |
| kc868-a6-lora | 66 624 B | 1 437 607 B | 1 462 592 B |
| kc868-a6-nrf24 | 66 880 B | 1 439 715 B | 1 465 376 B |
| kc868-a6-gps | 66 624 B | 1 436 979 B | 1 461 808 B |
| kc868-a6-coprocessor | 66 624 B | 1 437 091 B | 1 461 920 B |

Przyrost względem konfiguracji: +24 B statycznego RAM i 4320–4424 B linked
flash; obrazy zajmują 92,94–93,17% OTA. Bez nowych zależności, zmian partycji
i flashowania. Aktualizacja WebGUI wymaga wgrania nowych assets LittleFS.
Końcowa aktualizacja obejmuje README i package.json (description, keywords,
Node dev requirement, scripts test/build/matrix/buildfs); wersja pozostaje
1.0.0, ponieważ nie wykonano wydania. Nie ma pliku ipackage.json ani innego
manifestu paczki wymagającego równoległej zmiany.
Sprawdzono `npm test` (25 grup native i 4 testy DOM) oraz `npm run build:fs`
(LittleFS image SUCCESS). Zweryfikowano JSON/scripts metadanych, lokalne
linki, identyfikatory DOM i zachowanie 50 zadań checklisty.

### Postęp realizacji — czwarty przyrost: capabilities → UI/OLED/IFTTT

**Zakres planu nadal nie jest ukończony.** Ten przyrost uruchamia przepływ
end-to-end dla istniejących odczytów i przekaźników; nie deklaruje nowych
driverów/protokołów ani ukończenia zależnych 50 zadań.

| Zadanie | Status częściowy | Zrealizowano / pozostało |
| --- | --- | --- |
| IO-01 | Rozpoczęte | BoardIo: 8 DI/6 relays, readback snapshots, typed action channel/on; manual API i legacy IFTTT używają registry dispatch. Hardware helper/DI log/S2/beeper nadal częściowo main. |
| SENS-01 | Rozpoczęte | SensorReadings: 16 typowanych cached properties dla obecnych temperature/humidity/air/GPS/compass. Inicjalizacja i pomiar driverów pozostają w main; nie jest to pełna ekstrakcja sensorów. |
| CORE-01 | Rozpoczęte | Numeric ranges w ParameterDescriptor i walidacji dispatcher/API. Per-module configurable/config schema ownership nadal pending. |
| API-01 | Rozpoczęte | `/api/capabilities`, `/api/actions`, `/api/settings/schema`, `/api/screen` z istniejącą polityką auth; jawne błędy/capacity. Versioned pełny API, pagination i migration contracts pozostają. |
| WEB-01/WEB-02 | Rozpoczęte | Nowe zakładki IO/Ekran/Ustawienia, module cards i typed action forms z descriptors; 27-field settings form ze wspólnego schema. Pozostałe strony nadal legacy, pełny page/widget registry nie istnieje. |
| OLED-01/GRID-01 | Rozpoczęte | Portable bounded layout, per-page rows/columns, binding ID, preview, NVS record i renderer SSD1306 dla displayMode=5. Driver/manager/page/widget separation i nowe controllers pending. |
| IFTTT-01 | Rozpoczęte | Registered property sources i generic capability command; wspólny dispatcher przekaźnika. Evaluator nadal jest legacy, ale obsługuje teraz 8 reguł i grupy akcji; nowy event engine/scheduler pending. |
| BASE-01 | Rozpoczęte | 3 nowe native grupy i 4 nowe JS tests, w tym schema/capability JSON, max pages/widgets, typed actions oraz HTML zamiast JSON. Hardware matrix/power-loss pending. |

Pliki: [BoardIo](./include/peripherals/board_io.h),
[SensorReadings](./include/peripherals/sensor_readings.h),
[API codec](./src/web/capabilities.cpp),
[layout](./include/display/screen_layout.h),
[layout codec](./src/display/screen_layout.cpp),
[dynamic UI](./data/capabilities.js),
[native tests](./test/test_capabilities.cpp) i
[UI tests](./test/test_capability_ui.js).

Registry bindings są czytelne bez RTTI. API udostępnia trzy moduły i
34 properties (4 ADC + 14 IO + 16 sensor cache), z quality/type/unit/timestamp,
trigger descriptors i jedną akcją przekaźnika. Missing/error/disabled values
są null. UI tworzy controls z deskryptorów, nie własnej whitelisty.
SensorReadings publikuje obecne cache; DHT używa lastRead timestamp,
GPS odrzuca fix starszy niż 30 s; failed fresh BME/ENS/compass nie jest
prezentowany jako poprawny nowy pomiar. Nie są to nowe drivery.

Action request: `{"id":"io.main/relay/set","parameters":{"channel":1,"on":true}}`.
Channel 1–6 i on bool są walidowane przy zapisie reguły i przed wykonaniem.
Write helper wymaga odczytu stanu przed modyfikacją, utrzymuje P6/P7 HIGH
i sprawdza readback. Błąd po zapisie pozostaje jawny (applied może być true,
confirmed false); nie ma rollback fizycznego wyjścia.
Legacy manual relay i IFTTT relay przechodzą przez ten sam dispatcher.

IFTTT zachowuje formaty legacy oraz przyjmuje `action:"capability"` i `command`
w formacie action request. Source może być zarejestrowanym property ID.
Stare 4 reguły/evaluator/timer nie zostały zastąpione nowym silnikiem.
Settings schema jest aplikacyjny (27 pól), nie per-module IConfigurable.

Screen record schemaVersion=1 w `screen_grid`: pages[{rows,columns}],
widgets[{page,row,column,property,label}]. Zakres wdrożony: 1–8 stron,
grid 1–4 rows/columns, **16 widgetów łącznie**, zero-based positions,
ASCII labels do 16 znaków. Ograniczenie total zamiast 16 per-page celowo
utrzymuje bounded RAM; większy budżet wymaga pomiarów i osobnego przyrostu.
Duplicate cells/unknown bindings/invalid types/ranges odrzucane przed zapisem.
Single NVS write/readback; błąd nie publikuje candidate, lecz committed record
po failed readback może różnić się od RAM. Corrupt layout jest logowany,
GET zwraca 503; valid POST może go zastąpić. NVS power-loss nie przetestowano.
Nowy displayMode=5 cyklicznie renderuje wartości registry na SSD1306.
Stare tryby/ustawienia pozostają. Nie wdrożono SSD1309/SH1107 ani pageManager.

Walidacja: `npm test` — **28 native groups + 8 JS tests PASS**, ASan/UBSan,
warnings-as-errors. Browser smoke na **jawnie oznaczonym synthetic preview**
sprawdził module cards, relay command form (bez real hardware), zapis 2 stron,
27 pól Settings i capability IFTTT rule, bez page errors. Preview plików bez
API zwracał HTML/404; request helper teraz sprawdza Content-Type i pokazuje
jawne „brak API JSON” zamiast Unexpected token '<'; przypadek ma regresję.
Static preview nie jest firmware i nie steruje płytką.
Próba dodatkowej walidacji mobile została przerwana timeoutem kliknięcia;
nie deklarować jej jako PASS.

Final firmware matrix: **2/2 PASS**; LittleFS base build **PASS**.
Complete firmware.bin: base 1 485 872 B, coprocessor 1 486 048 B,
~94,47–94,48% OTA slot (1 572 864 B), ~85 KiB pozostaje.
Static RAM: 69 040 B (+2 416 B); runtime heap/latency nie zmierzono.
To nie jest budżet dla wszystkich planowanych driverów naraz.
Final action error response zachowuje applied/confirmed, tak aby partial
hardware write nie wyglądał jak operacja, która na pewno nie nastąpiła.
Docs links/metadata/page wiring/checklist i `git diff --check` PASS.
README, rozdziały wiki i package description/test script zaktualizowano;
bez zmian dependencies/partition table/release version.
**Ten firmware i nowe assets nie zostały jeszcze wgrane na płytkę**;
ostatni fizyczny upload A6 dotyczy wcześniejszego przyrostu diagnostycznego.
Synthetic preview zatrzymano po testach; jego dane nie są stanem urządzenia.

**Pozostały brakujący zakres:** WS2812, DHT11, SSD1309/SH1107, fan PWM,
relay RAM/NVS startup policy; UDP/TCP/MQTT/API/DAC/RS232/RS485 capability modules;
P17 routing i coprocessor/custom bridge; NTP/scheduler/network settings/mDNS;
pełny OLED driver/manager/widgets i dynamiczny WebGUI page/provider registry.
Bluetooth/LoRa/RF24/NFC drivers oraz ich osobne zakładki nie zostały
w tym przyroście przeniesione na nowe kontrakty. Te zadania nadal są otwarte.

### Postęp realizacji — piąty przyrost: BLE/transport/PWM i poprawki formularzy

Zrealizowano zakres kolejnego zgłoszenia użytkownika, z poniższymi ograniczeniami
sprzętowymi. **Cały 50-zadaniowy plan nadal nie jest ukończony.**

- WEB-01/WEB-02: parametry akcji mają czytelne etykiety i pionowy układ;
  pola liczbowe 38 px, boolean checkbox 20×20 px również w IFTTT.
  Odświeżanie odczytów nie odtwarza formularza i nie gubi wpisanych parametrów.
  Usunięto legacy duplikaty ADC/DI/sensorów z nowej listy źródeł;
  zapisane starsze reguły zachowują kompatybilność.
- SENS-01/DHT-01: DHT11 jako sensorType=3 na GPIO32/33 w listach Sensors i walidacji
  konfiguracji; inicjalizacja DHT z odpowiednim typem, odczyty T/RH/timestamp.
  0/1/2 bez zmian. Nadal istnieje adapter cache, nie pełne wydzielenie driverów.
- COMM-01/IFTTT-01: wspólny nieblokujący scanner, wybór MAC z listy i zapis
  `/api/bluetooth/target` do NVS z readback. `ble.presence/in_range`:
  warming_up do pierwszej obserwacji, następnie true, false po timeout
  (default 60 s, WebGUI 5–3600 s). Wyłączony scan/beacon nie udaje valid absence.
  `==1` wejście, `==0` wyjście; timeout to opóźniona obserwacja reklam BLE,
  nie natychmiastowy status połączenia. Private rotating MAC nie gwarantuje tożsamości.
- NET-01/UDP-01/SER-01/IFTTT-01: `transport.main` daje Wi-Fi connected,
  UDP RX count/text, UART1/2 RX count/text, UDP send i serial send.
  UDP listenPort=0 wyłącza RX; UART1 zablokowany dla GPS/koprocesora.
  UART2 to istniejący RS485. Payload max 256 B valid UTF-8/TAB/CR/LF,
  serial LF/CRLF albo idle 25 ms; overflow odrzuca ramkę z błędem.
  To nie są dowolne protokoły binarne ani potwierdzenia odbioru u partnera.
  Empty UDP nie jest wiarygodnie obserwowalne przez WiFiUDP w tym SDK.
- IFTTT-01/CORE-01: comparison `changed` dla liczników odbioru/kolejnych zmian,
  pierwszy valid sample ustala baseline; nie gubi kolejnej zmiany przez
  poprzedni true match. Portable ConditionState testuje edge/recovery.
  Ewaluacja co 100 ms dostępności main loop; legacy blocking I/O może opóźniać,
  burst między ewaluacjami jest agregowany. Nadal limit 4 reguł.
  Provider.validate + Registry.validateAction służą side-effect-free walidacji
  saved commands; zakresy, host, długość/payload sprawdzane także w dry-run.
  JSON string lengths są zachowane, embedded NUL nie jest cicho obcinany.
- PWM-01/BOARD-01: `fan.main/set(percent 0..100)`, duty property, 8-bit LEDC
  na DA-2/DAC2/GPIO25 default **disabled**, frequency 100–25000 Hz, default1kHz.
  WebGUI wybiera GPIO25/26; beeper wybiera DA-1/DAC1/GPIO26 default lubGPIO25.
  Włączone konflikty pinów/DAC odrzucane przed NVS (409); osobne timery LEDC
  i kanały 4/6. Duty=0 po starcie/konfiguracji; nie zapisywać niezerowego
  wysterowania startowego automatycznie.
  **To logiczny PWM 3.3 V dla oscyloskopu/zewnętrznego konwertera PWM→0–10 V,
  nie analogowe 0–10 V, pomiar RPM ani sterowanie triakiem AC.**
- GRID-01/OLED-01: widget.font=0 Classic / 1 FreeSans9 / 2 FreeMono9,
  textSize=1–3, unit ASCII max 8 znaków. Pusta jednostka używa descriptor unit.
  Osobny bounded canvas per-cell obcina większy font bez nadpisania sąsiednich pól.
  Browser preview jest orientacyjny, nie identyczny z rasteryzacją GFX.
- COMM-01/BASE-01: całe RF24 driver/history/storage/replay jest pod
  `USE_RADIO_NRF24`; obie utrzymane konfiguracje bez tego flag/dependency.
  GET monitor: compiled=false; UI ukrywa RF. nm potwierdza brak symboli
  RF24::/history/replay w final base ELF. Nie dodano zbędnego osobnego env radio;
  base `kc868-a6` to żądany build bez RF24.
- CFG-01: schema 33 pola; device_cfg version=2, zachowana migracja kompletnego
  v1 z 27 pól + bezpieczne wartości domyślne nowych. Corrupt/missing original
  fields nadal oznaczają błąd. Optional font/unit są zgodne ze starszymi widgetami
  w layout version=1; starszy firmware nie rozumie record v2/rozszerzonych
  widgetów przy downgrade.

Nowe moduły:
[BLE](./include/peripherals/ble_presence.h),
[BLE ESP32 adapter](./src/peripherals/esp32_ble_presence_scanner.cpp),
[transport](./include/peripherals/transport.h),
[transport ESP32 adapter](./src/peripherals/esp32_transport.cpp),
[PWM](./include/peripherals/fan_control.h),
[IFTTT conditions](./include/core/ifttt_condition.h).
Registry: **6 modułów / 43 properties / 4 akcje**; schema aplikacji ma 33 pola.

Walidacja: `npm test` PASS — 29 dotychczasowych grup + BLE/transport native suites,
ASan/UBSan, 9 JS tests; pełne 6-module capabilities mieści się w 24 KiB
JSON workspace. Firmware matrix 2/2 i LittleFS PASS.
Final base firmware **1 513 312 B**, coprocessor 1 513 472 B
(~96,21–96,22% OTA; ~58 KiB wolne); static RAM 74 384 B.
Real board API/UDP tests: free heap 53 736 B, min 22 544 B
(wcześniejsza obserwacja min 22 136 B); to nie jest worst-case/long-run
certification ani budżet na wszystkie rozszerzenia.

**Wgrano final A6 bez coprocesora/RF24 oraz matching LittleFS na /dev/ttyUSB0,
hash verified, NVS/partitions zachowane**, po potwierdzeniu bezpiecznych obciążeń.
Real API: capabilities=6/schema=33/Wi-Fi=true; BLE bad ID reject +
canonical save/readback i warming_up; PWM bounds/conflict 409;
font/unit save/readback/reject/restore.
Real UDP: RX counter/text i TX reply otrzymane na local socket;
tymczasowy port przywrócono. PWM GPIO25 enable/50% duty/API property PASS,
potem 0/disabled/restored; nie zmierzono oscyloskopem ani napędu.
Browser real proxy DOM smoke: pola 38 px/checkbox 20×20, canonical IFTTT
41 sources, 3 font choices, DHT11 option, Settings 33; IO viewport 390 px bez
horizontal overflow. Native browser click timeout -> użyto DOM clicks;
nie jest to pełny test pointer/accessibility.
Testowe układy/targets/settings odtworzono. Proxy/helper zatrzymano i usunięto.
Physical DHT11/BLE range/UART peer/0–10 V converter/waveform nadal wymagają sprzętu.

README/wiki/package opis i build:a6 script zaktualizowane jako ostatni krok;
bez nowych dependencies/partition changes/release version bump.
Pozostałe drivery/protokoły i zadania planu nadal mają status otwarty.

Następny krok: dokończyć BASE-01/CORE-01, następnie BOARD-01/CFG-01 i
pozostałą ekstrakcję IO zgodnie z zależnościami. Nie zastępować faktycznych
testów hardware zaznaczonym checkboxem.

### Postęp realizacji — szósty przyrost: pomiar prądu, sieć i wydanie 1.1.0

- [x] Header WebGUI: model/revision CPU, dwa rdzenie/MHz, heap free/total,
  PSRAM i fizyczny flash/rozmiar firmware z realnych API ESP32.
  MHz to nie procent obciążenia CPU; RAM opisuje heap, nie całe SRAM.
- [x] `current.main`: lifecycle/property/trigger, cztery istniejące kanały ADC,
  sygnał V/mA i ampery (osiem properties/conditions) dla IO/IFTTT/OLED.
  Cztery kanały domyślnie OFF; brak automatycznego zakładania podłączonego sensora.
- [x] Oddzielny schemaVersion 1/NVS `current_cfg`, GET/POST `/api/current`,
  dwupunktowe raw/signal/current endpoints, explicit calibrated, walidacja
  finiteness/ranges/types, error zamiast clamp/extrapolation poza zakresem,
  propagacja jakości/timestamp, niedostępność przy storage errors i naprawa
  przez valid POST. Formularz nie zeruje uszkodzonej kalibracji bez decyzji.
- [x] Sensors form: Voltage/Loop, ADC capture z valid property, potwierdzenie
  kalibracji, zapis/readback; zmiana typu/capture kasuje potwierdzenie.
- [x] Portable hostname codec, default `kinconductor`, osobny `net_cfg`,
  lowercase DNS label 1–63, GET/POST `/api/network`, Settings form,
  setHostname przed Wi-Fi begin, reconnect po odpowiedzi, mDNS HTTP/TCP80,
  stop na disconnect i retry po failure/reconnect.
- [x] UDP: widoczna konfiguracja portu w Interfaces/Settings, opis stałego
  bind `0.0.0.0`, IP/port w statusie; outbound host/port w action parameters.
  Nie dodano pozornego edytora bind address ani static-IP/DNS konfiguratora.
- [x] Naprawa ściśle powiązanych limitów: startup IFTTT JSON 2048→8192;
  `NoMemory` nie nadpisuje NVS. Capabilities pool oparty o target-size slots;
  exact String reservation/serial length check zapobiega niepełnemu JSON.
  Browser HTML hostname pattern uwzględnia Unicode-sets flag `v`.
- [x] Pin oficjalnego pioarduino `55.03.312-1` / Arduino ESP32 3.3.12 /
  IDF5.5.5, bibliotek do przetestowanych wersji; firmware/package 1.1.0.
- [x] Lokalny tag-release workflow dla obu profili, testy + buildfs,
  artefakty i release job wyłącznie po udanych tag-builds; manual run bez release.
  PlatformIO6.2.0, Node22/Python3.12 w CI; native runner obsługuje `PIO_ENV`.
- [x] Node packager: sześć obrazów, offsets/partitions, exact binary-size
  threshold, factory component matching, SHA256/manifest i NVS warnings.
- [x] README, description/scripts/version paczki, wiki rozdziały dla prądu,
  źródeł producenta i release, wszystkie pięć żądanych odnośników KinCony;
  opis torów analogowych/LM258 DA oraz niepotwierdzonego konfliktu RS232.

Walidacja: 32 native groups + BLE/transport, ASan/UBSan; 15 Node tests.
Both profiles + matching LittleFS PASS; actionlint1.7.7 PASS.
Final A6 bin **1 532 032 B**, coprocesor **1 532 192 B**;
OTA slot 1 572 864 B, headroom 40 832/40 672 B; static RAM 75 096 B,
LittleFS 917 504 B. Zależności budować sekwencyjnie z testami, nie zmieniać
`.pio/libdeps` w czasie native compiler run.

Po potwierdzeniu bezpiecznych obciążeń wgrano base bez RF24/coprocesora
i matching LittleFS na `/dev/ttyUSB0`, oba hashe verified, NVS zachowane.
Real KC868-A6: 7 modules/51 properties, CPU2×240MHz,
PSRAM0/flash4MiB. 30 consecutive capability JSON responses PASS;
observed stress minheap21 772 B, nie worst-case guarantee.
Hostname change/canonicalization/reconnect/readback PASS oraz real LAN
mDNS `kinconductor-test.local` resolved to the device; `kinconductor`
was restored.
Calibration invalid payloads rejected bez zmiany record; real browser
ADC capture/Voltage-save/warming_up PASS; wszystkie kanały przywrócono OFF.
Hostname/calibration survive reset PASS. Mobile390px: input38px,
checkbox20×20, brak horizontal overflow/overlap z header.
DOM click/submission smoke, nie pełny pointer/accessibility test.
Fizyczny sensor/pętla i kalibracja metrologiczna/RMS nie były testowane.
Rekomendacja i ograniczenia: [pomiar prądu](./docs/current-measurement.md).
Pełne zadania architektoniczne i pozostałe drivery pozostają otwarte.

### Settings, CSS, BLE, UDP i kontekst LLM — 2026-10-31

- [x] Poprawiono ograniczenia nested grid i kolumn DI; real dashboard przy
  390/860/1000/1200 px bez overflow komórek, paneli i wierszy przekaźników.
- [x] Settings: sieć osobno, sześć akordeonów funkcjonalnych; wszystkie 33
  pola dokładnie raz, kontrolki 38 px, checkbox 20×20 px, sprawdzone zwijanie.
  API i zapis pozostają jednym płaskim rekordem.
- [x] BLE: serializer kopiuje tymczasowy MAC zamiast przechowywać martwy
  wskaźnik; bounded cache 24 urządzeń, typed error/HTTP503, komunikat w UI.
  Native test wszystkich adresów po powrocie; real skany 9–13 urządzeń i
  poprawne karty/selektory w WebGUI.
- [x] UDP per reguła: port lokalny, IPv4/port nadawcy, exact/contains i tekst,
  max 256 bajtów UTF-8; max cztery reguły/listenery, wspólny socket dla tego
  samego portu, bounded pending matches. Osobno globalny UDP i port docelowy
  akcji TX. Wspólna walidacja i HTTP409 dla konfliktów global/mDNS.
- [x] Real UDP: porty wspólne/różne, filtry, powtórzone pakiety i akcje/replies,
  odrzucenia konfliktów bez zmiany rekordu. Formularz IFTTT zapisał i odczytał
  poprawnie RX port oraz TX host/port/data; przywrócono oryginalne reguły.
- [x] Przeniesiono 423 linie zweryfikowanych historycznych przyrostów z aktywnego
  ToDo do tego archiwum; wszystkie niepełne zadania i blokady zostały w planie.
- [x] Dodano [AGENTS.md](./AGENTS.md), [pamięć projektu](./docs/llm-memory.md)
  i [skill peryferiów](./.github/skills/kinconductor-peripheral/SKILL.md).
  Istniejący [workflow](./.github/workflows/release.yml) rozszerzono o branch/PR
  validation; release nadal tylko dla tagów. Actionlint1.7.7 PASS lokalnie.
- [x] Zaktualizowano README, rozdziały wiki i opis package.json bez zmiany
  zależności, partycji lub wersji/tagu poprzedniego release.
- [x] `npm test`: 35 native groups + BLE/transport ASan/UBSan, 19 JS PASS.
  Oba firmware + matching buildfs PASS. Base **1 538 272 B**, coprocessor
  **1 538 432 B**; headroom **34 592 / 34 432 B**, static RAM **76 784 B**.
  LittleFS **917 504 B**. Registry: osiem modułów / 55 właściwości, limit wypełniony.
- [x] Za zgodą wgrano final base i matching LittleFS, hash verified,
  NVS zachowane. Aktualne poprawki są working-tree, nie nowym release v1.1.0.

Ograniczenia: skan BLE to cache wspólnego skanera, nie pełny pairing/test
wejścia-wyjścia z zasięgu. UDP ma bounded processing, nie gwarancję burst delivery.
Browser DOM/layout smoke, nie pełny pointer/accessibility test.
Pomiar prądu/PWM i pełny plan pozostają nieukończone. Publikacja repozytorium
i wykonanie Actions na GitHub nadal zablokowane w tej sesji.

### Wiele zapisanych BLE — v1.1.1

- [x] Do ośmiu MAC w jednym module BLE; każde urządzenie ma własny timestamp,
  `in_range` i quality. Śledzenie niezależne od pełnego cache skanera UI.
- [x] Stabilne ID `ble.presence/<MAC>/in_range` osobno w IO, IFTTT i OLED.
  Nowe selektory pomijają ogólny alias; cztery reguły IFTTT pozostają limitem.
- [x] Zapisany MAC legacy zachowany po reorder; usunięcie go wyłącza alias,
  nie przekierowuje istniejącej reguły na inne urządzenie.
- [x] Version-1 `ble_targets`, walidacja duplikatów/limitów/typów/NUL;
  import poprzedniego `ble_target`, wymagany write/readback przed publikacją.
  Pojedynczy endpoint nie nadpisze listy wieloelementowej: HTTP409.
- [x] WebGUI: dodawanie, lista i usuwanie konkretnego MAC; kontrola 8-entry
  limit i aktualizacja capability selectors nawet bez zmiany registry revision.
- [x] Native ASan/UBSan: osiem równoczesnych stanów, niezależne expiry,
  reorder/removal, legacy alias, restore codec, full-cache i max JSON.
  21 JS tests PASS; oba profile i matching LittleFS build PASS.
- [x] Synthetic browser preview: osiem kart przy 390/860/1200 px,
  no overflow i widoczne przyciski. To nie test firmware/hardware.
- [x] README/wiki/memory/package/firmware version zaktualizowane do 1.1.1.

Base **1 543 376 B**, coprocessor **1 543 552 B**, static RAM **78 448 B**;
OTA headroom **29 488 / 29 312 B**. Osiem modułów, max 63 properties.
Nie wgrano firmware, nie opublikowano repo/tagu na GitHub. Wielourządzeniowy
test fizycznej obecności i NVS po zaniku zasilania pozostają do wykonania.

### WebGUI/RF24 i publikacja z taga — v1.1.2

- [x] RF24 monitor/diagnostics/rule options domyślnie ukryte; jawne compiled
  support wymagane. Missing/false/invalid/API failure usuwa stale radio UI.
  Status feature flag i packet API używają wspólnego helpera.
- [x] Pełne zakresy sampling/OLED page/beeper zamiast presetów pomijających
  poprawne NVS wartości. Wymagane pola liczbowe, named schema mode/type/pin
  choices, poprawna nazwa DHT11 w telemetry.
- [x] Grid: style inputów, lokalizowane etykiety, odrzucanie pustych pozycji,
  aktualizacja max po zmianie wymiarów. Controls/grid/cards bez overflow.
- [x] IO/IFTTT data textarea dla multiline LF, UTF-8 max256; pusty próg IFTTT
  i invalid RF payload odrzucane z komunikatem.
- [x] Istniejący Actions workflow reaguje na push `v*`; dodano early version
  check/fetch tags, serializację ref runs, draft/assets/publish z możliwością
  retry oraz test błędów innych niż HTTP404.
- [x] Native ASan/UBSan + 26 JS tests PASS, oba firmware/buildfs PASS,
  actionlint1.7.7 PASS. Synthetic browser wszystkie osiem tabów przy
  390/860/1200px; input min38px, RF24 niewidoczne, no-op save zachowuje
  37s/2750Hz/750ms, grid resize/empty i multiline LF sprawdzone.
- [x] README/wiki/memory/package/firmware version zaktualizowane do 1.1.2.

Image sizes/headroom/RAM nie zmieniły się od v1.1.1. Nie wykonano uploadu
na hardware, push do GitHub ani rzeczywistego Actions run. Szerszy plan
architektury i fizyczne kryteria akceptacji pozostają otwarte.

### Profile identity and runtime headroom — 2026-10-31

- [x] Firmware carries `KC868_TARGET_MODEL`; dashboard title, model badge and
  generated SVG favicon reflect the selected Ax build.
- [x] Diagnostics includes `pinoutValidated`; non-A6 aliases warn that the
  displayed map is the A6 baseline. Separate Ax electrical pin/IO profiles
  remain **not implemented** pending model/revision schematics and validation.
- [x] Hardware summary reports remaining capacity in the running OTA app
  partition explicitly, alongside firmware size and total flash.
- [x] ESP-IDF FreeRTOS idle runtime counters report per-core CPU utilization
  when enabled by the target build; native UI test covers the displayed shape.

### Wiki screenshots and installation guide — 2026-10-31

- [x] Added [WebGUI guide](./docs/webgui.md) with live screenshots for
  Dashboard, IO, Sensors, Interfaces, IFTTT, Screen, Settings and Diagnostics.
  Captures came from the reachable KC868-A6 instance running firmware 1.1.0;
  status/configuration/capabilities endpoints returned HTTP 200.
- [x] Redacted network name, local IP, BLE MAC and UDP port in screenshots.
  Hardware and product reference images were added with source/revision caveats.
- [x] Expanded the hardware chapter with manufacturer images and explicit
  warnings about conflicting product-sheet/diagram analog and relay claims.
- [x] Updated the first-start flash commands to select the A6 profile and upload
  firmware and LittleFS separately. No flash/upload was performed for this docs task.
- [x] Re-captured all eight live WebGUI screenshots with the **EN** language
  selected after the initial Polish captures. Screenshot-only event translations
  and redactions were applied locally; device configuration was not changed.
  Verified all eight images show English page navigation and labels; SSID,
  local IP, BLE MAC and UDP port remain hidden. IFTTT shows the three saved
  rules present at capture time.

### English README and wiki default — 2026-10-31

- [x] Translated the live WebGUI wiki chapter into English and verified its
  screenshots, hardware instructions, warnings and cross-links.
- [x] Confirmed the top-level README, documentation index and existing build,
  hardware, release, peripheral, first-start and architecture chapters are
  written in English.
- [x] Set English as the default for repository documentation in `AGENTS.md`,
  the README and documentation index. Keep identifiers, API fields, commands
  and quoted source text unchanged unless localization is explicitly requested.

### Release preparation — v1.1.3

- [x] Package and firmware version metadata advanced together from 1.1.2 to
  1.1.3 for the selected KC868-Ax build identity and diagnostics changes.
- [x] Removed the live controller's private LAN IP from tracked documentation.
- [x] `npm test` passed; both maintained firmware profiles and both matching
  LittleFS images built successfully with the 1.1.3 metadata.
- [ ] Create and push the immutable `v1.1.3` tag; verify the remote tag-triggered
  GitHub Actions build and release before claiming publication succeeded.

### Serial profiles, Modbus RTU and Vaillant eBUS — 2026-11-01

- [x] Created branch `feature/serial-protocols-ebus-modbus`; persisted raw/
  Modbus RTU selection, baud, data/parity/stop framing, UART2 P17 route and
  opt-in eBUS writes in settings record v3. Existing v1/v2 records remain
  readable with safe defaults.
- [x] Modbus RTU actions use the callback-based `modbus-esp8266` 4.1.1 tag,
  one transaction at a time, bounded reads (16 registers), response snapshots,
  raw-serial exclusion and registry-driven IFTTT/UI actions.
- [x] eBUS actions use named `ebusd` command TCP on port 8888, with bounded
  response polling, `last/value`, `last/text`, sequence/connection state and
  IFTTT properties. Adapter enhanced port 9999, micro-ebusd HTTP, direct eBUS
  serial and ebusd authentication are not implemented. Boiler writes default
  OFF and require explicit opt-in.
- [x] `npm test` passed, including ASan/UBSan native suites and 30 Node tests.
  `pio run -e kc868-a6 -e kc868-a6-coprocessor` and matching `buildfs` for both
  profiles passed. Maximum eight-module capabilities JSON fits the tested
  1050-slot + 8192-byte pool. This is a host/build result, not a heap test on A6.
- [ ] Confirm VU/VUW PL 200/3-5 product/HW/SW IDs with `ebusd scan`/`info`;
  test the v6 adapter, external ebusd host, Modbus equipment, RS-485 direction,
  P17 electrical/strap behavior, Wi-Fi recovery and boiler writes physically.
  No firmware or filesystem upload was performed.
