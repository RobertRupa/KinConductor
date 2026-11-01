# KC868-A6 — specyfikacja refaktoryzacji i rozbudowy

Status: realizacja rozpoczęta — kontrakty/rejestr, ADC, konfiguracja,
profil płytki i diagnostyka zasobów;
pozostały zakres planowany. Szczegóły postępu w rozdziale 13.

Dokumentacja użytkowa: [indeks rozdziałów](./docs/README.md).
Aktualne instrukcje build/upload i wynik wdrożenia:
[build i wdrożenie](./docs/build.md). Historyczne macierze poniżej zachowują
nazwy profili obowiązujące podczas pomiaru; nie są aktualnymi komendami build.
Data analizy: 2026-10-31.

Odbiorcy: agenci LLM implementujący kolejne zadania oraz osoba zatwierdzająca sprzęt i zachowanie urządzenia.

## 1. Cel i zasady realizacji

Zbudować modularny firmware, w którym dodanie peryferium nie wymaga dopisywania jego nazwy do silnika IFTTT, centralnego generatora statusu i formularzy WebGUI. Analogicznie oddzielić sterownik OLED od stron i widgetów, a powłokę WebGUI od konkretnych urządzeń.

Wymagane rezultaty:

- Wspólny interfejs cyklu życia peryferiów i usług komunikacyjnych.
- Osobne, opcjonalne interfejsy źródeł/wyzwalaczy i akcji IFTTT.
- Rejestr możliwości jako źródło danych dla API, WebGUI, IFTTT i OLED.
- Dynamiczne strony i widgety WebGUI zależne od rzeczywiście dostępnych możliwości.
- Zakładki: Dashboard, IO, IFTTT, Sensors, Interfaces, Bluetooth, LoRa, Screen, Diagnostic, Settings.
- Nowe urządzenia: WS2812, DHT11, SSD1309, SH1107, sterowanie PWM wentylatora.
- UDP, TCP, MQTT, API HTTP, DAC, RS-232 i RS-485 jako pełnoprawne moduły dostępne również w IFTTT.
- Wybór przechowywania stanów przekaźników w RAM albo NVS, z jednoznaczną polityką startu po utracie zasilania.
- Diagnostyka z mapą zasobów KC868-A6, diagramem połączeń logicznych i informacjami o konfliktach.
- Zachowanie obecnych funkcji i migracja istniejącej konfiguracji.

Rozszerzenie zakresu z 2026-10-31: ocena SIM7600/GNSS, Meshtastic, Zigbee,
Thread, audio, CC1101, IR TX/RX i IrDA; synchronizacja NTP, harmonogram IFTTT
oraz budżety sprzętowe i strategia wielordzeniowa opisane w rozdziałach 16–20.
Ten rozszerzony zakres ma osobne profile i zadania; nie oznacza obietnicy
uruchomienia wszystkich stosów jednocześnie na obecnej płytce.

Uściślenie zakresu: P17 może być sprzętowym UART przez GPIO Matrix, z
docelowym TX=GPIO12/RX=GPIO13 dla koprocesora. Profile H2/C5/C6, własny
mostek i optymalizacja dla 1–2 aktywnych rozszerzeń są opisane w rozdziale 21.
To zastępuje założenie, że wszystkie wymienione rozszerzenia muszą pracować
jednocześnie. Wiele urządzeń I2C pozostaje osobnym budżetem magistrali.

Nazwy `Screen` i `Settings` porządkują pisownię „Screan” i „Setting” z wymagania. Polskie etykiety UI mogą brzmieć „Ekran” i „Ustawienia”. IFTTT oznacza tutaj lokalny silnik reguł, nie integrację z usługą chmurową ifttt.com.

**Nie realizować całej listy jednym dużym patchem.** Najpierw ekstrakcja z zachowaniem zachowania, następnie migracja kontraktów, na końcu nowe sterowniki. Zmiany zachowania wskazane poniżej muszą mieć osobne testy i opis.

## 2. Stan obecny — punkty zaczepienia w kodzie

| Obszar | Istniejące pliki / symbole | Wnioski do refaktoryzacji |
| --- | --- | --- |
| Główna aplikacja | [src/main.cpp](./src/main.cpp): `setup`, `loop`, globalne obiekty i flagi `*Ready` | Jeden plik łączy sprzęt, stan, automatyzację, JSON, OLED i logi. |
| IO | [src/main.cpp](./src/main.cpp): `initializeRelays`, `setRelay`, `initializeInputs`, `reportInputsAndSensors` | 6 przekaźników, 8 DI, 4 AI. Przekaźniki startują OFF, brak utrwalania ich stanów. |
| Konfiguracja | [include/config/device_settings.h](./include/config/device_settings.h), [include/config/config_service.h](./include/config/config_service.h), [src/config/preferences_settings_store.cpp](./src/config/preferences_settings_store.cpp); [src/wifi_web_ui.cpp](./src/wifi_web_ui.cpp): `prepare`, `handleSettings` | Model i zapis ustawień sprzętu wyodrębnione; callback apply, Wi-Fi/auth/rules/RF24 nadal legacy. Namespace NVS: `kc868a6`; wersjonowany `device_cfg`. |
| IFTTT | [src/main.cpp](./src/main.cpp): `iftttValue`, `updateIftttRules`, `evaluateIftttRules`; [src/wifi_web_ui.cpp](./src/wifi_web_ui.cpp): `handleIfttt` | Up to eight rules with bounded composite actions; current sources/actions still use branches and whitelists. Rule JSON is parsed during evaluation. |
| OLED | [src/main.cpp](./src/main.cpp): `initializeDisplay`, `setDisplayContrast`, `renderOled` | SSD1306 128×64. Strony status/sensors/air oraz siatka własna; logika zależna od konkretnych globalnych pomiarów. |
| HTTP | [include/wifi_web_ui.h](./include/wifi_web_ui.h): `WifiWebUi`; [src/wifi_web_ui.cpp](./src/wifi_web_ui.cpp): `startServer` | Zestaw wyspecjalizowanych callbacków i endpointów, które rosną z każdym urządzeniem. |
| WebGUI | [data/index.html](./data/index.html), [data/app.js](./data/app.js), [data/app.css](./data/app.css) | Zakładki overview/sensors/interfaces/diagnostics/automation. Lista urządzeń częściowo dynamiczna; formularze i IFTTT w dużej części statyczne. PL/EN. |
| Status | [src/main.cpp](./src/main.cpp): `deviceStatusJson` | Centralny JSON zna urządzenia i ustawienia. Należy rozdzielić wykrycie, włączenie, gotowość i poprawność pomiaru. |
| Radio / BLE | [src/main.cpp](./src/main.cpp): `sendNrf24Packet`, `radioMonitorJson`, `configureBluetooth`, `scanBluetoothDevices` | RF24 ma historię 16 pakietów i zapisane ramki NVS; log zdarzeń ma 24 pozycje. BLE scan/beacon są rozłączne. |
| Pinout | [include/kc868_a6_pins.h](./include/kc868_a6_pins.h), [README.md](./README.md) | Baza dla mapy zasobów, nie zastępstwo weryfikacji rewizji PCB. |
| Build | [platformio.ini](./platformio.ini), [partitions.csv](./partitions.csv) | Arduino/ESP32, C++17, ArduinoJson 6, LittleFS, profile base/LoRa/nRF24/GPS/coprocessor. |

### 2.1. Zachowanie, które trzeba zachować podczas ekstrakcji

- Logiczne `true` oznacza przekaźnik ON lub aktywne DI, mimo active-low na PCF8574.
- Przekaźniki P0–P5; P6–P7 nie są kanałami użytkownika i mają pozostać w stanie zarezerwowanym.
- ADC ma zakres surowy 0–4095. Nie interpretować go automatycznie jako napięcia na złączu.
- Typy sensorów w istniejącym NVS: `0=OFF`, `1=DS18B20`, `2=DHT22`.
- DHT22 nie jest odczytywany częściej niż co 2 s.
- Dotychczasowy IFTTT wykonuje akcję przy przejściu warunku false → true, nie przy każdym odświeżeniu.
- RF24: 32-bitowy payload, kanał 76, 1 Mbps, pipe `E8E8F0F0E1`; nie zamieniać go niejawnie na dowolny protokół.
- OLED: dotychczasowe ustawienia jasności, obrotu, wielkości tekstu, automatycznego przełączania i maski pól.
- Wi-Fi provisioning, mDNS, PL/EN, sterowanie ręczne, log zdarzeń, RF24 replay i zapisane ramki.
- Obecny build domyślny to `kc868-a6` według konfiguracji PlatformIO. README opisuje jako domyślny nRF24 — rozbieżność wyjaśnić, nie przenosić jej do nowej architektury.

### 2.2. Problemy ściśle związane z refaktoryzacją

Uwzględnić w testach migrowanego obszaru, zamiast kopiować jako nowe kontrakty:

- W `deviceStatusJson` elementy `analogReadings` są iterowane jako `uint8_t`, mimo przechowywania `uint16_t`. Nowy kontrakt musi zachować pełne 12 bitów.
- Występuje utożsamianie `connected`/`detected` z konfiguracją lub ostatnią poprawną wartością. Wprowadzić niezależne statusy.
- Zbiorcze źródło `humidity` wybiera pomiar według kolejności dostępności. Migracja ma zachować ten alias, ale nowe reguły muszą wybierać jawne źródło.
- Niektóre operacje pomiarowe, NFC, skanowania i łączenia sieci mogą blokować. Zmierzyć rzeczywiste czasy i oddzielić je od szybkiej ścieżki IO/IFTTT.
- Włączenie niektórych urządzeń nie odpowiada ich inicjalizacji wprost. Nowy lifecycle musi respektować `enabled`.
- Akcja OLED w starym IFTTT obsługuje strony 1–3, choć konfiguracja zna również stronę 4. Ekstrakcja zachowuje stary zakres; nowy kontrakt wybiera istniejące strony przez ID.

## 3. Docelowa architektura

```text
                         App / composition root
                                  |
            +---------------------+----------------------+
            |                     |                      |
      ConfigService       ResourceManager          Scheduler
      StateStore          BoardProfile             EventBus
            |                     |                      |
            +---------------------+----------------------+
                                  |
                         PeripheralRegistry
                                  |
            +---------------------+----------------------+
            |                     |                      |
       sterowniki IO         sensory / OLED        komunikacja
       Relay/DI/ADC          DHT/DS/BME/...         BLE/LoRa/RF24
       DAC/PWM/WS2812        SSD/SH...              UART/UDP/TCP/MQTT/HTTP
                                  |
                         CapabilityRegistry
                properties / triggers / actions / config
                      /           |            \
                IftttEngine    ApiRouter     TelemetryService
                                  |
                             WebGUI / OLED
                        strony, widgety, read models
```

Zależności:

1. Rdzeń nie zależy od `WebServer`, `Preferences`, bibliotek sensorów ani DOM.
2. Sterownik zna swój hardware, zasoby i kontrakty rdzenia, ale nie zna HTML ani reguł.
3. Silnik IFTTT zna zarejestrowane źródła i akcje, nie klasy `DHT`, `RF24` lub `Relay`.
4. API mapuje transport HTTP na wspólne usługi; nie zapisuje sprzętu bezpośrednio.
5. OLED i WebGUI czytają ten sam model możliwości/telemetrii. Nie muszą mieć identycznych widgetów.
6. Jedno wywołanie akcji obsługuje zarówno przycisk w WebGUI, polecenie API, jak i IFTTT.
7. Magistrale i zasoby sprzętowe mają jednego właściciela/koordynatora; sterowniki nie wykonują niezależnego `Wire.begin`, `SPI.begin` czy `Serial1.end`.

### 3.1. Proponowany układ plików

Nazwy poniżej są docelowe, nie opisują plików już istniejących. Dopuszczalne jest scalanie małych implementacji, ale nie odtwarzanie monolitu.

```text
include/core/        kontrakty, wartości, statusy, błędy, deskryptory
src/core/            rejestry, scheduler, zdarzenia, zasoby
include/board/       profil KC868-A6 i opis zasobów
src/board/           rezerwacje i walidacja konfiguracji sprzętowej
include/config/      modele i wersjonowanie konfiguracji
src/config/          adapter Preferences/NVS, migracje, StateStore
include/peripherals/ kontrakty publiczne wymagane przez composition root
src/peripherals/     IO, sensors, outputs, display, radio, serial
src/services/        sieć, UDP, TCP, MQTT, HTTP API/client
src/automation/      model reguły, kompilator, evaluator
src/ui/oled/         manager stron, canvas adapter, widgety
src/web/             API router, compatibility endpoints, assets
data/js/core/        API client, router, capability store, i18n
data/js/pages/       moduły stron WebGUI
data/js/widgets/     widgety i formularze ze schematów
test/                testy rdzenia, migracji i kontraktów
src/main.cpp         wyłącznie uruchomienie App i App.tick()
```

Nie dodawać osobnego katalogu/class wrappera dla każdego pola pomiaru. Jedno peryferium może publikować wiele właściwości i kanałów.

## 4. Kontrakty interfejsów i rejestracja możliwości

### 4.1. Interfejsy firmware

Poniższa tabela jest kontraktem semantycznym. Konkretne sygnatury i typy należy ustalić w zadaniu CORE-01 i potem stosować konsekwentnie.

| Interfejs | Odpowiedzialność | Minimalny kontrakt |
| --- | --- | --- |
| `IPeripheral` | Cykl życia urządzenia lub usługi | Stabilne `id`, deskryptor, `begin(context)`, `tick(nowMs)`, `stop()`, `health()`. |
| `IPropertyProvider` | Odczyty i bieżący stan | Lista właściwości i odczyt snapshotu z typem, jednostką, jakością i czasem aktualizacji. |
| `ITriggerProvider` | Źródła IFTTT | Lista deskryptorów źródeł: property/condition oraz discrete event; publikacja zdarzeń przez EventBus. |
| `IActionProvider` | Wykonywanie akcji | Lista akcji, schemat parametrów, walidacja, `execute`; wynik synchroniczny albo przyjęcie do kolejki z ID operacji. |
| `IConfigurable` | Konfiguracja modułu | Schemat, odczyt, walidacja, klasyfikacja hot-apply/restart, przygotowanie i zastosowanie zmiany. |
| `IDisplayDriver` | Fizyczny ekran | Lifecycle oraz abstrakcyjny canvas, rozmiar, obrót, kontrast, power, commit klatki. |
| `IOledPage` | Strona logiczna OLED | ID, wymagane możliwości, render read modelu na canvasie, brak bezpośredniego I2C. |
| `IOledWidget` | Fragment strony OLED | Wymagania danych, rozmiar/layout, render do przydzielonego prostokąta. |

Zasady implementacji:

- Każde peryferium implementuje `IPeripheral`.
- Dodaje tylko te interfejsy IFTTT, które rzeczywiście realizuje. Sensor pomiarowy nie musi udawać urządzenia z akcjami. Transport może implementować oba; przekaźnik może publikować zdarzenia zmiany i akcje sterujące.
- Nie tworzyć jednej klasy bazowej z dziesiątkami pustych metod.
- Nie wymagać RTTI i `dynamic_cast`. Composition root/fabryka jawnie rejestruje dostępne wskaźniki do interfejsów lub tablice bindingów.
- Rejestry nie posiadają obiektów sterowników. App jest właścicielem instancji; binding nie może przeżyć instancji.
- Wirtualne destruktory tam, gdzie kontrakt pozwala usuwać obiekt polimorficznie. Obiekty sprzętowe tworzyć na starcie, nie w każdej iteracji.
- ID, typy, jednostki i schematy są definiowane w module raz. Metadata API, IFTTT i UI powstaje z tego samego deskryptora.
- Nie przekazywać luźnego JSON przez cały rdzeń. API i NVS tłumaczą JSON na sprawdzone struktury; evaluator pracuje na typowanym modelu.

### 4.2. Deskryptor peryferium

Wymagane pola:

- `id`: stabilne ID instancji, np. `relay-bank.main`, `sensor.gpio32`, `display.main`, `mqtt.main`.
- `type`: stabilny typ sterownika, np. `dht22`, `sh1107`, `mqtt`.
- `labelKey` i opcjonalna nazwa nadana przez użytkownika.
- `category` i sugerowana strona: IO, sensors, interfaces, bluetooth, lora, screen.
- `compiled`: czy typ dostępny w bieżącym buildzie.
- `enabled`, `configured`, `detected`, `ready`, `health` i ostatni błąd jako niezależne informacje.
- Powiązania zasobów: GPIO, magistrala, adres I2C, CS, UART, LEDC/RMT, socket.
- Właściwości, wyzwalacze, akcje, konfiguracja i preferowane rodzaje widgetów.

`detected` ma wartość `unknown`, jeśli protokół nie umożliwia wiarygodnej detekcji. Sam ACK I2C nie identyfikuje kontrolera OLED. Brak pomiaru DHT nie znaczy automatycznie brak fizycznego sensora.

### 4.3. Typy i status wartości

- Typy podstawowe: bool, signed/unsigned integer, finite number, string, enum, bounded bytes i struktury z jawnie opisanymi polami.
- Jednostka jest częścią deskryptora, np. `degC`, `%RH`, `ppm`, `ppb`, `raw_adc`, `raw_dac`, `%`, `dBm`.
- Status pomiaru: `valid`, `stale`, `unavailable`, `error`, `disabled`, `warming_up`.
- Odczyt zawiera `updatedAtMs`, `ageMs`, opcjonalnie `errorCode`. Dla daty absolutnej osobne pole z flagą synchronizacji zegara.
- W API niepoprawna wartość to `null` wraz ze statusem, nigdy pozorne zero. JSON nie może zawierać `NaN` ani `Infinity`.
- Błąd po ostatnim poprawnym odczycie nie może zostawiać go jako bezterminowo `valid`. Ustalić TTL per właściwość.
- Dla wyjść oddzielić wartość żądaną, zastosowaną i potwierdzoną. Odczyt PCF8574 nie potwierdza fizycznego zwarcia styków przekaźnika.

### 4.4. Rejestr możliwości

Przykładowe identyfikatory:

```text
relay-bank.main/channel/1/state
relay-bank.main/action/set
sensor.gpio32/property/temperature
sensor.gpio32/property/humidity
sensor.gpio32/trigger/reading
ws2812.main/action/setColor
pwm-fan.main/action/setDuty
mqtt.main/trigger/messageReceived
rs485.main/action/sendFrame
display.main/action/selectPage
```

Reguły:

- ID nie zależy od indeksu w wektorze, kolejności wykrycia, języka ani nazwy użytkownika.
- W jednej wersji schematu nie zmieniać znaczenia istniejącego ID. Model sensora na GPIO32 może się zmienić, ale reguła wilgotności musi zostać zawieszona, jeśli nowy sensor nie ma tej właściwości.
- Duplikat ID jest błędem startu modułu z wpisem diagnostycznym, nie nadpisaniem.
- Rejestr aktualizuje `capabilityRevision` po zmianie zestawu możliwości. Health i telemetria mają osobne rewizje/znaczniki.
- Typy skompilowane, ale niewłączone, są widoczne w katalogu konfiguracji; nie otrzymują aktywnych akcji.
- Niedostępna akcja może być pokazana jako zablokowana w istniejącej regule. Nie wolno usuwać reguły ani podmieniać celu.
- Nowy moduł ze standardowymi właściwościami i akcjami ma działać bez edycji evaluatorów, API routera i ogólnego kreatora IFTTT.

## 5. Runtime, błędy i zarządzanie zasobami

### 5.1. Scheduler i EventBus

- Zachować prosty cooperative scheduler, dopóki pomiary nie wykażą konieczności dedykowanego taska.
- Oddzielne harmonogramy: DI/S2, sensory, radio/serial/network, IFTTT, OLED i telemetria HTTP.
- `tick` ma ograniczoną pracę; żadnej pętli odbioru bez limitu bajtów/ramek na iterację.
- DS18B20: request conversion → oczekiwanie wymagane przez rozdzielczość → read; nie czytać poprzedniej konwersji jako nowej.
- DHT22 minimum 2000 ms; dla DHT11 konserwatywnie również 2000 ms, chyba że zadanie sterownika uzasadni zmianę.
- BLE scan, NFC, DNS, TCP connect, MQTT reconnect i HTTP request nie mogą blokować obsługi przekaźników przez pełny timeout transportu.
- Czas `millis()` obsługiwać odejmowaniem unsigned; przepełnienie objąć testem.
- EventBus ma ograniczoną kolejkę, monotoniczne ID zdarzenia i licznik przepełnienia. Odrzucenie musi być widoczne w Diagnostic.
- Callbacki bibliotek/ISR nie wykonują IFTTT, renderowania ani NVS; kopiują ograniczone dane do bezpiecznej kolejki.

### 5.2. ResourceManager / BoardProfile

Rezerwować i walidować:

- GPIO: wejście/wyjście, input-only, strapping, stały obwód płytki, zewnętrzny adapter.
- I2C: para SDA/SCL, prędkość, urządzenie/adres, timeout i błędy.
- SPI: wspólna magistrala, osobne CS, zgodność transakcji i fizyczne konflikty.
- UART: jeden właściciel kontrolera i strumienia. UART1 domyślnie RS-232,
  UART2 domyślnie RS-485; jeden z nich można przepiąć na P17 przez GPIO
  Matrix, wyłączając jego poprzednią rolę. UART0 pozostaje USB/log.
- GPIO25/26: DAC, beeper PWM i potencjalne inne funkcje są rozłączne.
- LEDC: timer/częstotliwość/rozdzielczość; RMT dla WS2812 zależnie od wybranej biblioteki i wersji core.
- Sockety, bufory i ograniczenia pamięci usług.

Konflikt ma odrzucić konfigurację przed dotknięciem sprzętu; API zwraca szczegóły obu właścicieli, a UI wskazuje kolizję. Nie oferować wszystkich GPIO jako „wolnych”.

Zmiany konfiguracji: validate schema → validate resources → prepare → persist candidate → apply. Gdy apply zawiedzie, spróbować przywrócić poprzedni stan; wynik i stan recovery są jawne. Dla zmian wymagających restartu zachować konfigurację aktywną i oczekującą osobno. Nie deklarować transakcyjności, której hardware/NVS nie zapewnia.

### 5.3. Wspólny model błędów

Minimalne kody: `invalid_argument`, `not_supported`, `disabled`, `unavailable`, `resource_conflict`, `timeout`, `queue_full`, `storage_unavailable`, `storage_write_failed`, `authentication_required`, `operation_failed`.

Wynik zawiera kod, kontekst, ID operacji/urządzenia i rozróżnienie `accepted` / `applied` / `confirmed`. Nie zwracać HTTP 200 oznaczającego sukces, jeśli akcja nie została wykonana.

Logi strukturalne: czas, severity, moduł, kod, komunikat lokalizowalny lub argumenty, korelacja rule/event/operation. Nie logować haseł Wi-Fi, MQTT ani tokenów.

## 6. IFTTT — model niezależny od urządzeń

### 6.1. Źródła i wyzwalanie

Dwa tryby:

1. **Condition**: wartość właściwości + operator + próg; domyślnie akcja na false → true.
2. **Event**: zdarzenie dyskretne, np. ramka UDP, pakiet LoRa, UID NFC, wiadomość MQTT, webhook.

Porównania wynikają z typu: liczby mają operatory porównania i zakres; bool ma równość/zmianę; enum ma wybór wartości; tekst ma równość/prefiks; bytes mają jawny wzorzec. Nie stosować rzutowania każdej ramki do `double`.

Dla condition dodać opcjonalną histerezę, debounce/`holdMs` i cooldown. Domyślne zero zachowuje obecne zachowanie. Pierwszy poprawny odczyt spełniający warunek po starcie może wyzwolić regułę, jak dziś. `stale/error` nie wyzwala i nie resetuje automatycznie historii warunku, aby odzyskanie sensora nie powodowało przypadkowej ponownej akcji.

Dla event każde nowe ID zdarzenia jest osobnym wyzwoleniem, także gdy payload jest identyczny. Eliminować ponowne przetworzenie tego samego ID, nie wszystkich równych danych.

### 6.2. Reguły i akcje

- Stabilne `ruleId`, nazwa, `enabled`, źródło, warunek/filtr, lista akcji, polityka cooldown.
- W pierwszej wersji zachować limit 4 reguł; opublikować go w API. Docelowe zwiększenie wymaga pomiaru pamięci, nie tylko zmiany stałej.
- Nowa reguła może mieć 1–4 akcje wykonywane po kolei; kolejność i limit publikowane przez API.
- Domyślna polityka: zatrzymaj kolejne akcje po błędzie i zarejestruj częściowe wykonanie. Nie cofać fizycznych skutków już wykonanych akcji.
- Przy asynchronicznej akcji kontynuować po jej zakończeniu albo timeout; `accepted` nie jest zakończeniem.
- Kompilować/wiązać reguły przy zapisie i zmianie rejestru; nie parsować JSON w każdej iteracji.
- Nieznane źródło, niedostępny target albo niezgodne parametry: jawny błąd walidacji; istniejąca reguła po zmianie sprzętu ma status suspended.
- Przenosić korelację zdarzenia do akcji; ograniczyć cykle/feedback. Nie uruchamiać rekurencyjnie evaluatorów. Limit kolejki i cooldown chronią przed lawiną, bez obietnicy wykrycia wszystkich cykli sieciowych.
- Nie wprowadzać interpreterów JS, `eval`, dowolnych wyrażeń C++ ani skryptów z konfiguracji.

### 6.3. Przykładowy model reguły v2

```json
{
  "schemaVersion": 2,
  "id": "rule.fan-hot",
  "name": "Cooling",
  "enabled": true,
  "trigger": {
    "kind": "condition",
    "sourceId": "sensor.gpio32/property/temperature",
    "operator": ">=",
    "value": 30,
    "hysteresis": 2,
    "holdMs": 0
  },
  "actions": [
    {
      "actionId": "pwm-fan.main/action/setDuty",
      "parameters": { "dutyPercent": 80 }
    }
  ],
  "cooldownMs": 1000,
  "onError": "stop"
}
```

To przykład kontraktu, nie gotowy wpis działający na obecnym firmware. Histereza dla `>=30` oznacza powrót do false poniżej 28; analogiczne granice innych operatorów muszą mieć testy.

### 6.4. Migracja istniejących reguł

Mapowanie:

| Stare pole / źródło | Docelowo |
| --- | --- |
| `temperature1`, `temperature2` | Właściwość temperature `sensor.gpio32` / `sensor.gpio33`. |
| `bmeTemperature`, `airQuality` | Jawne właściwości BME680 / ENS160. |
| `humidity` | Jawny compatibility provider odtwarzający obecną kolejność wyboru; UI zaleca wybór sensora, nie przepisuje progu. |
| `digital1`…`digital8`, `analog1`…`analog4` | Właściwości kanałów głównych banków IO. |
| `buttonS2` | Logiczny stan przycisku S2. |
| `rf24Packet` | Compatibility property ostatniego uint32; nowy receive event jest oddzielnym źródłem. |
| `relay` + `relay`/`on` | `relay-bank.main/action/set`, numer kanału 1–6 i bool. |
| `beeper` | Akcja test/play z zachowaniem obecnych parametrów. |
| `oledPage` + 1/2/3 | `display.main/action/selectPage`, ID status/sensors/air. |
| `rf24Send` | Akcja wysyłania uint32. |
| `rf24Saved` | Akcja replay ze stabilnym ID zapisanej ramki; zachować i zweryfikować zapisany payload. |

Stare endpointy pozostają adapterami przez okres migracji. Nowa reguła niemożliwa do przedstawienia w starym formacie ma jawny komunikat incompatibility, nie zostaje spłaszczona z utratą akcji.

## 7. WebGUI i API zależne od możliwości

### 7.1. Warstwa API

Wprowadzić wersjonowane `/api/v2/...`; istniejące endpointy delegują do tych samych usług:

| Endpoint v2 | Zawartość |
| --- | --- |
| `GET /api/v2/capabilities` | Wersje schematów, rewizja, aktywne instancje, właściwości, wyzwalacze, akcje i schematy konfiguracji. |
| `GET /api/v2/catalog` | Skompilowane typy dostępne do konfiguracji, ograniczenia i wymagane zasoby. |
| `GET /api/v2/state` | Snapshot wartości/health z ID i jakością; filtrowanie po instancji/stronie. |
| `GET /api/v2/config` | Ustawienia publiczne, wersja, konfiguracja aktywna i oczekująca. Sekrety tylko jako `configured`. |
| `PATCH /api/v2/config` | Walidowana zmiana; konflikt rewizji, błędy pól i zasobów, potrzeba restartu. |
| `POST /api/v2/actions` | `actionId`, parametry, client request ID; wspólny dispatcher. |
| `GET /api/v2/operations/{id}` | Stan przyjętej operacji, zakończenie/błąd, TTL historii. |
| `GET/PUT /api/v2/rules` | Typowany model reguł, wersja i status wiązania. |
| `GET /api/v2/diagnostics` | Zasoby, błędy, reset reason, uptime, heap, czasy loop i kolejki; bez sekretów. |
| `GET /api/v2/board` | BoardProfile, rewizja PCB/poziom potwierdzenia, pinout i konflikty. |
| `GET /api/v2/events` | Stronicowana historia od ID zdarzenia; utrata zakresu jawnie sygnalizowana. |

Standardowa odpowiedź błędu:

```json
{
  "error": {
    "code": "resource_conflict",
    "messageKey": "error.resourceConflict",
    "details": {
      "resource": "gpio26",
      "owners": ["dac.main/channel/1", "beeper.main"]
    }
  }
}
```

Statusy HTTP: 400 błędne typy/JSON, 401 auth, 404 nieznane ID, 409 konflikt/niedostępna funkcja lub rewizja, 413 zbyt duży payload, 422 niepoprawna wartość schematu, 503 niedostępny sprzęt/usługa, 500 błąd wykonania/zapisu, 202 dla operacji oczekującej.

Ograniczyć rozmiary requestów, liczbę parametrów i czas obsługi. Deskryptory nie mogą wymuszać dowolnego kodu lub HTML w przeglądarce. UI używa whitelisty komponentów i bezpiecznego `textContent`.

### 7.2. Dynamiczna nawigacja

Powłoka WebGUI:

1. Pobiera katalog i rejestr możliwości.
2. Buduje nawigację z lokalnego rejestru stron i ich `supports(capabilities)`.
3. Montuje widgety dla właściwości, akcji i konfiguracji.
4. Odpytuje tylko potrzebne snapshoty; polling stanów oddzielony od metadanych.
5. Reaguje na zmianę `capabilityRevision`; nie resetuje niezapisanych formularzy bez ostrzeżenia.

Stałe strony: Dashboard, IFTTT, Interfaces, Diagnostic, Settings. IO i Sensors zależą od właściwych instancji. Bluetooth, LoRa i Screen występują jako strony funkcjonalne, gdy są dostępne; skompilowany, niewłączony moduł konfiguruje się przez Interfaces/catalog. Wykryty wcześniej moduł z błędem pozostaje widoczny ze statusem, a nie znika.

Rozróżnić: „nieobsługiwane w tym buildzie”, „wyłączone”, „skonfigurowane, brak odpowiedzi”, „gotowe”, „niepoprawny pomiar”. Dynamiczne UI nie oznacza ukrywania błędów.

### 7.3. Zawartość stron

| Strona / route | Zakres |
| --- | --- |
| Dashboard `/dashboard` | Podsumowanie health, aktywne wyjścia, wybrane pomiary, sieć i ostatnie zdarzenia. Karty powstają z właściwości, nie sztywnych nazw sensorów. |
| IO `/io` | Przekaźniki, DI/S2, ADC, DAC, PWM/fan, WS2812; sterowanie ręczne, wartości raw/jednostki, trwałość stanów wyjść. |
| IFTTT `/ifttt` | Kreator źródła/filtru/akcji z deskryptorów, lista reguł, limity, suspended, ostatnie wykonanie i błędy. |
| Sensors `/sensors` | Instancje, modele GPIO32/33, interwały, TTL, jednostki, jakość pomiaru, konfiguracja sensorów I2C. |
| Interfaces `/interfaces` | Magistrale, rezerwacje, UART, UDP/TCP/MQTT/HTTP, Wi-Fi, katalog modułów, RF24 i zapisane ramki. |
| Bluetooth `/bluetooth` | OFF/scan/beacon, wyniki i ustawienia BLE, brak udawanej obsługi Classic Bluetooth. |
| LoRa `/lora` | Stan, parametry radiowe, RX/TX i historia; tylko profil obsługujący SX1278. To LoRa punkt-punkt, nie automatycznie LoRaWAN. |
| Screen `/screen` | Model kontrolera, geometria, adres/bus, jasność/obrót, konfigurowalna liczba stron, osobny grid/widgety każdej strony, dane interfejsów, kolejność i timeout powrotu z akcji IFTTT. |
| Diagnostic `/diagnostic` | Pinout i diagram, zasoby/konflikty, detekcja, kolejki, błędy, reset, pamięć, NVS, czasy obsługi. |
| Settings `/settings` | Ustawienia systemowe, sieć/auth, hostname/mDNS, DHCP/static IP/DNS, wersja config, backup/import z walidacją, polityka relay RAM/NVS i startup. |

RF24 może później dostać osobną stronę bez zmiany routera; w pierwszym zakresie zachować go w Interfaces. Nie dopisywać „itp.” jako nieograniczonego zakresu.

### 7.4. Interfejsy JS

Moduły stron realizują kontrakt `id`, `labelKey`, `supports`, `mount(context)`, `update(snapshot)`, `unmount`. Widgety: `supports(descriptor)`, `mount`, `update`, `destroy`.

- Rejestr rendererów według typu/właściwości i opcjonalnej wskazówki UI; domyślny renderer tekstowy i formularz schematu zapewniają obsługę nowych standardowych modułów.
- Specjalistyczny widget WS2812 lub diagram ma własny moduł, ale korzysta z tego samego API klienta i dispatcherów.
- `unmount/destroy` usuwa listenery i timery. Polling ma jednego koordynatora, nie osobny nieskończony timer dla każdej karty.
- Zachować PL/EN, focus, ARIA, obsługę klawiatury, mobile i `prefers-reduced-motion`.
- Zachować stary link `/` oraz mapowanie overview→dashboard, automation→ifttt, diagnostics→diagnostic.
- Wybrać routing hash na pierwszą wersję, aby odświeżenie nie wymagało przepisywania routingu LittleFS. Podane route są logicznymi nazwami.
- Nie dodawać frameworka SPA ani bundlera wyłącznie dla modularności; native ES modules są pierwszym wyborem.

### 7.5. Konfiguracja sieci, hostname i mDNS/DNS-SD

Stan obecny: [src/wifi_web_ui.cpp](./src/wifi_web_ui.cpp) uruchamia
`MDNS.begin("kc868-a6")` przy połączeniu Wi-Fi i podaje sztywny
`kc868-a6.local` w statusie. Docelowo hostname i reklama usług wynikają z
ConfigService/NetworkService, a status odzwierciedla rzeczywiste działanie.

**Cel:** użytkownik otwiera np. `http://sterownik-salon.local`, bez
znajomości IP, a narzędzia discovery znajdują usługę HTTP przez DNS-SD.
Nie oznacza to rejestracji publicznej domeny ani automatycznego DNS w routerze.

Konfiguracja WebGUI Settings→Sieć; Interfaces pokazuje aktualny stan:

- Hostname urządzenia: pojedyncza etykieta ASCII 1–63 znaków, litery/cyfry
  i myślnik, bez myślnika na początku/końcu; normalizacja do lowercase.
  Nie akceptować URL, spacji, `.local` ani pełnej domeny jako hostname.
- Ten hostname użyć również dla klienta DHCP Wi-Fi, jeśli stos wspiera
  ustawienie przed startem interfejsu. DHCP hostname nie gwarantuje,
  że router udostępni nazwę w zwykłym DNS.
- Default dla nowych konfiguracji: unikalny `kc868-a6-<deviceSuffix>`;
  legacy zachowuje `kc868-a6`, ale UI ostrzega o możliwych kolizjach.
  Suffix z trwałego device ID, nie losowany po każdym resecie.
- `mdns.enabled`, nazwa instancji usługi dla człowieka oraz włączenie reklamy
  HTTP. Rozróżnić hostname `sterownik-salon`, FQDN `sterownik-salon.local`
  i service instance name, np. `Sterownik salon`.
- Wi-Fi STA: SSID/sekret, DHCP albo static IPv4, adres/maska/gateway,
  DNS primary/secondary; provisioning AP ma osobną konfigurację/status.
- Dla static IPv4 walidować format, maskę ciągłą, adres hosta, subnet i
  gateway zgodnie z obsługiwanym modelem LAN; odrzucać multicast/unspecified
  oraz adres sieci/broadcast tam, gdzie mają takie znaczenie.
- DNS dla static IP jest jawny; nie podstawiać niejawnie publicznego DNS
  i nie pokazywać „NTP działa”, gdy rozwiązywanie nazw nie działa.
- Nie wprowadzać IPv6 jako pozornej konfiguracji IPv4. Opcje IPv6 tylko
  przy rzeczywistym wsparciu stosu i osobnej walidacji.
- Sekrety sieciowe redacted; zmiany wymagające reconnect poprzedzone
  ostrzeżeniem, gdzie panel będzie dostępny po zmianie.

Przykładowy **docelowy** fragment konfiguracji:

```json
{
  "network": {
    "hostname": "sterownik-salon",
    "wifi": {
      "ipv4Mode": "dhcp"
    },
    "mdns": {
      "enabled": true,
      "instanceName": "Sterownik salon",
      "advertiseHttp": true
    }
  }
}
```

Lifecycle i discovery:

- Responder startuje dopiero po uzyskaniu właściwego lokalnego adresu.
  Reconnect, zmiana hostname i wyłączenie mDNS aktualizują responder i
  usuwają nieaktualną reklamę; używać API zgodnego z wersją core.
- DNS-SD: `_http._tcp` z faktycznym portem HTTP, opcjonalnie `_https._tcp`
  wyłącznie gdy HTTPS rzeczywiście działa. Nie reklamować nieistniejących usług.
- Bounded TXT metadata: model, device ID, firmware/API version i path;
  bez haseł, tokenów, kluczy sieciowych oraz pełnego katalogu capabilities.
- Na pierwszym etapie reklamować tylko na Wi-Fi STA LAN. AP pozostaje
  dostępne przez swój IP/captive portal; mDNS na AP dopiero po sprawdzeniu
  backendu. LTE/PPP nie zakłada multicast discovery ani dostępu z internetu.
- mDNS jest link-local multicast; `.local` może nie działać przez VLAN,
  router, client isolation albo bez obsługi mDNS na kliencie. Pokazać
  również IP jako fallback, ale nie twierdzić, że discovery się udało.
- Kolizja nazwy: obsługa zależna od respondera; sprawdzić API/behavior.
  Raportować requested hostname, effective hostname jeśli dostępny,
  responder state i błąd. Nie tworzyć pozornego adresu z żądanej nazwy.
- Status rozróżnia mDNS started/advertising/error od potwierdzenia
  rozwiązania nazwy przez innego klienta. `MDNS.begin` success nie jest
  dowodem, że telefon odnalazł urządzenie.
- Błąd mDNS nie zatrzymuje IO ani HTTP po IP, ale jest widoczny w Diagnostic.
- Discovery nie zmienia auth i nie zapewnia szyfrowania; hostname z reklamy
  nie jest dowodem tożsamości urządzenia.

Zmiany sieci używają config revisions i walidacji z rozdziału 5.2. Podczas
reconnect nie trzymać handlera HTTP przez cały timeout. UI dostaje stan
pending/reconnecting i instrukcję nowego URL/IP; brak potwierdzenia stosowania
to błąd/pending, nie „zapisano i połączono”. Zachować dotychczasowy mechanizm
recovery/provisioning po braku sieci i przetestować static IP błędnego LAN.

API config/state/diagnostics: configured mode, requested/effective hostname,
sta/AP addresses, gateway/DNS, discovery services i last error. OLED może
wyświetlać IP/hostname/mDNS status jako zwykłe publiczne properties.

## 8. OLED — oddzielenie kontrolera od stron

- `IDisplayDriver` zapewnia abstrakcję canvasu; strony nie zawierają kodu SSD1306/SSD1309/SH1107.
- Adapter biblioteki zapewnia tekst, linie, prostokąty, piksele i wymiary po obrocie. Funkcje nieobsługiwane mają jawną flagę możliwości.
- `OledPageManager` rejestruje strony i filtruje wymagania, zarządza auto-rotate, wyborem ręcznym, IFTTT override i powrotem.
- Obecne strony przenieść do `StatusPage`, `SensorsPage`, `AirPage`, `CustomGridPage`.
- Pola siatki wskazują property ID, nie globalną maskę nowych sensorów. Stary `displayFieldMask` migruje do listy pól.
- Układ zależy od rzeczywistej geometrii: 128×64 nie jest założeniem dla wszystkich SH1107.
- Renderowanie z read modelu jest testowalne na mock canvasie, bez fizycznego OLED.
- Akcje: `selectPage`, `setEnabled`, `setBrightness`, opcjonalne `showMessage` z ograniczoną długością/TTL.
- Trigger: zmiana strony lub stanu ekranu, jeśli publikowany przez moduł. To nie obowiązek tworzenia fikcyjnych zdarzeń dla każdego frame.
- Konfiguracja modelu jawna; nie identyfikować kontrolera tylko po adresie `0x3C`.

### 8.1. Konfigurowalna liczba stron i grid osobno dla każdej strony

Docelowe `display.pages` to lista stron ze stabilnymi ID; liczba stron
wynika z tej listy, nie ze stałej `3` ani zestawu wartości `displayMode`.
W WebGUI dodawanie/usuwanie/duplikowanie i zmiana kolejności stron:

- Początkowy zakres: 1–8 stron na ekran i do 16 widgetów na stronę;
  rzeczywiste limity publikowane przez katalog, zatwierdzone RAM/NVS testem.
  Wyłączony ekran nie wymaga kasowania zapisanych stron.
- Każda strona ma własne `grid.columns`, `grid.rows`, spacing/padding,
  listę widgetów, nazwę, enabled i dwell time. Nie ma jednej globalnej
  siatki wspólnej dla wszystkich widoków.
- Propozycja grid 1–8 kolumn i 1–8 wierszy, `column`/`row` liczone od 0,
  `columnSpan`/`rowSpan` dodatnie. Granice/limity typu integer sprawdzać
  przed przydziałem oraz przed przeliczeniem na piksele.
- Pozycjonowanie: drag-and-drop ze snap-to-grid i uchwyty zmiany rozmiaru,
  ale również pola numeryczne oraz keyboard controls jako pełna alternatywa.
- Brak nakładania się widgetów w pierwszym zakresie; overlap/out-of-bounds
  daje błąd z ID obu elementów. Nie przesuwać automatycznie po zapisaniu.
- Prostokąty wynikają z geometrii canvasu **po obrocie**, z deterministycznym
  podziałem reszty pikseli. Nie gubić ostatniej kolumny/wiersza przez zaokrąglenie.
- Walidować minimalny rozmiar wybranego widgetu/fontu. Zbyt gęsty grid
  dla małego ekranu daje jawny błąd, nie zerowy rozmiar komórki.
- Zmiana modelu/geometrii/obrotu wymaga ponownej walidacji wszystkich
  stron. UI proponuje korektę layoutu, nie niszczy go niejawnie.
- Nie usuwać strony na podstawie chwilowej niedostępności sensora.
  Widget pokazuje unavailable/stale; strona pozostaje konfigurowalna.

Manager:

- Kolejność `pages[]` określa auto-rotate; dwell per strona, globalny
  default tylko jako wartość początkowa nowej strony.
- Tryb fixed wskazuje page ID, nie pozycję w tablicy. Dodanie/reorder
  strony nie zmienia celu akcji IFTTT.
- Usunięcie strony użytej przez IFTTT wymaga ostrzeżenia i zawiesza tę
  regułę, chyba że użytkownik jawnie zmieni target. Nie mapować na kolejny indeks.
- Wszystkie strony disabled przy enabled display: błąd konfiguracji.
  Brak dostępnego źródła nie oznacza disabled strony.
- Akcja `selectPage` obsługuje każdą skonfigurowaną, włączoną stronę oraz
  jawny override TTL/powrót do fixed/auto; nie utrwala wyboru co klatkę.
- Presety status/sensors/air/custom są szablonami tworzącymi strony
  gridowe i bindings, nie czterema niezmiennymi jedynymi widokami.

### 8.2. Dane sensorów i interfejsów na OLED

Każdy widget danych wskazuje stabilne `propertyId` z CapabilityRegistry,
nie nazwę biblioteki ani sztywną maskę „sześciu pól”. Ten sam mechanizm
obejmuje lokalne i proxy properties z koprocesora:

- IO: przekaźniki/DI/ADC, DAC, PWM duty i logical state WS2812.
- Sieć: IP, hostname, RSSI, mDNS state, Wi-Fi/cellular connection.
- UART/P17: link state, baud, RX/TX counters, peer health i wybrane dane.
- MQTT/UDP/TCP/HTTP: connected, counts, last status, jawnie opublikowana
  ostatnia wiadomość lub pole wiadomości.
- LoRa/RF24/CC1101/BLE/NFC: RSSI, counters, last bounded payload/UID,
  peer/node availability zgodnie z adapterem.
- GNSS/NTP/RTC: współrzędne/fix, UTC/local time, sync source/quality.
- Sensory i offload bridge: dowolne wspierane typed properties.

Discrete events nie są automatycznie trwałą właściwością. Jeśli ekran
ma pokazać ostatnią ramkę/wiadomość, odpowiedni moduł publikuje bounded
property z timestamp/TTL; OLED nie konsumuje ani nie przechwytuje zdarzeń
zamiast IFTTT. Nie utrzymywać osobnego nieograniczonego logu ekranu.

Widgety pierwszej wersji: label/static text, value+unit, bool/enum status,
progress/bar i time. Bounded tekst/binary render jako tekst/hex ma jawny
limit; dowolna ścieżka JSON nie jest parserem skryptu. Pola structured
messages muszą być jawnie opisane przez adapter jako property/subfield.

Każdy widget: ID, type, property binding, opcjonalny label, format/precision,
font/alignment, grid position/span i zasady unavailable/stale. Formatowanie
nie zamienia niepoprawnej wartości na zero. Długie teksty: deterministyczne
clip/wrap zgodnie z widgetem; scrolling tylko jako osobny wspierany tryb.
Unicode/PL na OLED zależy od fontu; znak nieobsługiwany ma jawne ograniczenie/
transliterację, nie obietnicę identycznego renderowania jak w przeglądarce.

UI pobiera wyłącznie publiczne display-eligible properties; żadnych haseł,
tokenów, kluczy MQTT/Meshtastic/Thread. OLED korzysta z cached snapshotów,
nie wywołuje synchronizującego odczytu sieci/sensora przy renderowaniu.

Przykładowa **docelowa** strona:

```json
{
  "id": "page.network",
  "name": "Network",
  "enabled": true,
  "dwellSeconds": 5,
  "grid": { "columns": 2, "rows": 2, "gapPx": 0, "paddingPx": 0 },
  "widgets": [
    {
      "id": "widget.hostname",
      "type": "value",
      "propertyId": "network.main/property/hostname",
      "label": "Host",
      "column": 0,
      "row": 0,
      "columnSpan": 2,
      "rowSpan": 1
    },
    {
      "id": "widget.rssi",
      "type": "value",
      "propertyId": "network.main/property/rssi",
      "label": "RSSI",
      "column": 0,
      "row": 1,
      "columnSpan": 1,
      "rowSpan": 1
    }
  ]
}
```

Property ID powyżej jest planowanym bindingiem, nie istniejącym endpointem.
Preset dla 128×64 musi dobrać fonty i etykiety do faktycznej powierzchni.

### 8.3. Edytor, zapis i migracja OLED

- Screen: lista stron + edytor wybranej strony + podgląd rzeczywistego
  aspect ratio/orientation. Selector danych filtrowany po typie widgetu,
  z jednostkami, jakościami i nazwami interfejsów.
- Preview używa tych samych zasad geometrycznych, formatowania i font
  metrics co renderer lub jawnie pokazuje ograniczenia przybliżenia.
  Nie oznaczać browser preview jako dowodu hardware rendering.
- Dodanie strony, reorder i zmiana grid nie nadpisują niezapisanych zmian
  innych stron. Save waliduje cały layout i config revision; odrzucenie
  pozostawia aktywny układ bez zmian.
- Layout zapisany jako wersjonowany config przy zatwierdzeniu, nie podczas
  każdego drag/move/refresh. Uwzględnić budżet istniejącego NVS 20 KiB:
  limity serialized layout i całej konfiguracji, pozostałe klucze i
  candidate/active. Jeśli nie mieści się z bezpiecznym zapasem, jawnie
  wybrać atomic/versioned LittleFS store i plan recovery zamiast truncation.
- API config udostępnia pages/grid/widgets i limity; actions wybierają ID.
  Stary endpoint OLED nie może spłaszczyć wielu stron do starej maski
  bez błędu incompatibility.
- Migracja legacy tworzy strony status/sensors/air/custom ze stabilnymi ID,
  zachowuje `displayMode`, kolejność, dwell i fields mask custom. Nowe
  strony są dopisywane przez użytkownika; nie zmieniać automatycznie liczby
  stron na podstawie liczby podłączonych interfejsów.
- Render mock testuje dokładne pixel bounds i clipping; hardware test
  potwierdza fonty, obrót i widoczność dla SSD1306/SSD1309/SH1107.

## 9. Nowe peryferia i interfejsy — szczegółowy zakres

### 9.1. Macierz integracji

| Moduł | Właściwości / źródła IFTTT | Akcje IFTTT | UI |
| --- | --- | --- | --- |
| Relays | stan kanału, applied/error, zmiana stanu | set/toggle | IO, Dashboard, Settings |
| DI / S2 | stan, zbocze, debounce | Brak sztucznej akcji wyjściowej | IO, IFTTT |
| ADC | raw 12-bit, valid/stale; napięcie tylko po kalibracji | Opcjonalna jawna konfiguracja, nie „ustaw ADC” | IO, Sensors |
| DS18B20 / DHT11 / DHT22 | temperatura; wilgotność wyłącznie DHT; jakość | Brak obowiązkowej akcji | Sensors |
| BME680 / ENS160 / QMC5883L / RTC / GPS / NFC | istniejące pomiary i zdarzenia UID/time/location stosownie do modułu | Tylko faktycznie obsługiwane, np. ustawienie RTC po walidacji | Sensors, Interfaces |
| WS2812 | logical on/color/brightness/effect, applied/error | setColor, setBrightness, setEffect, off | IO, Dashboard |
| PWM Fan | requested/applied duty, frequency, ready; RPM tylko przy tachometrze | setDuty, stop | IO, Sensors |
| SSD1306 / SSD1309 / SH1107 | enabled/page/geometry/health; pageChanged | selectPage, power, brightness/message zgodnie z możliwościami | Screen |
| DAC | raw 0–255, requested/applied/error | setRaw, setPercent | IO |
| RS-232 / RS-485 | frameReceived, link/error/counters | sendFrame | Interfaces |
| UDP | datagramReceived, counters/error | sendDatagram | Interfaces |
| TCP | connected/disconnected/frameReceived/error | connect/disconnect/sendFrame zgodnie z trybem | Interfaces |
| MQTT | messageReceived/connected/disconnected/error | publish | Interfaces |
| HTTP API/client | authenticated webhookReceived, response/error/status | request | Interfaces |
| BLE | advertiserObserved, scanCompleted, stan trybu | startScan/setBeacon/stop zgodnie z aktualną implementacją | Bluetooth |
| LoRa / RF24 | packetReceived, TX/RX/error/counters | send, RF24 saved replay | LoRa / Interfaces |

Zdarzenia wyjść są zmianami stanu logicznego, nie potwierdzeniem fizycznym bez osobnego sensora.

### 9.2. WS2812

- Jawna instancja: data GPIO, liczba LED, kolejność kolorów, jasność 0–255 i limit częstotliwości odświeżania.
- Pierwszy zakres: kolor całej taśmy, zakres LED oraz ograniczona lista efektów: static, blink, breathe.
- RGB to liczby 0–255; indeks i długość segmentu sprawdzane przed modyfikacją.
- Bufor alokowany raz; limit liczby LED wynikający z budżetu RAM i czasu `show`, opublikowany przez katalog.
- Sterownik wykorzystuje bibliotekę zgodną z faktycznie zainstalowanym Arduino-ESP32/RMT; nie dodawać zależności przed wyborem i testem builda.
- GPIO wybierane spośród jawnie dopuszczonych połączeń zewnętrznych; brak domyślnego zajęcia GPIO13, 25, 26, 32 lub 33.
- Uwzględnić zasilanie taśmy, wspólną masę, level shifter i ograniczenia prądowe. GPIO nie zasila LED.
- Start OFF, bez automatycznego odtwarzania stanu, dopóki nie powstanie osobna zatwierdzona polityka. Wymaganie RAM/NVS dotyczy przekaźników.

### 9.3. DHT11

- Rozszerzyć istniejący selector bez zmiany znaczenia NVS: `3=DHT11`.
- Wewnętrzny model używa enum/string ID, legacy liczby pozostają w adapterze migracji.
- Przełączenie DHT11/DHT22 rekonfiguruje bibliotekę z właściwym modelem, nie tylko etykietę.
- GPIO32 i GPIO33 pozostają domyślnymi gniazdami sensorów; model OFF/DS18B20/DHT22/DHT11.
- Odczyt i błędy zgodne ze wspólną jakością danych; nie współdzielić ostatniej wilgotności poprzedniego modelu.
- Nie zakładać takiej samej dokładności i zakresów DHT11 oraz DHT22. Zakresy z dokumentacji modelu opisać w deskryptorze i testach.

### 9.4. SSD1309 i SH1107

- Dodać sterowniki/adaptory do tego samego `IDisplayDriver`; zachować SSD1306.
- Pierwsza iteracja: modele z potwierdzonym I2C i konkretną geometrią. Moduł tylko SPI wymaga osobnego adaptera i rezerwacji, nie może być deklarowany jako działający I2C.
- Konfiguracja: controller, width, height, address, rotation, brightness, optional reset pin.
- Wybrać bibliotekę faktycznie obsługującą dany kontroler, offsets kolumn, RAM mapping i orientation.
- Dla SSD1309 jawnie określić tryb zasilania/inicjalizacji; nie kopiować bez weryfikacji komend charge pump SSD1306.
- Testować rozmiary wybranego modułu SH1107, tekst przy obrocie i pełną powierzchnię ekranu.
- Przy tym samym adresie nie dopuszczać dwóch instancji na tej samej magistrali bez separacji adresowej/topologii.
- Brak modułu lub błąd init nie blokuje IO i sieci.

### 9.5. PWM wentylatora

- `PwmFanPeripheral` korzysta z współdzielonego managera PWM, nie kopiuje kodu beepera.
- Konfiguracja: mode `four_wire_control` albo `power_pwm`, GPIO, częstotliwość, rozdzielczość, polaryzacja i zakres duty.
- Dla wentylatora 4-wire przewidzieć 25 kHz jako propozycję zależną od jego dokumentacji; odpowiedni open-drain/open-collector driver i wspólna masa.
- Dla power PWM wymagany zewnętrzny stopień mocy; nie sterować zasilaniem wentylatora bezpośrednio z ESP32/DA-1.
- `0%` ma zdefiniowaną semantykę logiczną; fizyczne zatrzymanie zależy od wentylatora i interfejsu. UI nie pokazuje „RPM=0” bez pomiaru.
- Start bezpieczny według jawnej konfiguracji konkretnego zastosowania. Propozycja ogólna: duty 0, ale system chłodzenia wymagający fail-safe może potrzebować 100%; przed użyciem sprzętowym zatwierdzić.
- Opcjonalny tachometr wymaga osobnego GPIO i deskryptora RPM; bez przewodu tach nie publikować fikcyjnych obrotów.
- Testować kolizje timerów i częstotliwości z beeperem oraz potencjalnymi innymi PWM.

### 9.6. DAC

- Dwa kanały ESP32: GPIO26 DA-1 i GPIO25 DA-2, raw 8-bit 0–255.
- API/IFTTT używa raw lub percent; przeliczenie percent zaokrąglane i testowane.
- Napięcie na złączu zależy od układu płytki; nie obiecywać 0–3,3 V albo 0–10 V bez weryfikacji toru analogowego.
- Konflikt DA-1 z beeperem i innymi funkcjami jest walidowany centralnie.
- Start raw=0, ograniczenia zakresu, raport stanu logicznego bez deklaracji pomiaru rzeczywistego napięcia.

### 9.7. RS-232 i RS-485

- Wspólny typ ramki i helper framing; osobne adaptery do UART1 i UART2.
- Konfiguracja baud/parity/stop bits ograniczona do wspieranych ustawień; zachować 9600 8N1 jako legacy default.
- Framing: delimiter z limitem długości albo fixed length/idle timeout. Wybór jawny. Odbiór częściowych ramek i przepełnienie testowane.
- Payload bytes/hex albo tekst z określonym encoding, nie niejawna liczba.
- RS-232: UART1 RX17/TX16; konflikt GPS/legacy coprocessor albo
  przypisania tego samego kontrolera do P17 odrzucany.
- RS-485: UART2 RX14/TX27. Zweryfikować kierunek DE/RE lub auto-direction według schematu/revision; nie wymyślać wolnego GPIO dla DE.
- Half-duplex: po TX uwzględnić opróżnienie UART i powrót RX, timeout oraz echo jeśli występuje.
- Nie dodawać Modbus jako ukrytego wymagania. Ramki raw w pierwszym zakresie; Modbus może być osobnym modułem później.
- P17 TTL może używać pełnego sprzętowego UART1 lub UART2: docelowy host
  TX12/RX13, nie RS-232. Nie jest to dodatkowy czwarty kontroler UART;
  patrz rozdział 21 i konflikt GPIO13 z obecnym PN532 CS.

### 9.8. UDP

- Endpoint z local port oraz jawnie skonfigurowanym remote host/port do akcji.
- Odbiór pojedynczego datagramu daje osobne zdarzenie z payload, source IP/port i ID.
- Limity długości, filtr source i rate limit. Broadcast/multicast wyłączony w pierwszym zakresie.
- Wysyłanie nie potwierdza odbioru. Wynik oznacza lokalne przyjęcie/nadanie, nie delivery.
- Za duży datagram jest jawnie odrzucony; nie generować prawidłowo wyglądającego obciętego payloadu.

### 9.9. TCP

- Jawny tryb client albo server. Pierwsza wersja server: jeden aktywny klient na instancję; więcej wymaga pomiarów.
- TCP jest strumieniem: ramkowanie delimiter/fixed length/length-prefix z ograniczeniami; nie traktować każdego `read` jako kompletnej wiadomości.
- Client: reconnect z bounded backoff; server: kontrola przyjęcia klienta, limity i timeout idle.
- Akcje send/connect/disconnect zgodne z trybem. Po disconnect usunąć niepełną ramkę z jawnym logiem.
- Testować częściowe write/read, utratę Wi-Fi, DNS error i powtarzający się connect.

### 9.10. MQTT

- Konfiguracja broker host/port, clientId, auth, TLS policy, subscriptions, publish defaults, keepalive i LWT.
- Bibliotekę wybrać pod non-blocking reconnect, RAM i realną obsługę TLS; certyfikatów nie weryfikować w trybie „insecure” domyślnie.
- W pierwszym zakresie gwarantować QoS0; QoS1 i potwierdzenia tylko jeśli wybrany stos je rzeczywiście wspiera. Nie deklarować QoS2 bez implementacji.
- Trigger zawiera topic, payload, retain i QoS. Wildcards subskrypcji walidowane.
- Retained message po reconnect domyślnie nie wyzwala sterowania; reguła może jawnie włączyć `includeRetained`.
- Akcja publish: topic/payload/retain/QoS z walidacją i limitem. Publish success rozróżniony od dostarczenia odbiorcy.
- Brak domyślnej nieograniczonej kolejki offline. Przy disconnected jawny błąd, chyba że osobna ograniczona kolejka jest świadomie skonfigurowana.
- Sekrety nigdy nie trafiają do capabilities/config export/logów.

### 9.11. API HTTP — dwa różne kierunki

1. **Inbound webhook**: uwierzytelniony endpoint, np. `POST /api/v2/integrations/webhooks/{id}`, publikuje typed event z walidowanym payloadem.
2. **Outbound HTTP client**: akcja request do wcześniej skonfigurowanego endpointu; trigger response/error z status code i bounded body.

- Metody GET/POST/PUT/PATCH/DELETE zależnie od katalogu endpointu; timeout, TLS/certyfikaty, maksymalny body i filtr odpowiedzi.
- Adresy endpointów konfiguruje administrator. Przychodzący payload nie może sam wybierać URL, hosta, nagłówka auth ani ścieżki do plików.
- Brak automatycznych redirectów z przekazaniem sekretów do innego hosta.
- Inbound event dopiero po poprawnym auth i schemacie. Osobne tokeny/scopes dla integracji; istniejące Basic Auth pozostaje dla kompatybilności panelu.
- Nowe webhooki i sterowanie transportami domyślnie niedostępne w otwartym provisioning AP. Zachować dotychczasową procedurę setup, ale nie rozszerzać niejawnie jej zakresu.
- CORS wyłączony, chyba że konkretna integracja świadomie go wymaga; nie publikować `*` dla API sterującego.
- Request zaakceptowany asynchronicznie, zakończenie widoczne w operation history i IFTTT.

### 9.12. Zachowanie istniejących modułów

- RF24 i LoRa mają osobne adaptery capability, współdzielą SPI manager. Nie włączać obu na wspólnym CS5.
- LoRa: dodać rzeczywistą ścieżkę RX/TX i zdarzenia, limity payload/rate; częstotliwość i parametry wymagają zgodności modułu i lokalnych regulacji.
- BLE pozostaje BLE scan/beacon; nie rozszerzać do Classic ani dowolnego GATT bez osobnego wymagania.
- Legacy GPS/coprocessor zachowują rolę UART1 dla zgodności; nowy koprocesor
  P17 ma osobny profil routingu kontrolera. Sam link UART nie implementuje
  Zigbee/Thread.
- RTC/NFC i pozostałe sensory przenieść bez utraty obecnych odczytów; wykorzystać zdarzenia tylko tam, gdzie rzeczywiście dostępne.

## 10. Stany przekaźników: RAM albo NVS

### 10.1. Konfiguracja i semantyka

Wprowadzić:

```json
{
  "relayStateStorage": "ram",
  "relayStartupPolicy": "all_off"
}
```

Wewnętrznie enum, nie rozrzucona flaga bool. UI może przedstawiać wybór jako „Zapamiętuj stany po zaniku zasilania”. Konfiguracja wyboru jest zawsze trwała w NVS, nawet gdy **stany** są tylko w RAM.

| Storage | Startup policy | Zimny start po zaniku zasilania | Reset programowy/watchdog |
| --- | --- | --- | --- |
| `ram` | `all_off` | OFF dla wszystkich kanałów | OFF; zwykły RAM nie jest magazynem restartowym. |
| `nvs` | `restore_last` | Najpierw bezpieczna inicjalizacja OFF, następnie odtworzenie ostatniego poprawnie zapisanego stanu | Ta sama polityka odtwarzania. |
| `nvs` | `all_off` | OFF, mimo zachowania snapshotu w NVS | OFF; nie nadpisywać snapshotu samą inicjalizacją OFF. |

`ram + restore_last` jest niepoprawną konfiguracją. Nie używać RTC memory jako ukrytej zmiany znaczenia RAM. Default i migracja: `ram + all_off`, tak jak dzisiejszy firmware.

### 10.2. Co i kiedy zapisywać

- Jeden rekord logicznych 6 bitów, nie raw PCF8574 z active-low ani P6/P7.
- Rekord: wersja, liczba kanałów, maska ON, generacja i kontrola integralności. Zweryfikować długość i zakres przy odczycie.
- W trybie NVS zapisywać po rzeczywistej, skutecznie zastosowanej zmianie, niezależnie od źródła: UI/API/IFTTT.
- Zmiana no-op nie zapisuje flash i nie generuje kolejnej generacji.
- Baseline trwałości: zapis od razu po zmianie, bez ukrytego debounce. Rate limit zmian sterujących może ograniczać zużycie flash, ale musi jawnie odrzucać nadmiarowe polecenia.
- ACK „stan utrwalony” dopiero po sukcesie zapisu i weryfikacji rekordu. Przerwanie zasilania przed ACK może pozostawić poprzedni snapshot.
- Nie obiecywać zachowania stanu przy zaniku dokładnie podczas przełączania lub zapisu. Testy potwierdzają granicę: poprawnie zakończona operacja vs przerwana.
- NVS write error po udanym sprzętowym przełączeniu: raport `applied=true`, `persisted=false` i błąd. Nie udawać, że przekaźnik nie został zmieniony, i nie cofać go bez jawnej polityki.
- Brak NVS przy wymaganym trybie trwałym: start OFF, degraded health, jawny błąd; nie przełączać konfiguracji automatycznie na RAM.
- Nie zapisywać OFF z inicjalizacji przed odczytem snapshotu do odtworzenia.

### 10.3. Zmiana trybu

- RAM → NVS: po potwierdzeniu użytkownika zapisać aktualny zastosowany stan jako pierwszy snapshot; zmiana trybu nieudana, jeśli zapis zawiedzie.
- NVS → RAM: zapisać trwałą konfigurację trybu, zaprzestać utrwalania stanów; stary rekord może zostać zachowany, ale nie jest używany.
- Ponowne RAM → NVS zastępuje snapshot bieżącym stanem, a nie odtwarza przypadkiem starego.
- Zmiana startup policy działa przy kolejnym restarcie i jest opisana w UI; nie przełącza natychmiast fizycznych przekaźników.

### 10.4. Sekwencja startu

1. Uruchomić minimalne logowanie.
2. Odczytać i zwalidować konfigurację oraz ewentualny snapshot.
3. Uruchomić I2C i ustawić PCF8574 na bezpieczne OFF z P6/P7 zachowanymi jako zarezerwowane.
4. W trybie restore odtworzyć tylko poprawny snapshot; sprawdzić wynik I2C.
5. Opublikować stan applied/health/persistence.
6. Dopiero teraz włączyć sterujące API, akcje transportów i evaluator IFTTT.

Brak/uszkodzony snapshot → OFF z kodem `snapshot_missing`/`snapshot_invalid`; nie tworzyć przypadkowej maski. Brak ekspandera → `unavailable`, brak deklaracji skutecznego OFF lub restore.

Software nie gwarantuje braku impulsów podczas narastania zasilania, resetu MCU i startu PCF8574. Sprawdzić to sprzętowo; jeśli wymagane są gwarancje, potrzebne może być sprzętowe blokowanie wyjść. Automatyczne odtwarzanie ON wymaga świadomego potwierdzenia ryzyka w UI.

## 11. Diagnostic — mapa KC868-A6 i diagram

### 11.1. Źródło i wiarygodność

Baza poniżej odzwierciedla aktualny nagłówek pinów i README. Nie jest nowym pomiarem sprzętu ani zatwierdzeniem każdej rewizji PCB.

Źródła producenta do zweryfikowania przed implementacją/hardware test:

- [Produkt i schemat KC868-A6](https://www.kincony.com/esp32-6-channel-relay-module-kc868-a6.html).
- [Forum — definicje I/O](https://www.kincony.com/forum/showthread.php?tid=1962).

Istnieje udokumentowana rozbieżność LoRa: forum RESET14/DIO0=21 kontra mapowanie przyjęte ze schematu RESET21/DIO0=2. Nie zmieniać mapowania bez sprawdzenia konkretnego PCB; GPIO14 należy też do RS-485 RX.

### 11.2. Mapa referencyjna

| Zasób / złącze logiczne | ESP32 / ekspander | Rola i ograniczenia |
| --- | --- | --- |
| Relay 1–6 | PCF8574 `0x24`, P0–P5 | Active-low. P6–P7 zarezerwowane. |
| DI 1–8 | PCF8574 `0x22`, P0–P7 | Odczyt active-low, nie GPIO wyjściowe. |
| A1/A2/A3/A4 | GPIO36/39/34/35 | ADC1, input-only; według README tor wejściowy płytki 0–5 V, nie bezpośrednie GPIO 5 V. |
| Sensor 1/2 | GPIO32/33 | OFF, DS18B20, DHT22, docelowo DHT11; alternatywna rola wyłącznie po rezerwacji i weryfikacji. |
| DA-1 / DA-2 | GPIO26/25 | DAC; DA-1 konflikt z beeperem/PWM. Zweryfikować napięcia i stopień wyjściowy płytki. |
| I2C SDA/SCL | GPIO4/15 | Wspólna magistrala. GPIO15 jest również pinem strapping ESP32. |
| RS-485 RX/TX | GPIO14/27, UART2 | Poziomy/transceiver według płytki, kierunek RS-485 do weryfikacji. |
| RS-232 RX/TX | GPIO17/16, UART1 | Alternatywnie GPS/coprocessor, nie równolegle. |
| P17 TTL UART (docelowy profil) | TX GPIO12 / RX GPIO13 | Pełny sprzętowy UART1 albo UART2 przez GPIO Matrix, nie czwarty UART. 3,3 V, GPIO12 strapping; RX13 koliduje z obecnym PN532 CS13. |
| SPI SCK/MISO/MOSI | GPIO18/19/23 | Wspólna magistrala radii i PN532. |
| Board radio CS | GPIO5 | Wspólne CS LoRa/nRF24; GPIO5 strapping. |
| LoRa RESET/DIO0 | GPIO21/2 | Weryfikacja rewizji; GPIO2 strapping. |
| nRF24 CE | GPIO22 | Wspólne SPI i CS5. |
| PN532 CS | GPIO13 | SPI; nie łączyć I2C PN532 `0x24` równolegle z ekspanderem przekaźników. |
| USB UART RX/TX | GPIO3/1, UART0 | Programowanie/logowanie, nie domyślna pula wolnych pinów. |
| S2 / BOOT | GPIO0 | Active-low, strapping/download mode przy resecie. |
| Reset | EN | Nie zwykły GPIO. |
| Wi-Fi / BLE | Radio ESP32-WROOM-32 | Brak Ethernet/Zigbee/Thread na samej płytce według obecnych źródeł. |

Adresy I2C: DI `0x22`, relays `0x24`, OLED zwykle `0x3C`, QMC5883L `0x0D`, ENS160 `0x53`, RTC `0x68`, BME680 `0x77`. Alternatywny adres/moduł wymaga konfiguracji i walidacji.

### 11.3. Diagram logiczny do odwzorowania w UI

```text
ESP32-WROOM-32
 |
 +-- GPIO4/15 -- I2C --+-- 0x22 PCF8574 --> DI1..DI8
 |                    +-- 0x24 PCF8574 --> Relay1..Relay6
 |                    +-- OLED / BME680 / ENS160 / RTC / QMC5883L
 |
 +-- GPIO36/39/34/35 ---- ADC <--- A1..A4 (tor wejściowy płytki)
 +-- GPIO32/33 --------- Sensor1/2 lub zatwierdzona alternatywa
 +-- GPIO26/25 --------- DA-1/DA-2 (DAC; kolizje PWM/beeper)
 +-- UART1 GPIO17/16 --- RS-232 ALBO GPS ALBO coprocessor
 +-- UART2 GPIO14/27 --- RS-485
 |   UART1 LUB UART2 --> P17 TX12/RX13 (zamiast powyższej roli)
 +-- SPI 18/19/23 ------+-- CS5 LoRa ALBO nRF24
 |                     +-- CS13 PN532
 +-- GPIO0 ------------ S2 / BOOT
 +-- UART0 GPIO3/1 ---- USB / logowanie
 +-- radio ------------ Wi-Fi / BLE
```

To diagram funkcjonalny, nie schemat elektryczny ani fizyczny rozkład złączy. Fizyczny board view wymaga osobnego sprawdzenia orientacji, numeracji i rewizji PCB.

### 11.4. Wymagania ekranu Diagnostic

- Widok tabelaryczny i SVG diagram generowane z `BoardProfile`, bez ręcznej drugiej mapy GPIO w JS.
- Każdy zasób: rola nominalna, aktualny właściciel, aktywna konfiguracja, stan, ograniczenia i konflikt.
- Kliknięcie GPIO/magistrali podświetla powiązane peryferia i przechodzi do ich konfiguracji.
- Filtry: wszystkie/zajęte/zarezerwowane/konflikty/niepotwierdzone.
- Status `unknown/unverified` dla rewizji, DE/RE i połączeń bez potwierdzenia; nie oznaczać ich zielonym „OK”.
- Legenda poziomów elektrycznych, input-only i strapping. Nie zalecać pracy z napięciem sieciowym ani testowania obciążeń niebezpiecznych.
- Detekcja I2C odróżniona od health sterownika; ręczny rescan ograniczony, nie ciągły scan zakłócający magistralę.
- Uptime, reset reason, firmware/build profile, wolny/minimalny heap, największy blok, loop latency, kolejki/drop counters, błędy bus, stan NVS i ostatni restore.
- Diagram ma działać lokalnie bez internetu/CDN, z opisem tekstowym i obsługą mobile.

## 12. Konfiguracja, migracje i zgodność

### 12.1. Wersjonowanie

- Osobne wersje config, rules, capabilities API i relay snapshot.
- Legacy config bez wersji traktować jako v1; migracja do wersji modularnej zachowuje dotychczasowe wartości.
- Nie usuwać starych kluczy NVS przed poprawnym zapisaniem i odczytem nowej konfiguracji.
- Użyć bezpiecznego mechanizmu candidate/active lub dwóch slotów z generacją; testować przerwanie zapisu.
- Limit długości nazw kluczy/namespaces Preferences/NVS sprawdzić w używanej wersji; długie ID instancji nie mogą bezpośrednio stać się kluczami.
- Odczyt nowszej nieobsługiwanej wersji ma jawny błąd kompatybilności, nie reset fabryczny.
- Import jest validate-only przed apply; backup z UI domyślnie wyklucza sekrety.
- Downgrade do starego firmware nie ma gwarancji obsługi nowej konfiguracji; zachować legacy dane i opisać granice zgodności.

### 12.2. Migracje wymagane

- `DeviceSettings` → system config + instancje peryferiów + display layout.
- gpio32/gpio33 selector z zachowaniem numerów 0/1/2.
- `displayMode` 0/1/2/3/4 → auto/status/sensors/air/custom.
- `displayFieldMask` → lista pól custom grid, bez zmiany kolejności.
- BLE OFF/scan/beacon, UART1 mode, UART baud i beeper parameters.
- RF24 saved packets z zachowaniem ID/nazw/payload i kompatybilności replay.
- IFTTT v1 → v2 z compatibility sources.
- Nowe pola relay storage/startup ustawione domyślnie RAM/OFF.
- Wszystkie istniejące flagi enabled zachowane; ograniczenia builda widoczne w statusie.

### 12.3. Budżety

Obecna tablica partycji: NVS `0x5000` = 20 KiB, każda aplikacja `0x180000` = 1,5 MiB, LittleFS `0x0E0000` = 896 KiB. Nie zmieniać partycji bez pomiaru i planu migracji; NVS jest współdzielone z innymi danymi stosu.

Przed nowymi bibliotekami zapisać baseline flash/RAM per profil. Duży capabilities JSON nie musi być budowany jako jeden `String` i dokument w RAM; stosować filtrowanie/paginację lub streaming zgodny ze stosem HTTP.

Początkowe limity transportów/LED/zdarzeń są propozycjami do zatwierdzenia pomiarami: 512 B payload sieci/serial, 32 zdarzenia w kolejce, 4 reguły, 4 akcje/regułę, 1 klient TCP server/instancję. Katalog publikuje faktycznie wdrożone limity. Limit WS2812 i liczby instancji wyznaczyć przed wdrożeniem, nie zostawiać nieograniczonych.

## 13. ToDo dla agentów LLM — kolejność i zależności

Jedyna główna checklista: wszystkie **50 zadań** z zakresu podstawowego,
rozszerzeń i mostka P17 są zebrane poniżej. Niezaznaczony checkbox oznacza,
że nie spełniono jeszcze całego kryterium ukończenia; częściowy postęp jest
opisany osobno poniżej.
Kolejność jest architektoniczna i topologiczna: każde zadanie występuje po
wszystkich swoich jawnych `Depends`. Zachowano identyfikatory, opisy i kryteria
ukończenia; nie zmieniono zakresu ani zależności przez samo sortowanie.

Etapy porządkują realizację, lecz nie tworzą dodatkowych zależności. Zadania
niezależne można wykonywać równolegle po ukończeniu ich rzeczywistych
prerekwizytów. Najpierw uzgodnić kontrakty i profile sprzętowe, potem budować
wspólne mechanizmy; nowe protokoły mają korzystać z gotowych rejestrów,
transportów, UI i zasad ownership, zamiast wprowadzać własne odpowiedniki.

Rozdziały 1–12 i 14–21 opisują wymagania i testy; rozdziały 20 oraz 21.7
nie mają odrębnych checklist. Opcjonalne rozszerzenia wybiera manifest
sprzętowy. Niewybrane zadanie nie jest automatycznie ukończone: przed
integracją jawnie dostosować zakres i zależności EXT-01 zgodnie z manifestem.

Każde zadanie kończy się raportem: zmienione pliki, kontrakty, testy, wynik,
ograniczenia i kolejny odblokowany krok.

### Ukończone przyrosty i pozostałe kryteria

Zweryfikowane implementacje i historyczne pomiary przeniesiono do
[TODO_REFACTORED.md](./TODO_REFACTORED.md). Pełne zadania poniżej pozostają
otwarte, jeżeli którykolwiek wymagany element nadal nie jest zrealizowany.

Ostatni zweryfikowany przyrost: CSS/grid DI, grupy Settings, poprawny JSON BLE,
odbiorniki UDP per reguła z filtrami, kontekst LLM i rozszerzenie istniejącego
workflow CI. Wgrano firmware base oraz matching LittleFS; wyniki i ograniczenia
w [archiwum wykonanych prac](./TODO_REFACTORED.md#settings-css-ble-udp-i-kontekst-llm--2026-10-31).

Przyrost v1.1.1: lista do ośmiu zapisanych BLE z niezależnymi
`ble.presence/<MAC>/in_range`, migracja pojedynczego celu i stabilny alias
legacy, walidacja/zapis NVS, UI listy i dynamiczne wyzwalacze.
Testy software/local builds PASS; nie wgrano tej wersji i nie wykonano
fizycznego testu obecności ośmiu nadajników.

Przyrost v1.1.2: RF24 UI ukryte do jawnego potwierdzenia compiled support,
kontrolki z pełnymi zakresami/nazwanymi trybami, multiline UDP/serial,
grid bounds/empty-input validation, workflow tag build/release retry.
Lokalne testy i synthetic browser PASS; zdalny push/Actions nadal niewykonane.

- [ ] Publikacja repozytorium i wykonanie GitHub Actions: **zablokowane w tej
  sesji przez brak możliwości przesłania kodu do zewnętrznej usługi**.
  Lokalny workflow/actionlint to nie dowód udanego builda na GitHub.

- [ ] Fizyczna kalibracja sensora/pętli, testy DHT11/BLE presence z urządzeniem,
  pomiary PWM/DA i pełna weryfikacja peryferiów wymagają właściwego sprzętu.

### Etap A — fundamenty architektury, sprzęt i kontrakty

- [ ] **BASE-01 — Zabezpieczyć istniejące zachowanie.** Depends: brak.
  - Odczytać projekt ponownie i zebrać przykładowe status/config/rules/RF24 JSON bez sekretów.
  - Zbudować profile kc868-a6 i kc868-a6-coprocessor oraz uzasadnione warianty compile flags; zapisać flash/RAM i faktyczną wersję Arduino-ESP32.
  - Dodać najmniejsze testy characterization dla IO, ADC, IFTTT edge, selectorów i migracji OLED.
  - Done: znane baseline, niepowodzenia istniejące oddzielone od nowych.

- [ ] **CORE-01 — Zdefiniować typowane kontrakty i deskryptory.** Depends: BASE-01.
  - Wdrożyć interfejsy z rozdziału 4, wartości/statusy i błędy.
  - Ustalić jeden schemat konfiguracji/akcji, typy i limity.
  - Done: fake peripheral rejestruje property/event/action; test duplikatu ID i walidacji typów, bez bibliotek hardware w rdzeniu.

- [ ] **HW-01 — Wybrać role protokołów i profil sprzętowy.** Depends: BASE-01.
  - Potwierdzić flash/PSRAM/moduł, pinout, UART/SPI/I2S/RMT i zasilanie;
    zdefiniować concurrent/exclusive features i rolę każdego NCP/RCP.
  - Zestawić build/map/runtime budgets i plan adaptera lub zewnętrznego hosta.
  - Done: zatwierdzony manifest profili, brak przypisania zajętych pinów;
    wszystkie niepotwierdzone hardware choices jawnie odnotowane.

- [ ] **BOARD-01 — Wprowadzić profil płytki i ResourceManager.** Depends: CORE-01.
  - Przenieść obecne stałe bez niepotwierdzonych zmian; dołączyć metadata ograniczeń.
  - Walidacja I2C/SPI/UART/GPIO/DAC/PWM/RMT.
  - Done: konflikty radio, UART1, PN532/I2C, beeper/DAC i strapping są testowane i raportowane.

- [ ] **CFG-01 — Wyodrębnić ConfigService i NVS adapter.** Depends: CORE-01, BOARD-01.
  - Odłączyć konfigurację sprzętu od WifiWebUi; wdrożyć wersje, candidate/active i jawne błędy.
  - Done: legacy settings migrują, przerwany zapis nie niszczy active, import invalid nie zmienia sprzętu.

- [ ] **CORE-02 — Wprowadzić rejestry, scheduler i zdarzenia.** Depends: CORE-01.
  - Ownership w App, binding bez RTTI, ograniczone kolejki, time source do testów.
  - Done: fake peripheral działa przez tick i dispatcher; test overflow, lifecycle, zmiany revision i rollover czasu.

### Etap B — ekstrakcja istniejących funkcji i zgodność

- [ ] **IO-01 — Wyodrębnić Relays, DI/S2, ADC i beeper.** Depends: CORE-02, BOARD-01, CFG-01.
  - Zachować active-low, limity kanałów, P6/P7, S2 debounce i parametry beeper.
  - Poprawić pełny zakres ADC w nowej i compatibility telemetrii.
  - Done: ręczne sterowanie przechodzi przez ten sam dispatcher co akcje; błędy I2C nie są sukcesem.

- [ ] **SENS-01 — Wyodrębnić istniejące sensory.** Depends: CORE-02, BOARD-01, CFG-01.
  - DS18B20/DHT22/BME680/ENS160/QMC5883L/RTC/NFC/GPS; per-device timing i jakość danych.
  - Done: wszystkie obecne pomiary i enabled działają; brak starej wartości jako bezterminowo valid; DS18B20 conversion test.

- [ ] **COMM-01 — Wyodrębnić BLE, LoRa i RF24.** Depends: CORE-02, BOARD-01, CFG-01.
  - Zachować profile i RF24 format/history/replay; utworzyć realne RX/TX LoRa oraz eventy.
  - Done: odbiór dwóch identycznych ramek daje dwa zdarzenia; konflikt CS5, ograniczenia BLE i bezpieczny stop.

- [ ] **OLED-01 — Rozdzielić driver, manager, strony i widgety OLED.** Depends: CORE-02, SENS-01, IO-01, CFG-01.
  - SSD1306 adapter i migracja czterech stron; render tylko z read modelu.
  - Done: mock canvas testuje geometrię i obrót; brak zależności stron od konkretnej biblioteki kontrolera.

- [ ] **IFTTT-01 — Zastąpić whitelisty kompilowanymi bindingami.** Depends: IO-01, SENS-01, COMM-01, OLED-01.
  - Condition/event, parametry akcji, cooldown/histereza, kolejność multi-action, operation tracking.
  - Done: wszystkie stare źródła/akcje działają przez adapter; brak parsowania JSON w tick; suspended i błędy widoczne.

- [ ] **MIG-01 — Wdrożyć i przetestować migracje legacy.** Depends: CFG-01, IFTTT-01, OLED-01, COMM-01.
  - Settings/rules/OLED/RF24, zachowanie ID i przerwane migracje.
  - Done: fixtures z v1 przechodzą, nowa nieobsługiwana wersja nie wywołuje resetu config, migracja idempotentna.

### Etap C — wspólne usługi, transporty i architektura offload

- [ ] **API-01 — Wprowadzić API v2 i compatibility router.** Depends: IFTTT-01, MIG-01.
  - Capabilities/catalog/state/config/actions/operations/rules/events/board/diagnostics.
  - Auth, limity, błędy i sekret-free responses.
  - Done: stary panel nadal działa przez adapter; nowa schema ma testy kontraktu i pełny zakres ADC.

- [ ] **NET-01 — Wydzielić NetworkService z provisioning.** Depends: CFG-01, API-01.
  - Wspólne network readiness, reconnect, mDNS i status; zachować dotychczasowy setup.
  - Done: transporty mogą reagować na Wi-Fi bez zależności od strony WWW, reconnect nie blokuje IO.

- [ ] **WEB-01 — Wyodrębnić router, API client i rejestry UI.** Depends: API-01.
  - ES modules, wspólny polling, PL/EN, page/widget lifecycle i formularze schematów.
  - Done: fake moduł standardowego typu pojawia się bez zmian centralnego HTML/kreatora IFTTT; brak wyciekających timerów.

- [ ] **SER-01 — Udostępnić RS-232/RS-485 w IFTTT.** Depends: CORE-02, BOARD-01, CFG-01, WEB-01.
  - Framing, RX event, TX action, UART ownership i kierunek RS-485.
  - Done: partial/oversize/timeout, half-duplex i kolizje GPS/coprocessor; potwierdzony tor elektryczny.

- [ ] **P17-01 — Dodać routing sprzętowego UART na P17.** Depends: HW-01, BOARD-01, SER-01, CFG-01.
  - Jednoznaczne TX12/RX13, controller ownership, pin/buffer/baud schema,
    wyłączenie kolidującej roli UART i PN532 CS13, bez software UART.
  - Done: UART1→P17 + RS-485 i UART2→P17 + RS-232 przetestowane;
    pełny duplex 115200/460800, błędy/overflow, reset strap i brak regresji
    dotychczasowego routingu. Wyższe baud tylko po realnej walidacji.

- [ ] **BRIDGE-01 — Zaimplementować kontrakt i host custom bridge.** Depends: P17-01, CORE-02, API-01.
  - Wersjonowany codec/framing, typed proxy registry, credits, sessions,
    request dedupe, accepted/completed/errors i config reconciliation.
  - Done: fake peer i parser testują partial/multiple/corrupt/oversize frames,
    zanik linku/reset, bounded inventory i brak replay nieidempotentnych akcji.

- [ ] **PEER-01 — Dodać firmware/profil peer H2/C6/C5.** Depends: BRIDGE-01.
  - Osobne build targets ze zgodnym SDK, role custom/NCP/RCP i pinout;
    dla custom bridge minimum jeden realny property/trigger/action.
  - Done: wybrany peer target działa sprzętowo, handshake/version manifest,
    offloaded funkcja nie utrzymuje duplikatu stosu na hoście.
    Pozostałe targety jako unsupported/pending do ich build/hardware testów,
    nie deklarować wsparcia trzech układów po jednym teście.

- [ ] **RTOS-01 — Wdrożyć zmierzony podział pracy i ownership.** Depends: CORE-02, IO-01, SENS-01, NET-01, HW-01.
  - Profilowanie, worker queues, bus/storage ownership, affinity i stack budgets.
  - Done: poprawa latency/no blocking potwierdzona A/B, brak regresji
    config/NVS/IR/audio i poprawny wariant single-core.

- [ ] **PROFILE-01 — Zoptymalizować workload 1–2 rozszerzeń.** Depends: HW-01, CORE-02, BRIDGE-01, PEER-01.
  - Manifest single/dual, compiled catalog vs active instances,
    exclude local offloaded libraries, buffer/task budgets i map raport.
  - Done: fresh builds i pomiary heap/latency dla konkretnych kompatybilnych
    par; brak nieużywanych dużych stosów i brak twierdzenia o runtime unload.

### Etap D — dynamiczne UI, discovery i diagnostyka

- [ ] **WEB-02 — Wdrożyć komplet stron i przenieść istniejące formularze.** Depends: WEB-01.
  - Wszystkie strony z rozdziału 7; conditional navigation i stany error/disabled/unsupported.
  - Done: ekran i ustawienia nie są dublowane; routing legacy, mobile, keyboard i retained form edits przetestowane.

- [ ] **DIAG-01 — Zbudować mapę zasobów i diagram.** Depends: API-01, WEB-01, BOARD-01.
  - BoardProfile→JSON→SVG/tabela, właściciele i konflikty, runtime metrics.
  - Done: brak drugiej mapy pinów w JS; oznaczone niepotwierdzone rewizje, brak udawanej fizycznej numeracji złączy.

- [ ] **DISCOVERY-01 — Dodać konfigurację sieci, hostname i mDNS.** Depends: NET-01, WEB-02, CFG-01.
  - DHCP/static IPv4/DNS, hostname, responder lifecycle i HTTP DNS-SD,
    config revisions, reconnect UX i public properties dla OLED.
  - Done: dwóch klientów/urządzenia z kolizją nazwy, zmiana hostname/IP,
    reconnect, mDNS off/error i dostęp po IP; złe ustawienia nie blokują
    niejawnie recovery, reklama usług odpowiada faktycznie aktywnym usługom.

- [ ] **BRIDGEUI-01 — Udostępnić P17 i peer w WebGUI/IFTTT.** Depends: BRIDGE-01, WEB-02, DIAG-01.
  - Role/routing/conflicts, generic widgets/config, peer/link diagnostics,
    scopes i pending configuration; bez zdalnego kodu/niechronionej konsoli.
  - Done: proxy module widoczny w IFTTT bez whitelist, disconnect suspenduje
    reguły, brak silent success przy utracie ACK config.

### Etap E — czas i harmonogramy automatyzacji

- [ ] **TIME-01 — Dodać TimeService i NTP.** Depends: NET-01, SENS-01, CFG-01, WEB-01.
  - SNTP, UTC/timezone, DS3231/GNSS quality arbitration, holdover i telemetry.
  - Done: brak blokowania IO bez sieci, valid/invalid source i time jumps
    testowane; Settings/Diagnostic pokazują stan i sekrety nie są potrzebne.

- [ ] **SCHED-01 — Dodać cron/interval/one-shot dla IFTTT.** Depends: TIME-01, IFTTT-01, WEB-02.
  - Parser 5 pól, preview, due events, DST/misfire/dedupe, trwałość one-shot.
  - Done: fake clock testuje rok przestępny, DOM/DOW OR, brak czasu,
    DST PL, NTP jumps, restart i queue failure; brak powtarzania co loop.

### Etap F — nowe IO, sensory i wielostronicowy OLED

- [ ] **RELAY-01 — Wdrożyć RAM/NVS i startup policy.** Depends: IO-01, CFG-01, API-01.
  - StateStore, wersjonowany snapshot, sekwencja startu, tryb/storage errors i UI.
  - Done: wszystkie kombinacje z rozdziału 10 testowane, zapis wyłącznie po skutecznej zmianie, no-op bez flash.

- [ ] **DHT-01 — Dodać DHT11.** Depends: SENS-01, MIG-01, WEB-01.
  - Typ 3 legacy, prawdziwa rekonfiguracja modelu, schema i property TTL.
  - Done: DHT22/DS18B20 zachowane, zmiana modelu nie zostawia starej wilgotności, test timing i błędów.

- [ ] **DAC-01 — Udostępnić dwa kanały DAC.** Depends: IO-01, BOARD-01, WEB-01.
  - SetRaw/setPercent, rezerwacje i UI IO.
  - Done: zakres 0/255 i zaokrąglenie percent, konflikt beeper/DA-1, dostępność w IFTTT bez whitelist.

- [ ] **PWM-01 — Dodać PWM Fan.** Depends: IO-01, BOARD-01, WEB-01.
  - Shared PWM manager, tryby sprzętowe, duty/frequency i ewentualny tach.
  - Done: brak konfliktu timerów i GPIO, pomiar przebiegu, jawny startup/fail-safe zatwierdzony dla wybranego wentylatora.

- [ ] **LED-01 — Dodać WS2812.** Depends: CORE-02, BOARD-01, CFG-01, WEB-01.
  - Wybór biblioteki, bufory, kolor/jasność/efekty/segmenty.
  - Done: limit LED/pamięci i frame time zmierzony; standardowe akcje widoczne w IFTTT, brak blokady IO przy efekcie.

- [ ] **OLED-02 — Dodać SSD1309 i SH1107.** Depends: OLED-01, BOARD-01, WEB-02.
  - Potwierdzone modele/geometrie i adaptery; selector Screen.
  - Done: te same strony działają na wszystkich kontrolerach, właściwe offsety/obrót, init error nie zatrzymuje aplikacji.

- [ ] **GRID-01 — Dodać wielostronicowy grid OLED i edytor WebGUI.** Depends: OLED-01, OLED-02, WEB-02, CFG-01, DISCOVERY-01.
  - Lista 1–8 stron, indywidualne grids/widgets, stable IDs i typed bindings
    do danych sensorów/interfejsów; drag+keyboard, preview, persistence/migration.
  - Done: geometria/overlap/min-size, reorder/delete z IFTTT, unavailable
    sources i interface data testowane; zapis mieści się w zatwierdzonym
    storage budget, rendering działa na wspieranych kontrolerach.

### Etap G — opcjonalne protokoły i peryferia rozszerzone

- [ ] **UDP-01 — Dodać UDP.** Depends: NET-01, CORE-02, WEB-01.
  - Bounded datagram RX/TX, source filter/rate limit i deskryptory.
  - Done: dwa identyczne datagramy wyzwalają dwa eventy, oversize jawny, send nie udaje delivery.

- [ ] **TCP-01 — Dodać TCP.** Depends: NET-01, CORE-02, SER-01, WEB-01.
  - Wykorzystać wspólny helper framing; client/server, bounded reconnect, jeden klient server.
  - Done: segmentacja i sklejenie ramek niezależne od read, partial write/error i Wi-Fi disconnect.

- [ ] **MQTT-01 — Dodać MQTT.** Depends: NET-01, CORE-02, WEB-01.
  - Broker/auth/TLS, sub/publish, retained policy, LWT i brak nieograniczonej offline queue.
  - Done: broker reconnect/TLS failure/retain/duplikaty/limity, sekrety niewidoczne w API i logach.

- [ ] **HTTP-01 — Dodać webhook i outbound request.** Depends: NET-01, API-01, CORE-02, WEB-01.
  - Endpoint catalog, scopes, schemas, async operations i response events.
  - Done: brak webhooku bez auth, brak sterowania URL przez payload, bounded body/timeout i brak secret forwarding.

- [ ] **CELL-01 — Dodać SIM7600 i cellular transport.** Depends: HW-01, NET-01, SER-01, WEB-01.
  - Wybrać sockety AT lub PPPoS, lifecycle URC, APN/SIM/auth, routing/failover
    i modem supply/interface.
  - Done: realny modem connect/reconnect, command/data arbitration,
    utrata SIM/sieci i bounded buffers; IFTTT/API/GUI korzystają z capabilities.

- [ ] **GNSS-01 — Dodać źródło GNSS modemu i jawny wybór GPS.** Depends: CELL-01, SENS-01, TIME-01.
  - Wersja AT/GNSS, data quality, source IDs, priority bez utraty legacy GPS.
  - Done: fix/UTC validity rozdzielone; PPP i odczyt GNSS nie korumpują UART.

- [ ] **MESH-01 — Dodać Meshtastic bridge.** Depends: HW-01, SER-01, TCP-01, WEB-01, COMM-01.
  - Client API/protobuf, bounded nodes/history, message actions/events
    i jawnie wspierane config parameters.
  - Done: drugi węzeł działa end-to-end, framing resync i schema/version
    errors testowane, brak udawanego mesh przez raw LoRa.

- [ ] **ZB-01 — Dodać wybraną integrację Zigbee.** Depends: HW-01, SER-01, CORE-02, WEB-01.
  - Konkretny NCP/RCP/host stack, role, supported clusters, inventory i actions.
  - Done: join/report/command/leave z testowym urządzeniem; stan/sekrety,
    limity i firmware compatibility widoczne, rola radio rzeczywiście działa.

- [ ] **THREAD-01 — Dodać wybraną integrację Thread.** Depends: HW-01, SER-01, NET-01, CORE-02, WEB-01.
  - Endpoint/gateway/BR i application protocol jawne; dataset i resource budgets.
  - Done: realna sieć IPv6 i wybrana wiadomość aplikacyjna→IFTTT→akcja;
    brak obietnicy Matter bez stosu Matter i brak niepotwierdzonego dual-radio.

- [ ] **AUDIO-01 — Dodać audio w zatwierdzonym zakresie.** Depends: HW-01, BOARD-01, CORE-02, CFG-01, WEB-01.
  - Kodek I2C/I2S albo jawny moduł playback, DMA, buffer i profile.
  - Done: playback/stop/volume/events, realne sample format i underrun;
    I2C relays/IFTTT działają podczas audio, pamięć zmierzona.

- [ ] **CC-01 — Dodać CC1101 i katalog wybranych protokołów.** Depends: HW-01, BOARD-01, COMM-01, WEB-01.
  - Verify whether CC1101 can use the RF connector and signal path intended for
    RF24 on the selected PCB revision. Confirm connector pinout, logic levels,
    SPI/CS/GDO routing, antenna/band compatibility, and exclusive ownership;
    an RF24-compatible connector must not be assumed to make the radio a
    drop-in replacement or permit concurrent use of shared resources.
  - Provide bounded packet monitoring and history, with explicit selection,
    naming, save/delete and persistence of user-selected supported frames.
    Publish received supported packets as discrete capability events so they
    can later be selected as IFTTT triggers. Expose transmit actions only for
    explicitly supported, authorized protocols and devices; replay of saved
    fixed-code frames must be clearly distinguished from secure rolling-code
    operation.
  - Support rolling-code pairing only through a verified, manufacturer-defined
    enrollment procedure that preserves the protocol's security. Do not clone,
    replay, predict or bypass rolling codes; if secure enrollment is not
    implemented for a protocol, report it as unsupported and never present a
    captured frame as a usable paired command. Protect any enrollment secrets
    from UI/API exports and logs, and require an appropriate storage design
    before enabling such a protocol.
  - Add an optional repeater role only for a specifically supported,
    security-preserving protocol and after hardware/regulatory validation.
    Repeater mode is mutually exclusive with the monitor, packet-history,
    normal TX/RX, pairing and IFTTT roles for that radio; switching roles must
    be explicit and report which capabilities become unavailable. No generic
    capture-and-retransmit repeater for rolling-code or access-control signals.
  - Done: test with an authorized device for the supported monitor/save/event
    and transmit path, bounded framing/history/overflow and resource conflicts
    with LoRa/PN532; verify exclusive repeater-role behavior and its supported
    protocol end-to-end. Demonstrate secure rolling-code enrollment only if
    the selected vendor protocol and implementation support it; otherwise
    verify that pairing/replay is rejected and reported unsupported.

- [ ] **IR-01 — Dodać IR TX/RX pilotów.** Depends: HW-01, BOARD-01, CORE-02, WEB-01.
  - RMT, LED driver/receiver, decoder whitelist, repeats i learned frames.
  - Done: realny send/decode, zakres długości/duty/carrier i kolizje WS2812
    testowane; ISR nie uruchamia logiki IFTTT.

- [ ] **IRDA-01 — Potwierdzić i wdrożyć wymagany wariant IrDA.** Depends: HW-01, SER-01, CORE-02, WEB-01.
  - Najpierw peer/transceiver/SIR lub full stack i dostępność implementacji.
  - Done: wymiana na wymaganej warstwie z rzeczywistym peer; jeśli brak
    stosu, raport blocked/prototype, nie „pełne IrDA” po samym UART echo.

### Etap H — integracja, testy sprzętowe i wydanie

- [ ] **FINAL-01 — Zweryfikować end-to-end i zaktualizować dokumentację.** Depends: WEB-02, DIAG-01, RELAY-01, DHT-01, DAC-01, PWM-01, LED-01, OLED-02, SER-01, UDP-01, TCP-01, MQTT-01, HTTP-01, DISCOVERY-01, GRID-01.
  - Build matrix, RAM/flash, testy obciążenia i power cycle na bezpiecznym stanowisku.
  - README, instrukcja dodawania modułu, API schemas, pin/revision notes i granice kompatybilności.
  - Zaktualizować istniejące metadata/manifests paczki (package.json), scripts/profiles
    i informacje o assets zgodnie z rzeczywistym zakresem wydania.
  - Done: komplet macierzy akceptacji poniżej z dowodami, nie samo „build passed”.

- [ ] **P17TEST-01 — Zweryfikować mostek i workload na sprzęcie.** Depends: P17-01, PEER-01, BRIDGEUI-01, PROFILE-01.
  - Połączenia/zasilanie/straps, obciążony full-duplex, peer reset/host NVS
    writes/reconnect, local IFTTT i równoległy drugi moduł.
  - Done: raport wykorzystania linku/RAM/flash, poprawny recovery,
    co najmniej 30 min smoke i 24 h soak profilu produkcyjnego; oczekiwane
    „wsparcie” targetu wymaga testów tego targetu.

- [ ] **EXT-01 — Zweryfikować profile rozszerzone i budżety.** Depends: FINAL-01, HW-01, SCHED-01, GNSS-01, MESH-01, ZB-01, THREAD-01, AUDIO-01, CC-01, IR-01, IRDA-01, RTOS-01.
  - Dla wybranych wariantów hardware: fresh build matrix, dependencies,
    OTA/assets layout, concurrent workload i 24 h soak.
  - Done: raport funkcji supported/excluded per profil i pomiary flash/RAM/
    task/bus/power; opcje niewybrane formalnie wyłączone z manifestu zakresu.
    Zależność do zadania niewybranego nie jest „done”: skorygować zakres
    EXT-01 i jego dependencies jawnie przed rozpoczęciem integracji.

### Obowiązkowy ostatni krok każdego zadania/przyrostu

Po implementacji i weryfikacji zaktualizować README, status tego ToDo oraz
dane paczki/projektu: istniejący package.json i inne istniejące manifests,
jeżeli zmiana ich dotyczy. Uzgodnić opis funkcji, scripts, wymagania narzędzi,
profile, dependencies i sposób budowy/wgrywania assets. Nie tworzyć fikcyjnego
ipackage.json, nie podbijać wersji bez decyzji o wydaniu i nie zmieniać
partycji/zależności bez potrzeby i planu migracji. Sprawdzić JSON, lokalne
linki i nowe scripts; opisać kompatybilność, ograniczenia oraz faktyczne
wyniki testów. Jest to warunek zamknięcia każdego przyrostu, nie dodatkowe
zadanie omijające zależności.

### Wdrożenie i podział dokumentacji — 2026-10-31

README skrócono do wprowadzenia i nawigacji; dotychczasowe treści zachowano
w pięciu rozdziałach Markdown w docs/ oraz osobnym indeksie. Bez dodatkowych
zależności generatora dokumentacji. Package metadata wskazuje docs/;
sprawdzono wszystkie lokalne linki i JSON.

Po zgodzie użytkownika i potwierdzeniu odłączenia niebezpiecznych obciążeń
zweryfikowano `npm test` (25 grup native, 4 DOM) i dwa aktualne profile build.
Końcowo wgrano **kc868-a6 bez coprocessor** na /dev/ttyUSB0, zastępując
wcześniejszy upload coprocessor; firmware 1 461 760 B, hash flash verified.
Assets LittleFS również wgrane i zweryfikowane; NVS nie kasowano.
Serial potwierdził start, OLED/RTC, LittleFS i Wi-Fi. Lokalne HTTP checks:
status configuration ready/4 ADC, board valid/26 GPIO, settings UART1 mode=0
baud=9600 oraz aktualny index z tabelą diagnostyki. Nie oznacza to ukończenia
planu ani testów fizycznych wszystkich peryferiów/power-loss.

## 14. Walidacja i kryteria akceptacji

### 14.1. Minimalne testy jednostkowe / kontraktowe

| Obszar | Wymagany przypadek |
| --- | --- |
| Registry | Duplikat ID, brak optional interface, wyłączenie modułu, binding stale po zmianie typu, unsupported build. |
| IO | Active-low, kanały graniczne 1/6, odrzucenie 0/7, P6/P7 zachowane, I2C error, no-op, readback discrepancy. |
| ADC | Dokładny JSON dla 0, 255, 256, 2048, 4095; brak obcięcia do uint8. |
| Sensors | Timing DHT11/22, DS18B20 conversion, invalid/stale/warming_up, zmiana modelu i brak starego pomiaru. |
| IFTTT | False→true raz, powrót i ponowny trigger, histereza/hold/cooldown, identyczne payloady z różnymi ID, disabled/suspended. |
| Multi-action | Kolejność, async completion, timeout, częściowy sukces, stop-on-error, queue overflow i feedback bez rekurencji. |
| Config/migracje | V1→V2, idempotencja, interrupted write, invalid CRC/length, newer schema, RF24 saved ID. |
| RAM/NVS | Cold boot/reset, snapshot missing/corrupt, mode switch, no-op bez zapisu, write failure po applied, OFF init nie kasuje restore. |
| Framing | Rozbita ramka, wiele ramek w read, delimiter/fixed/length-prefix, oversize, idle timeout, bytes bez NUL terminatora. |
| API/UI | Schemy success/error, statusy HTTP, actions unavailable, secret redaction, dynamic nav, lifecycle/timers i PL/EN. |
| OLED | Mock canvas: layout/geometria/rotation, filtrowanie stron, custom fields i nieobsługiwany parametr sterownika. |
| ResourceManager | Wspólny CS5, UART1, PN532/I2C0x24, DA-1/beeper/PWM, input-only i piny strapping. |
| Network/mDNS | Hostname validation, DHCP/static/DNS, actual service port/TXT, collision, reconnect, disabled responder, fallback IP i brak sekretów w discovery. |
| OLED grid/pages | 1/8 stron i odrzucenie 0/9 przy enabled display, independent grid, spans/bounds/overlap, pixel rounding/rotation, reorder stable ID i delete suspension. |
| Interface bindings | Network/IP/RSSI, MQTT/UART/bridge data i invalid/stale rendering; brak sync IO podczas frame i brak sekretów na ekranie. |

### 14.2. Testy sprzętowe i integracyjne

- Przekaźniki testować bez niebezpiecznych obciążeń. Dla każdej maski 0–63 sprawdzić logiczne mapowanie i restore poprawnie utrwalonego snapshotu.
- Power cycle: RAM zawsze OFF; NVS restore po zakończonym zapisie; zanik w trakcie zapisu daje poprzedni poprawny snapshot albo jawny safe OFF, nigdy losową maskę.
- Zmierzyć glitch/startup wyjść; jeśli brak gwarancji sprzętowej, zapisać ograniczenie, nie zaznaczać „bez impulsów”.
- DHT11/DHT22/DS18B20 na obu gniazdach, odłączenie i ponowne podłączenie.
- SSD1306 oraz konkretne moduły SSD1309/SH1107: pełny ekran, wszystkie strony, obrót 0/90/180/270.
- WS2812 przy zadeklarowanym maksimum LED, efekty wraz z IO/IFTTT i BLE/network.
- PWM: oscyloskop/analizator potwierdza częstotliwość i duty; stop/fail-safe odpowiedni do wybranego wentylatora.
- DAC: wartości raw i konflikt beeper; pomiar napięcia z rozróżnieniem GPIO i złącza płytki.
- RS-232/RS-485: poprawne konwertery, half-duplex i kierunek transceivera.
- UDP/TCP z drugim hostem, MQTT z brokerem testowym, HTTP z kontrolowanym serwerem; błędy DNS/TLS/auth i Wi-Fi restart.
- LoRa/RF24 z drugim modułem zgodnym z profilem; nie oceniać TX wyłącznie na podstawie lokalnego bool.

### 14.3. Proponowane progi wydajności do sprawdzenia

To docelowe kryteria, nie wyniki obecnego firmware. BASE-01 ma wyznaczyć baseline; jeśli dany stos ich nie spełnia, opisać pomiar i konieczną zmianę architektury.

- Co najmniej 20% wolnej partycji aplikacji w każdym wspieranym profilu po finalnym buildzie.
- Dla zadeklarowanego profilu maksymalnego: minimum 40 KiB wolnego heap i największy blok co najmniej 16 KiB; sprawdzić także handshake TLS.
- 30 minut pracy z ruchem transportów i pollingiem UI: bez resetu watchdog, bez narastania zajętości heap po rozgrzaniu.
- Latencja lokalnego eventu DI/S2 → rozpoczęcie akcji: p95 ≤ 100 ms po debounce, bez doliczania jawnego hold/cooldown.
- Pojedynczy cooperative tick: p99 ≤ 20 ms; dłuższe operacje wydzielone lub jawnie ograniczone i pomierzone.
- Przy przeciążeniu: bounded queue i jawne drop/error, nie brak responsywności lub nieograniczony RAM.
- Dla NVS ACK utrwalenia uwzględnia czas flash; nie maskować go jako krótszej latencji. Zmierzyć oddzielnie applied i persisted.

### 14.4. Test rozszerzalności — właściwy cel refaktoryzacji

Dodać testowy moduł z:

- jedną właściwością number z jednostką,
- jednym discrete trigger,
- jedną akcją z parametrem enum,
- jedną opcją konfiguracji.

Moduł musi pojawić się w catalog/capabilities/state, dostać ogólny widget, formularz i pozycje kreatora IFTTT po **samej rejestracji w composition root**. Nie wolno w tym teście dopisywać case do engine, API routera, centralnego HTML ani tablicy źródeł JS.

Nowy typ niestandardowego widgetu może wymagać zarejestrowania renderera; nie może wymagać powielenia transportu i walidacji.

## 15. Instrukcja wykonywania pojedynczego zadania przez agenta

1. Przeczytaj ten dokument oraz rzeczywisty kod; dokument nie zastępuje aktualnego stanu workspace.
2. Wybierz jedno ID ToDo i sprawdź wszystkie `Depends`. Nie deklaruj zależności jako gotowej bez testów.
3. Przed edycją opisz zakres, zachowanie zachowane i świadomie zmieniane.
4. Poszukaj istniejących helperów i kontraktów. Nie twórz drugiej reprezentacji tej samej schema/ID/pinoutu.
5. Implementuj pionowo: driver/service → capability → akcja/trigger → API → UI → test, jeśli zadanie obejmuje nowy moduł.
6. Dodaj testy błędów i granic, nie tylko happy path.
7. Uruchom najmniejsze testy pokrywające zmianę i właściwy build PlatformIO. Build all profiles na integracji.
8. Aktualizuj bezpośrednio powiązaną dokumentację i stan konkretnego checkboxa dopiero po weryfikacji.
9. Zapisz rezultat: pliki, testy/komendy, wyniki, pomiary, ograniczenia hardware i niedokończone elementy.
10. Nie oznaczaj zadania jako ukończonego, jeśli wymaga sprzętowego potwierdzenia, którego nie wykonano; oznacz „kod gotowy, hardware validation pending”.

### Punkty wymagające zatwierdzenia przed użyciem sprzętowym

- Rewizja PCB, mapowanie LoRa i fizyczna numeracja złączy.
- Dostępne połączenia/GPIO dla WS2812, PWM fan i opcjonalnego tachometru.
- Typ wentylatora, stopień sterujący, częstotliwość i fail-safe/startup duty.
- Konkretne moduły SSD1309/SH1107, bus/geometria/zasilanie/reset.
- RS-485 auto-direction albo DE/RE; poziomy elektryczne GPS/coprocessor/RS-232.
- Dopuszczalność automatycznego restore przekaźników ON dla rzeczywistych obciążeń.
- Maksymalne instancje/payload/LED, TLS library i ewentualne rozszerzenie QoS/limitów IFTTT.

Nie zgadywać tych danych. Niezależne zadania rdzenia, migracji i UI mogą być realizowane wcześniej z mockami oraz statusem `unverified`.

## 16. Rozszerzony zestaw protokołów — wykonalność i wybór sprzętu

### 16.1. Krótka odpowiedź

**Na klasycznym ESP32-WROOM-32 zmieści się wybrany zestaw sterowników i
integracji, ale nie należy projektować pełnego, jednocześnie aktywnego
„wszystko w jednym” z rozbudowanym WebGUI, audio, LTE, mesh oraz bramami
Zigbee/Thread bez dodatkowej pamięci i koprocesorów.**

Trzeba rozróżnić:

1. Sterownik układu radiowego, np. SX127x lub CC1101.
2. Klienta/bramkę do urządzenia z własnym stosem, np. Meshtastic przez Client API.
3. Pełny stos uruchomiony na głównym MCU, np. Meshtastic firmware lub OpenThread Border Router.
4. Obsługę aplikacji nad protokołem, np. klastry Zigbee, profile IR, Matter nad Thread.

Wariant 2 jest zwykle znacznie lżejszy niż 3. „Konfigurowalne w WebGUI” znaczy
wszystkie **zaimplementowane i opublikowane** parametry danego adaptera,
nie automatyczną obsługę każdego protokołu, urządzenia i rozszerzenia standardu.

„ZIGBY” interpretujemy jako Zigbee, „NFC reder” jako czytnik NFC, „IrDATA”
roboczo jako IrDA. Jeśli IrDATA oznacza inny protokół, zakres IRDA-01 wymaga
doprecyzowania. „I2C audio” nie jest automatycznie I2S: I2C często konfiguruje
kodek, a dane audio płyną przez I2S. W dokumencie przewidziano te dwie osobne
warstwy, bez zgadywania konkretnego modułu.

### 16.2. Co obecnie wiadomo o pamięci

- Klasyczny ESP32 ma do 2 rdzeni 240 MHz i 520 KiB wewnętrznego SRAM łącznie.
  To nie jest 520 KiB wolnego heap: pamięć dzielą kod IRAM, dane, stosy,
  Wi-Fi/BLE, biblioteki i bufory.
- Dla opisanego ESP32-WROOM-32 nie zakładamy PSRAM. Potwierdzić oznaczenie
  rzeczywistego modułu, flash oraz konfigurację w zadaniu HW-01.
- Bieżąca tablica partycji odpowiada layoutowi mieszczącemu się w 4 MiB:
  dwa sloty OTA po 1,5 MiB i LittleFS 896 KiB. To dowód konfiguracji, nie
  odczyt fizycznej pojemności układu flash.
- Dwa rdzenie współdzielą RAM, flash i magistrale. Drugi rdzeń nie daje
  drugich 520 KiB ani drugiego niezależnego zestawu peryferiów.

Pomiar istniejących plików builda wykonany 2026-10-31, bez przebudowy:

| Profil / obraz | Rozmiar firmware.bin | Zajętość slotu 1 572 864 B | Wolne w slocie |
| --- | --- | --- | --- |
| `kc868-a6` | 1 447 440 B ≈ 1,38 MiB | ≈ 92,0% | 125 424 B ≈ 122,5 KiB |
| `kc868-a6-nrf24` | 1 450 864 B ≈ 1,38 MiB | ≈ 92,2% | 122 000 B ≈ 119,1 KiB |

Obrazy w [.pio/build/](./.pio/build/) mają daty modyfikacji odpowiednio
12:10 i 12:11 tego dnia. Nie stanowią potwierdzenia świeżego builda wszystkich
profili ani pomiaru aktualnie wgranego urządzenia. Rozmiar obrazu obejmuje
kod i dane umieszczone we flash; runtime heap trzeba zmierzyć na urządzeniu.
Zwykłe sumowanie kolumn `text/data/bss` narzędzia `size` dla ESP32 nie daje
wiarygodnego wyniku wolnego RAM; analizować sekcje/linker map i heap runtime.

**Obecny obraz już nie spełnia docelowego zapasu 20% z rozdziału 14.3.**
Niewielkie NTP i harmonogram są realnym rozszerzeniem, ale większe biblioteki
mogą przekroczyć slot OTA jeszcze przed problemami z RAM.

### 16.3. Orientacyjne koszty — nie wyniki kompilacji

Poniższe widełki są budżetami projektowymi rzędu wielkości dla typowych,
ograniczonych implementacji. Nie są pomiarem tego repozytorium ani sumą
gwarantującą pojemność. Zależności współdzielone, wersja IDF, optymalizacja,
TLS, rozmiar historii i wybrana rola protokołu bardzo zmieniają wynik.

`Δ flash` oznacza przybliżony przyrost względem buildu mającego już podstawową
sieć/UI; `RAM` oznacza przybliżone dodatkowe bufory/stosy/stan podczas pracy.
Nie doliczać ponownie biblioteki już obecnej w baseline.

| Funkcja / ograniczony wariant | Δ flash — budżet | Dodatkowy RAM — budżet | Najważniejsze zastrzeżenie |
| --- | --- | --- | --- |
| NTP/SNTP + cron 5 pól, 16 harmonogramów | 10–60 KiB | 2–12 KiB | SNTP może już być częściowo linkowany; bez ogromnej bazy stref. |
| GPS/GNSS NMEA | 10–50 KiB | 2–10 KiB | TinyGPSPlus już jest w projekcie; nowy driver może dodać niewiele. |
| SIM7600: AT + modemowe sockety | 40–200 KiB | 8–40 KiB | Koszt rośnie z komendami, parserem URC i buforami. |
| SIM7600: PPPoS/netif | 100–350 KiB | 25–100 KiB | TLS aplikacji osobno; CMUX i reconnect wymagają testów. |
| Jedna aktywna sesja TLS | 100–400 KiB, jeśli nie ma stosu w baseline | 30–100 KiB lub więcej w szczycie | Certyfikaty, rekordy TLS, handshake; nie mnożyć flash przez liczbę połączeń, ale RAM może się mnożyć. |
| LoRa raw driver/protocol bounded | 20–100 KiB | 2–15 KiB | Biblioteka LoRa już jest; brak kosztu całego mesh w tym wierszu. |
| Meshtastic Client API bridge | 60–250 KiB | 10–80 KiB | Protobuf, wersje schema, ograniczona baza węzłów/wiadomości. |
| Pełny Meshtastic na głównym MCU | Osobny profil rzędu 1–3 MiB całego obrazu | Rząd 100–300 KiB wewnętrznej pamięci lub więcej, plus wybrane dane w PSRAM | To osobny rozbudowany firmware; nie traktować zakresu jako zmierzonego przyrostu do obecnej aplikacji. |
| Zigbee host adapter do pełnego NCP | 50–250 KiB | 10–80 KiB | Tylko ograniczona liczba urządzeń i klastrów; stos i baza mogą być na NCP. |
| Thread adapter do zewnętrznej bramy aplikacyjnej | 30–200 KiB | 5–50 KiB | Nie jest hostowym OpenThread ani Border Routerem. |
| OpenThread host + RCP / Border Router | 200–900 KiB | 50–200 KiB lub więcej | RCP przenosi radio, nie cały stos; NAT64/IPv6/usługi dodatkowo obciążają hosta. |
| PN532 UID/obecność NFC | 20–100 KiB | 2–15 KiB | Biblioteka już jest; pełne NDEF/auth/emulacja to inne funkcje. |
| I2C control + I2S PCM bez dekodera | 30–150 KiB | 8–64 KiB | Zależność od DMA i długości buforu. |
| Audio streaming/dekodery | 150–800 KiB lub więcej | 50–250 KiB lub więcej, plus bufory PSRAM | Kodeki, HTTP/TLS i jitter buffer nie mieszczą się w kosztach samego I2S. |
| CC1101 raw + kilka jawnych profili | 20–120 KiB | 2–20 KiB | Katalog wielu dekoderów może dodać setki KiB. |
| IR TX/RX: ograniczony zestaw dekoderów | 30–200 KiB | 2–20 KiB | Klimatyzatory i rozbudowana baza protokołów zwiększają flash. |
| IrDA SIR z ograniczonym framing | 20–120 KiB | 4–32 KiB | Pełne IrLAP/IrLMP/OBEX to inna skala i brak gwarancji gotowej biblioteki. |
| Dodatkowy web panel z formularzami/diagramami | 100–800 KiB assetów LittleFS | Zależny od JSON/HTTP; większość DOM działa w przeglądarce | Assety nie są heap ESP32; duży JSON i sesje HTTP już tak. |

Scenariusze do planowania zakupów, nie obietnice builda:

- Ograniczony kontroler z wybranymi sterownikami, NTP, harmonogramem i jednym
  cięższym transportem: obraz około **1,5–2,5 MiB** jest rozsądną rezerwą.
  Obecne sloty OTA 1,5 MiB są za ciasne na taki zakres bez selekcji funkcji.
- Gateway z LTE, wieloma adapterami, TLS oraz Client API Meshtastic i
  zewnętrznymi stosami: rezerwować **2–4 MiB na aplikację**, PSRAM i
  przynajmniej 16 MiB flash na dwa sloty OTA, assets i zapas.
- Wszystkie pełne stosy na jednym MCU, duży katalog protokołów i streaming
  audio: roboczo **3–6 MiB lub więcej aplikacji**, a pamięć runtime może
  przekraczać budżet wewnętrznego RAM. Nie gwarantować działania nawet na S3;
  oddzielne procesory lub gateway Linux mogą być lepszym rozwiązaniem.

Sam większy flash pozwala zachować więcej kodu, ale nie rozwiązuje braku
heap, pinów, radia 802.15.4 ani czasu na obsługę magistral.

### 16.4. Warianty rozbudowy sprzętu

| Wariant | Zalety | Ograniczenia / ryzyko |
| --- | --- | --- |
| Zachować obecny MCU, profile funkcjonalne i zewnętrzne pełne moduły | Najmniej zmian płytki; pełny Meshtastic/Zigbee można wynieść poza host | Mało zapasu flash/RAM i wolnych UART/GPIO; często nie wszystko równolegle. |
| Klasyczny ESP32 z większym flash | Potencjalnie zachowanie peryferiów klasycznego ESP32, DAC i części zgodności | Potwierdzić konkretny moduł, footprint, pinout i wymagania zasilania; większy flash ≠ PSRAM. |
| ESP32-WROVER / klasyczny ESP32 z PSRAM | Więcej miejsca na dane; klasyczny rdzeń i DAC | Nie zakładać mechanicznego drop-in. GPIO16/17 mogą być zajęte przez PSRAM, a KC868-A6 używa ich dla UART1. |
| ESP32-S3, np. moduł 16 MiB flash + 8 MiB PSRAM | 2×240 MHz, więcej opcji buforowania i peryferiów; dobry host UI/LTE/audio/adapterów | Nie jest zamiennikiem WROOM-32 1:1. Brak wbudowanego DAC i radia 802.15.4; inny pinout i mapa ADC. |
| ESP32-C6 jako kontroler główny | Wi-Fi/BLE + 802.15.4 dla Zigbee/Thread | Jeden główny rdzeń aplikacyjny, nie upgrade do 2 mocniejszych rdzeni; wariant pamięci i stosów wymaga oceny. Nie drop-in. |
| S3 jako host + C6/H2/NCP/RCP + osobny węzeł Meshtastic | Rozdzielenie UI/IO od stosów RF, mniejsza integracja firmware hosta | Więcej modułów, połączeń, zasilania i firmware do wersjonowania; role RCP/NCP muszą być jawne. |
| Zewnętrzny gateway Linux + MCU do deterministycznego IO | Najłatwiejsze pełne protokoły, bogate UI, audio i rozbudowana baza urządzeń | Większy pobór mocy/koszt, system plików i lifecycle OS; MCU musi działać bez gatewaya. |

**Rekomendacja:** dla planowanego bogatego kontrolera rozpocząć projekt od
ESP32-S3 z 16 MiB flash / 8 MiB PSRAM jako hosta i odrębnych stosów
Zigbee/Thread/Meshtastic. Zachowanie KC868-A6 jako modułu IO i dodanie hosta
może być mniej ryzykowne niż wylutowanie ESP32. Przy wymaganiu pełnej bramy
Zigbee + Thread + audio jednocześnie rozważyć gateway Linux.

Migracja S3 wymaga adaptera lub nowego PCB/okablowania i nowego `BoardProfile`.
Na przykład aktualne A1–A4 na GPIO36/39/34/35 nie mogą zostać bezrefleksyjnie
przeniesione jako ADC S3; DA-1/DA-2 wymagają zewnętrznego DAC i dopasowania do
toru płytki. Sprawdzić również piny flash/PSRAM, boot straps, poziomy,
programowanie, antenę, regulatory i wszystkie przekaźniki przy starcie.
Nie podawać instrukcji wymiany modułu bez schematu adaptera i weryfikacji.

### KC868-Ax profile acceptance

The PlatformIO aliases `kc868-a4`, `kc868-a8`, `kc868-a8s`, `kc868-a16`,
`kc868-a32`, `kc868-a64` and `kc868-a128` currently compile the A6 baseline
with a model label; they do not have model-specific GPIO/IO mappings.
Diagnostics marks those aliases as pinout-unvalidated. Before treating any
alias as a supported board profile, obtain the exact PCB revision schematic,
define its relay/input/ADC/UART/I2C/SPI/DAC roles and reservations, then add a
profile-specific resource validator and tests against the schematic. The
manufacturer KCS model list alone is not electrical pinout evidence.

PSRAM przeznaczać na historię, duże modele/JSON i bufory zgodne z driverem.
Nie zakładać, że każdy stos, DMA, ISR i struktura Wi-Fi może znajdować się
w PSRAM. Mierzyć osobno free/min/largest **internal heap**, DMA-capable heap
i PSRAM. 8 MiB PSRAM nie jest 8 MiB równie szybkiego SRAM.

## 17. Rozszerzone adaptery i konfiguracja protokołów

### 17.1. SIM7600 i źródła GNSS

- `CellularPeripheral`: lifecycle, health, AT/URC state machine, rejestracja
  sieci, SIM status, operator, sygnał, APN, tryb połączenia i ostatni błąd.
- Opcje transportu: modemowe sockety **albo** PPPoS jako netif hosta.
  Wybrać świadomie jeden wariant pierwszego profilu. PPP ułatwia reuse
  TCP/MQTT/HTTP/NTP przez wspólny stos, ale kosztuje RAM i wymaga wsparcia
  używanego frameworka; sockety AT nie są automatycznie normalnym `WiFiClient`.
- NetworkService rozróżnia Wi-Fi i cellular, jawny priorytet/failover,
  route i reconnect. Nie zmienia trasy aktywnych połączeń bez ich ponownego
  zestawienia. UI jest nadal dostępne lokalnie; LTE zwykle ma CGNAT,
  więc brak automatycznej dostępności panelu z internetu.
- Konfiguracja: wariant modemu, UART/USB zgodnie z faktycznie wspieraną
  implementacją, baud/flow control, APN/user/password, SIM PIN, polityka
  połączenia i limity reconnect. PIN i dane operatora traktować jako sekrety.
- AT i PPP na jednym UART wymagają sprawdzonego CMUX, osobnego kanału lub
  kontrolowanego przejścia trybu. Nie wysyłać AT w strumień PPP.
- GNSS SIM7600 jest opcją zależną od wariantu/firmware/anteny. Udostępnić
  go jako osobne źródło obok zewnętrznego GPS; UID źródła i priorytet jawne.
- GPS: fix/age/satellites/coordinates/UTC; nie publikować starej lokalizacji
  jako valid. Geofence wymaga osobnego filtru z histerezą i jakością fix.
- SIM7600 potrzebuje odpowiedniego zasilania dla impulsów prądu LTE,
  anteny i właściwego interfejsu logicznego. Goły moduł może mieć UART
  o innych poziomach niż ESP32; breakout z konwerterem to inny przypadek.
  Nie zasilać przez GPIO ani zakładać wydajności regulatora KC868-A6.
- IFTTT: `registered`, `connected`, `disconnected`, `messageReceived` dla
  opcjonalnego SMS i jawne akcje connect/disconnect/SMS, jeśli wdrożone.
  SMS/call nie są podstawową obietnicą pierwszego profilu LTE data.
- WebGUI: Interfaces→Cellular/GNSS lub osobna strona z rejestru;
  pełna walidacja parametrów i bezpieczne redaction secrets.

### 17.2. Meshtastic

- Rekomendowany `MeshtasticBridgePeripheral` jest klientem zewnętrznego
  węzła Meshtastic przez serial/TCP Client API. BLE API jest osobnym
  wariantem i wymaga arbitrażu z bieżącymi trybami BLE hosta.
- Stosować wersjonowane protobuf/nanopb i opisane framing/resynchronization,
  nie wysyłać zwykłego tekstu do SX1278 z etykietą „Meshtastic”.
- Minimalny zakres: status węzła, ograniczona baza nodes, odbiór wiadomości,
  telemetry/position, wysłanie wiadomości, konfiguracja jawnie wspieranych
  parametrów channel/region/modem preset/rola przez API urządzenia.
- Klucze kanałów i uprawnienia do remote configuration są sekretami.
  Nie deklarować możliwości zdalnego administrowania bez ustawionych
  uprawnień i wsparcia konkretnej wersji urządzenia.
- IFTTT: message/telemetry/node availability; akcja sendText/wybrany
  packet type. Local accepted, radio ACK i aplikacyjne delivery to różne stany.
- Limit bazy nodes/history/payload, rate limiting i regionalne duty cycle.
- Alternatywa pełnego Meshtastic na głównym MCU jest oddzielnym profilem
  i projektem integracji, nie „kolejną biblioteką LoRa”. Sprawdzić wersję,
  target, współdzielenie schedulerów/NVS/UI i warunki licencji przed
  dystrybucją zintegrowanego firmware.
- Jeden transceiver LoRa nie obsługuje niezależnego raw LoRa i pełnego
  Meshtastic jednocześnie. Przełączanie roli resetuje połączenie/stan;
  ciągła równoległa praca wymaga drugiego radia lub osobnego węzła.
- Przy nowym sprzęcie preferować wspierany węzeł SX126x/LR11xx według
  rekomendacji Meshtastic; aktualny SX1278 wymaga sprawdzenia targetu,
  pasma i kompatybilności zamiast obietnicy obsługi dowolnego modułu.

### 17.3. Zigbee i Thread

- Klasyczny ESP32 i S3 nie mają radia IEEE 802.15.4. Nie da się dodać Zigbee
  lub Thread przez samą bibliotekę ani użyć w tym celu CC1101/LoRa.
- **RCP** realizuje głównie radio; host nadal utrzymuje stos, np. OpenThread.
  **NCP** przenosi większą część stosu na moduł. Nazwa „coprocessor” sama nie
  określa, ile pamięci i logiki pozostaje na hoście.
- `ZigbeeAdapter`: jawny coordinator/router/end-device lub gateway do NCP,
  join/leave/permitJoin, PAN/channel, inventory, clusters/attributes,
  bounded bindings/groups i ACL zgodnie ze wspieranym SDK/protokołem NCP.
- Zamierzona brama Zigbee musi określić wspierane klastry. Możliwość
  podłączenia urządzenia nie oznacza implementacji wszystkich jego funkcji.
- `ThreadAdapter`: określić, czy jest klientem istniejącej bramy,
  hostem OpenThread+RCP, Thread endpoint, czy pełnym Border Routerem.
  Dataset/channel/network key i commissioning dostępne tylko dla admina.
- Thread zapewnia sieć IPv6, nie uniwersalne akcje aplikacyjne. Dla IFTTT
  wskazać konkretny protokół nad Thread, np. wybrane CoAP resources.
  Matter jest osobnym rozbudowanym zakresem z własnymi wymaganiami.
- Jeden C6/H2/transceiver nie oznacza dwóch niezależnych, ciągle aktywnych
  sieci Zigbee i Thread. Domyślnie profile rozłączne; równoległość tylko po
  potwierdzeniu konkretnego multiprotocol firmware i jego ograniczeń albo
  z dwoma modułami. Wi-Fi/BLE/802.15.4 mogą też dzielić czas jednego radia.
- IFTTT: typed attribute/cluster event i command dla Zigbee; network events
  oraz jawne application messages dla Thread. Nie udawać triggerów dla
  aplikacji, której stos nie implementuje.
- UI: Interfaces lub nowe strony Zigbee/Thread z capability registry,
  rola host/NCP/RCP, stan radia, wersja firmware, limity urządzeń i sekret-free
  configuration. Przełączenie roli/kanału jasno opisuje utratę sieci.

### 17.4. Czytnik NFC

Zachować PN532 SPI i konflikt adresu I2C `0x24`. Czytanie UID, NDEF,
zapis tagów, uwierzytelnianie kart i emulacja karty to osobne możliwości.
Pierwsza wersja: UID/presence, z debounce obecności i zdarzeniem ponownego
wejścia tagu. Rozszerzenia publikować dopiero po realnej implementacji.
Sam UID nie jest bezpiecznym uwierzytelnieniem do kontroli dostępu.

### 17.5. Audio: I2C control i I2S data

- `AudioPeripheral` używa adaptera kodeka I2C i I2S drivera; ewentualny
  moduł odtwarzający pliki po komendach I2C ma osobny kontrakt.
- Najpierw scope: krótkie lokalne sygnały/PCM, potem osobny profil streaming
  lub dekodery. Nie obiecywać jednoczesnego MP3/AAC/Opus/FLAC bez budżetów.
- Config: codec type/address, BCLK/WS/DIN/DOUT/optional MCLK, sample rate,
  channels/format, volume/mute, limity buffer i źródła playback.
- Na klasycznym ESP32 MCLK ma ograniczenia pinowe; konkretna biblioteka/core
  i kodek muszą to potwierdzić. Nie przydzielać automatycznie GPIO0/1/3.
- DMA buffers i driver mają wymagania pamięci; decoder/jitter buffer mogą
  korzystać z PSRAM tylko przy zgodności implementacji.
- IFTTT: playTone/playLocal/stop/setVolume; started/completed/underrun/error.
  Nagrywanie, speech recognition i streaming z internetu to odrębne funkcje.
- Bez blokowania mutexa I2C na cały czas playback. I2C volume/config to
  krótkie transakcje; sample data idą osobną ścieżką.

### 17.6. CC1101

- `Cc1101Peripheral`: SPI/CS/GDO, RX/TX state machine, frequency, modulation,
  bitrate, bandwidth, packet/raw mode i bounded frame history.
- CC1101 jest radiem sub-GHz FSK/GFSK/MSK/ASK/OOK, nie LoRa ani Zigbee/Thread.
- Zdefiniować katalog konkretnych wspieranych protokołów z encoder/decoder
  i schema; „raw RF” nie jest pełną kompatybilnością urządzeń.
- Zapis/replay wyłącznie dla własnych/uprawnionych urządzeń i formatów,
  które rzeczywiście można odtworzyć. Rolling-code/encryption nie jest
  magicznie obsługiwane przez nauczenie jednego pakietu.
- SPI współdzielić przez transakcje; wymaga osobnego CS/GDO i potwierdzenia,
  że aktualna płytka daje te połączenia. Nie zajmować wspólnego CS5.
- Rate/power/frequency ograniczyć do legalnych, skonfigurowanych parametrów
  pasma i urządzenia. Trigger frameReceived/protocolDecoded; akcja send.

### 17.7. IR TX/RX oraz IrDA

**Piloty IR:**

- `IrRemotePeripheral`: RMT TX/RX, driver LED IR i odbiornik odpowiedniego
  typu, carrier/duty, decode profiles i bounded raw timings.
- Typowy odbiornik demodulowany 38 kHz obsługuje sygnały pilotów, nie jest
  ogólnym odbiornikiem IrDA.
- IFTTT: decoded code/repeat/learned frame, akcje sendCode/sendSaved.
  Powtórzenia mają jawny filtr; klimatyzatory mogą używać długich ramek stanu.
- IR/RMT konkuruje o zasoby z WS2812 zależnie od drivera. Nie stosować
  bit-bangingu z długim wyłączaniem przerwań jako obejścia.

**IrDA:**

- `IrdaPeripheral` wymaga właściwego transceivera optycznego i drivera
  SIR/trybu UART lub zewnętrznego kontrolera zgodnego ze sprzętem.
- UART TX/RX plus stos linku to nie to samo co NEC/RC5 remote.
- Pierwszy scope SIR i wybrany framing nie oznacza IrLAP/IrLMP/OBEX/FIR
  ani pełnej wymiany plików z każdym dawnym urządzeniem.
- Przed kodem potwierdzić peer, warstwę protokołu i dostępną bibliotekę.
  Jeśli brak wspieranego stosu, zadanie pozostaje research/prototype,
  bez sukcesu deklarowanego jako „UART działa”.
- WebGUI: parametry bitrate/link/profile; IFTTT frameReceived/sendFrame
  dopiero po potwierdzeniu znaczenia danych na wymaganej warstwie.

### 17.8. Równoległe zasoby i modularne profile

Lista wyżej potrzebuje więcej połączeń niż sugeruje liczba bibliotek:

- UART0 jest już USB/log; UART1 domyślnie RS-232/GPS/legacy coprocessor;
  UART2 domyślnie RS-485. P17 TX12/RX13 może dostać UART1 lub UART2 przez
  GPIO Matrix, zamiast poprzedniej roli tego kontrolera. SIM7600,
  Meshtastic serial i RCP/NCP nie dostają trzech dodatkowych UART.
- GNSS w modemie może usunąć potrzebę osobnego GPS UART, ale AT/GNSS/PPP
  wymagają arbitrażu; to nie darmowy niezależny kanał.
- Zewnętrzne UART bridge, USB host albo TCP do gotowych bram są wariantami
  z własnym budżetem, nie niejawne rozszerzenie GPIO. Software UART nie jest
  rekomendowany dla LTE/dużego ruchu i wielu jednoczesnych protokołów.
- CC1101, PN532 i LoRa mogą dzielić SPI przy osobnych CS i zgodnych
  transakcjach; wszystkie linie IRQ/reset i aktywność trzeba zinwentaryzować.
- I2S audio wymaga clock/data, IR TX/RX dodatkowych pinów i RMT, WS2812
  data/RMT, PWM osobnego output. Ekspander GPIO nie zastępuje tych
  peryferiów czasu rzeczywistego.

Planowane profile opisowe, do utworzenia dopiero po HW-01:

1. `legacy-io`: obecny hardware + NTP/cron + wybrany radio i lekkie adaptery.
2. `cellular-gateway`: LTE/GNSS i ograniczony zestaw usług, bez pełnych stosów mesh/audio.
3. `s3-gateway`: większy flash/PSRAM, LTE, IR/CC1101/audio i zewnętrzne protokoły.
4. `meshtastic-bridge`: klient zewnętrznego węzła; pełny Meshtastic oddzielnie.
5. `zigbee-gateway` / `thread-br`: konkretna rola i kompatybilne NCP/RCP.

WebGUI pokazuje pełne schematy **danego profilu**, role i zależności; opcja
wyłączona w buildzie nie staje się dostępna przez checkbox. Każdy profil ma
manifest active/exclusive features, limity i własny raport pamięci.

## 18. NTP i harmonogram dla IFTTT

### 18.1. TimeService

Nowa usługa implementuje `IPeripheral`, `IPropertyProvider`,
`ITriggerProvider`, `IActionProvider` i `IConfigurable` stosownie do funkcji:

- Źródła czasu: SNTP przez sieć, DS3231 z poprawnym stanem i opcjonalnie
  GNSS UTC. Stan `unsynchronized`, `synchronized`, `holdover`, `invalid`
  z source/lastSync/age i oszacowaniem jakości, jeśli mierzone.
- UTC jest podstawą obliczeń. Strefa to osobna konfiguracja i formatowanie.
  Uptime/durations/debounce/cooldown nadal opierają się na monotonic time,
  nie na wall clock.
- Config: enabled, do 3 NTP servers, sync interval, timeout, timezone,
  sources priority i holdover max age. Nie blokować startu IO przez czekanie
  na NTP; używać zdarzenia zakończenia SNTP, nie długiego wait w `setup`.
- Default propozycji: SNTP enabled, sync co 1 h, timezone `UTC`;
  w polskim UI podać łatwy wybór Europe/Warsaw.
- ESP libc zwykle przyjmuje POSIX TZ, nie pełną bazę IANA. Utrzymywać
  jawne mapowanie wspieranych stref, np. Europe/Warsaw →
  `CET-1CEST,M3.5.0/2,M10.5.0/3`; pokazać ograniczenia przyszłych zmian DST.
  Advanced POSIX TZ walidować, nie przepisywać `Europe/Warsaw` do `TZ`
  bez sprawdzenia wsparcia libc.
- DS3231: sprawdzić utratę zasilania/flagę oscillator stop i poprawność daty.
  RTC przechowuje UTC. Aktualizacja z NTP opcjonalna, ograniczona, bez
  zapisów NVS co sekundę. GNSS time valid nie jest tożsame z position fix.
- NTP bez uwierzytelnienia nie jest kryptograficznie zaufanym czasem;
  sanity checks daty/skoku, dopuszczalna korekta i alert. NTS osobny scope.
- Przy utracie sieci utrzymać holdover do jawnego TTL; po TTL harmonogramy
  wall-clock zawieszone i widoczne, nie wykonywane według 1970 r.
- Time correction event zawiera old/new UTC, delta i reason. Nie zmienia
  monotonic timers. Różne źródła nie mogą bez arbitrażu ustawiać czasu naraz.
- API/UI: bieżące UTC/local, strefa, source/age, next sync i last error;
  akcja resync i administracyjna setTime; trigger synchronized/lost/corrected.
- Scheduler po suspend/resume i korekcie stosuje poniższe reguły misfire,
  a nie odtwarza całą zaległą historię.

### 18.2. ScheduleService — nie scheduler tasków RTOS

Rozróżnienie:

- `Scheduler` runtime z rozdziału 5 planuje krótkie `tick` sterowników.
- `ScheduleService` opisany tutaj wyzwala IFTTT według czasu.
- FreeRTOS scheduler planuje wykonywanie tasków. Te trzy mechanizmy nie są
  wymienne i nie powinny mieć jednego nieprecyzyjnego obiektu „scheduler”.

Pierwsza wersja harmonogramów:

1. Cron pięciopolowy: `minute hour day-of-month month day-of-week`.
2. Interval: co N sekund/minut, na zegarze monotonicznym.
3. One-shot: w określonym UTC albo po opóźnieniu monotonicznym.

Zakres cron: liczby, `*`, listy, zakresy i `/step`; minute 0–59, hour 0–23,
DOM 1–31, month 1–12, DOW 0–6 z 0=niedziela. Bez sekund, `L/W/#`,
shell commands i dowolnych wyrażeń w pierwszej wersji.
DOM/DOW: gdy oba są ograniczone, **OR jak w klasycznym cron**; gdy jedno
jest `*`, obowiązuje drugie. Opisać to w podglądzie kreatora i testach.

Każdy harmonogram ma:

- Stabilne `scheduleId`, nazwę, enabled, typ, spec i revision.
- Strefę dla cron (`system` albo wspierany jawny timezone); one-shot
  przechowywany w UTC z etykietą wybranej strefy.
- `misfirePolicy`: default `skip`; opcjonalnie `run_once` z max lateness.
- Domyślnie bez catch-up po restarcie. Brak czasu suspenduje cron i UTC
  one-shot, lecz nie musi suspendować monotonic interval.
- Dla interval default fixed-rate od startu aktywnej instancji, brak
  trwałego odtwarzania fazy przez zanik zasilania; po opóźnieniu skip missed
  albo run_once, bez pętli wykonywania wszystkich zaległych akcji.
- Status valid/suspended/exhausted, nextRunUtc/remainingMs, lastDue,
  lastEventId, last dispatch error i opis następnych terminów.

Początkowy limit 16 harmonogramów, opublikowany w API. Harmonogram
**publikuje event IFTTT**, a nie wywołuje bezpośrednio przekaźnika. Wiele
reguł może słuchać jednego harmonogramu; limity 4 reguł/4 akcji pozostają
osobnym budżetem do ewentualnego zwiększenia pomiarami.

### 18.3. DST, skoki czasu i gwarancje wykonania

- Cron wylicza lokalny kalendarz, lecz identyfikuje zdarzenia również
  przez due UTC i revision harmonogramu.
- Default DST spring-forward: nieistniejący termin lokalny pomiń;
  `run_once` może wykonać jeden misfire wyłącznie w max lateness.
- Default DST fall-back: ten sam lokalny slot wykonać raz przy pierwszym
  wystąpieniu. Nie wykonywać ponownie po cofnięciu czasu/NTP.
- W runtime przechowywać bounded dedupe po scheduleId/revision/local slot
  i due UTC zgodnie z polityką; zwykłe `if(hour==...)` co loop jest błędne.
- Mała korekta SNTP/smooth nie resetuje historii. Duży forward jump:
  default skip; opcjonalnie jeden run_once, nigdy pełna lawina.
- Backward jump nie resetuje last-fired. Wyznaczanie nextRun ma ograniczony
  horyzont, np. 5 lat, i zwraca `no_occurrence_in_horizon`; nie oznacza
  automatycznie błędnego cron, np. poprawnego terminu 29 lutego.
- Po normalnym restarcie default nie wykonywać ponownie bieżącego slotu
  tylko dlatego, że nadal trwa ta minuta; uzbroić wall schedules dla
  przyszłych terminów. Opcjonalny run_once/recovery wymaga historii trwałej.
- One-shot monotonic po restarcie nie może udawać odtworzonego czasu
  pozostałego; default anulowanie z jawnym statusem, chyba że zapisano
  jawny UTC deadline w trybie z wiarygodnym zegarem.
- One-shot UTC może wymagać trwałego consumed marker. Zapisać i raportować
  jego błędy; odróżnić publikację eventu od zakończenia wszystkich akcji.
- **Exactly-once skutków fizycznych przez reset/power loss nie jest
  gwarantowane.** Zapis markera przed akcją może pominąć akcję po resecie;
  zapis po akcji może dopuścić powtórzenie. Dla krytycznych aplikacji
  stosować idempotentne akcje, dedupe IDs i świadomą politykę recovery.
- Nie zapisywać NVS w każdej minucie tylko dla odpytywania zegara.
  Trwałe markery/recovery mają ograniczenie zapisów i osobny budżet.

### 18.4. Przykład konfiguracji i powiązania IFTTT

```json
{
  "id": "schedule.weekdays-0700",
  "name": "Weekdays 07:00",
  "enabled": true,
  "kind": "cron",
  "expression": "0 7 * * 1-5",
  "timezone": "Europe/Warsaw",
  "misfirePolicy": "skip",
  "dstRepeatPolicy": "once"
}
```

Źródło reguły event:

```text
schedule.weekdays-0700/trigger/due
```

Payload: scheduleId/revision, planned UTC, actual UTC, lateness, source time
quality i ID eventu. Reguła korzysta z normalnych `actions` z rozdziału 6.
Monotonic interval przy nieważnym wall clock ma `actualUtc=null` i prawidłowe
monotonic timestamps.

WebGUI IFTTT: zakładka wewnętrzna „Harmonogramy”, kreator codziennie/tygodniowo/
interval/one-shot i advanced cron, preview kolejnych 5 terminów z UTC offset,
ostrzeżenia DST/time invalid, test walidacji bez wykonywania sprzętu.
Settings: źródła czasu/NTP/strefa; Diagnostic: sync quality/skoki/misfires.
Nowy panel nie wymaga osobnej globalnej strony „Cron”.

## 19. Optymalizacja wydajności i podział między rdzenie

### 19.1. Najpierw pomiar i usunięcie blokowania

Największy początkowy zysk zwykle dają:

- brak parsowania rules JSON w loop;
- DS18B20 conversion state machine, osobne sampling intervals;
- bounded RX i asynchroniczne timeouty transportów;
- cache metadanych, filtrowanie telemetrii i unikanie dużych kopii `String`;
- OLED update tylko po dirty/terminie, bez zbędnych pełnych transferów;
- BLE scan/NFC/HTTP/TLS/audio poza szybką ścieżką IO;
- świadome limity historii i kompilowanie tylko wybranych dekoderów;
- brak ponownej inicjalizacji wszystkich UART/sensorów po zmianie jednej
  niezwiązanej opcji formularza.

Nie wykonywać „optimize all tasks” bez pomiarów p95/p99, największego heap,
task stack high-water mark, bus wait i kolejki. Dwa rdzenie nie dają
automatycznie 2× wydajności; część workload jest I/O-bound.

### 19.2. Proponowany model tasków

Arduino-ESP32 już korzysta z FreeRTOS. Dokładny core affinity tasków Wi-Fi,
BLE i `loopTask` zależy od wersji/core/Kconfig; sprawdzić build zamiast
zakładać stałe „core0=sieć, core1=aplikacja” dla każdej płytki.

Model początkowy dla dual-core ESP32/S3, do zatwierdzenia pomiarami:

| Owner/task | Praca | Propozycja affinity |
| --- | --- | --- |
| `ControlTask` | S2/DI, dispatcher IO, evaluator IFTTT, harmonogram due, szybkie state machines | Core 1, odpowiedni priorytet wyższy niż własne UI, bez zagładzania systemu. |
| `IoWorker` / bus owner | Wolniejsze sensory/NFC, bounded I2C/SPI i publikacja snapshotów | Początkowo współpraca z ControlTask lub osobny worker według pomiaru; nie blokuje sterowania przez timeout sensora. |
| `NetworkWorker` | HTTP server/client, modem parser, MQTT reconnect/TLS w ograniczonych operacjach | Unpinned albo Core 0 po zmierzeniu współistnienia ze stosami systemowymi. |
| `AudioTask`, tylko profil audio | DMA refill/decoder/playback, bounded queue | Affinity/priority po pomiarze; nie każda biblioteka jest thread-safe. |
| `StorageWorker` | NVS/config/relay snapshots, serializowane zapisy | Jeden owner, krótkie zadania; wynik wraca przez kolejkę. |

To nie polecenie utworzenia pięciu tasków od razu. Łączyć małe prace,
wydzielać tylko realnie blokujące/ciągłe ścieżki. Każdy nowy task kosztuje
stack i scheduler overhead; budżet stack rzędu 3–8 KiB jest tylko startem
do pomiaru, TLS/codec może wymagać więcej. API IDF dla stack size sprawdzić:
często określa bajty, nie jednostki jak w vanilla FreeRTOS.

ControlTask nie może synchronicznie czekać na długi BLE scan/TLS/DNS/flash.
W trybie trwałych przekaźników ACK `persisted` czeka logicznie na
StorageWorker, ale sam IO owner dalej obsługuje system. Kolejność snapshotów
według operation/generation; późniejszy zapis nie może zostać nadpisany
starszym zadaniem. Jeśli backlog uniemożliwia gwarantowane utrwalanie,
odrzucać nadmiarowe nowe akcje jawnie przed przełączeniem.

### 19.3. Synchronizacja i ownership

- Jeden właściciel peryferium i jego mutable state. Pozostałe taski wysyłają
  komendy, konsumują immutable snapshots i zdarzenia.
- `WebServer`, `Preferences`, `Wire`, `SPI`, UART i bufory JSON nie są
  automatycznie bezpieczne dla niezależnych równoległych wywołań.
- Wybrać bus owner albo krótki mutex/transakcję per bus, nie oba chaotycznie.
  Nie trzymać mutexów przez network wait, DHT conversion ani playback.
- Jeden długi timeout I2C sensora może zablokować przekaźniki na tej samej
  magistrali mimo różnych rdzeni. Bus timeout, kolejność i recovery są
  kluczowe; ISR i drugi task nie omijają zajętego I2C.
- Kolejki bounded, payload copied lub buffer pool z jawnym ownership.
  Nie przekazywać wskaźników do lokalnego JSON/stacka callbacka.
- Atomics dla prostych współdzielonych liczników; mutex/message passing dla
  struktur. `volatile` nie zastępuje synchronizacji.
- Critical sections bardzo krótkie, bez I2C/NVS/JSON/print. Unikać priority
  inversion i lock ordering cycles.
- ISR tylko timestamp/capture/queue; RMT/I2S/DMA do czasu krytycznego.
- Zapis/erase flash może zakłócać wykonanie na obu rdzeniach i dostępność
  cache zależnie od drivera/config. Osobny StorageWorker nie usuwa tego
  efektu; mierzyć przy równoczesnym IR/audio/radio/relay NVS.
- Heartbeat/watchdog dla rzeczywistego postępu taska; nie karmienie
  watchdoga w miejscu, które ukrywa deadlock.
- C6/single-core używa poprawnego wariantu affinity; nie wywoływać core1.
  Działanie funkcjonalne bez pinningu jest punktem odniesienia.

### 19.4. Kryteria wielordzeniowej optymalizacji

Przed/po na tym samym workload:

- event→action latency, HTTP response, audio underruns/IR dropped capture;
- min/largest internal heap, PSRAM i stack high-water mark;
- czas oczekiwania I2C/SPI/locks, liczba reconnect/drop i CPU/task load;
- jednoczesny LTE/TLS handshake, BLE scan, WS2812/IR i NVS write;
- 30 min smoke oraz docelowo 24 h soak dla profilu produkcyjnego;
- brak race przy config hot-apply/stop, reboot i zaniku transportu.

Zmianę affinity zaakceptować tylko przy zmierzonej poprawie lub usunięciu
blokowania bez regresji. Nie przypisywać zadania do core0 tylko dlatego,
że w nazwie ma „network”.

## 20. Rozszerzenia — zakres integracji, testy i źródła

Zadania rozszerzeń są częścią uporządkowanej checklisty w rozdziale 13.
FINAL-01 waliduje zakres podstawowy, a EXT-01 wybrane profile rozszerzone.
Nie oznacza to obowiązku uruchomienia wszystkich funkcji w jednym obrazie.

### 20.1. Dodatkowe testy akceptacyjne

| Obszar | Wymagane potwierdzenie |
| --- | --- |
| NTP | Timeout bez blokowania IO, korekta w przód/w tył, DNS failure, utrata Wi-Fi/LTE, holdover TTL i niewiarygodny RTC. |
| Cron | `0 7 * * 1-5`, list/range/step, invalid range/zero step, DOM/DOW OR, 29 lutego i bounded next-occurrence search. |
| DST | Europe/Warsaw spring missing slot/fall double slot, once policy, display offset i dedupe. |
| Restart/misfire | Nieuzbrojony current minute, skip/run_once lateness, one-shot consumed write failure i brak obietnicy exactly-once. |
| SIM7600/GNSS | AT URC podczas transferu, PPP/CMUX, zanik SIM/sygnału/zasilania, poprawne UART levels, stale fix i UTC validity. |
| Meshtastic | Rzeczywisty Client API peer, powtarzające się teksty z nowym ID, wersja protobuf, ACK vs delivery, node DB limit. |
| Zigbee/Thread | Dokładna rola NCP/RCP/host, realne join/IPv6/application event, secret datasets, brak kolizji trybów radiowych. |
| Audio | Format/rate/buffers, underrun, I2C volume podczas relay action, TLS jitter dla opcjonalnego streamingu. |
| CC1101/IR/IrDA | Wybrany encoder/decoder/peer, timings, replays/repeats, radio/RMT ownership i jawne unsupported protocols. |
| SMP | Concurrent queues/config/stop, lock timeout, NVS flash stall, internal heap/DMA/PSRAM i stack high-water marks. |

### 20.2. Źródła techniczne i granice weryfikacji

Źródła online sprawdzone podczas przygotowania rozszerzenia 2026-10-31:

- [ESP32-S3 — CPU, SRAM, PSRAM i peryferia](https://www.espressif.com/en/products/socs/esp32-s3).
- [ESP32-C6 — radio Wi-Fi/BLE/802.15.4](https://www.espressif.com/en/products/socs/esp32-c6).
- [ESP32-S3 peripherals API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/index.html).
- [IDF FreeRTOS SMP / affinity](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html).
- [ESP-IDF heap capabilities i pamięć](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/mem_alloc.html).
- [SNTP, system time i timezone](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html).
- [I2S, DMA i ograniczenia peryferiów](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html).
- [esp_modem: SIM7600, AT, PPPoS i CMUX](https://docs.espressif.com/projects/esp-protocols/esp_modem/docs/latest/README.html).
- [Meshtastic supported hardware](https://meshtastic.org/docs/hardware/devices/).
- [Meshtastic Client API serial/TCP/BLE](https://meshtastic.org/docs/development/device/client-api/).
- [Espressif Zigbee architecture i SDK](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/introduction.html).
- [Espressif Thread Border Router](https://docs.espressif.com/projects/esp-thread-br/en/latest/).
- [Texas Instruments CC1101 — możliwości radia](https://www.ti.com/product/CC1101).
- [ESP32 UART controllers, bufory i przypisanie pinów](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/uart.html).
- [ESP32-H2 — BLE i 802.15.4 bez Wi-Fi](https://www.espressif.com/en/products/socs/esp32-h2).
- [ESP32-C5 — dual-band Wi-Fi, BLE i 802.15.4](https://www.espressif.com/en/products/socs/esp32-c5).

Źródła potwierdzają kierunki architektury i możliwości układów, **nie**
widełki pamięci konkretnej kompilacji ani poprawność adaptera do KC868-A6.
Dokumentacja `stable/latest` może opisywać nowszy IDF niż obecny PlatformIO;
zadania muszą dobrać API do rzeczywistej wersji i przypiąć kompatybilne
zależności. Wariant SIM7600, datasheet elektryczny, moduł IrDA i kodek audio
nadal wymagają potwierdzenia przed hardware implementation.

## 21. P17: sprzętowy UART, H2/C5/C6 i customowy mostek

### 21.1. Korekta rozumienia P17 i kontrolerów UART

GPIO12 i GPIO13 mogą pełnić funkcje TX/RX sprzętowego UART przez GPIO Matrix
ESP32. Daje to pełny duplex, FIFO/bufory drivera i przerwania; nie wymaga
software UART i nie ma pinowego limitu 115200 baud.

Na podstawie informacji użytkownika ze schematu KinCony przyjmujemy P17:

| Sygnał P17 | Docelowa rola po stronie KC868-A6 | Połączenie z koprocesorem |
| --- | --- | --- |
| GPIO12 / IO12 | TX hosta | RX koprocesora |
| GPIO13 / IO13 | RX hosta | TX koprocesora |
| 3V3 | Zasilanie pomocnicze | Tylko przy potwierdzonym budżecie regulatora/modułu |
| GND | Masa | Wspólna masa |

To przypisanie sygnałów, nie zatwierdzona kolejność pinów 1–4 na fizycznym
złączu; orientację sprawdzić na PCB. Schemat nie definiuje obowiązkowego
TX/RX dla IO12/13: jest to wybór konfiguracji. Piny P17 są elektrycznie
oddzielone od transceiverów RS-232/RS-485 i CH340C według opisu użytkownika.
Potwierdzenie konkretnej rewizji należy do HW-01.

**ESP32 ma 3 kontrolery UART, nie 4.** GPIO Matrix zmienia miejsce
wyprowadzenia istniejącego kontrolera, nie tworzy nowego:

| Konfiguracja | UART0 | UART1 | UART2 | Dostępne równocześnie |
| --- | --- | --- | --- | --- |
| Dotychczasowa | USB/log GPIO3/1 | RS-232 GPIO17/16 | RS-485 GPIO14/27 | USB + RS-232 + RS-485 |
| P17 + RS-232, propozycja domyślna | USB/log | RS-232 GPIO17/16 | P17 RX13/TX12 | USB + RS-232 + P17; RS-485 wyłączony |
| P17 + RS-485 | USB/log | P17 RX13/TX12 | RS-485 GPIO14/27 | USB + P17 + RS-485; RS-232/GPS/legacy coprocessor wyłączony |

Jednoczesne USB/log + RS-232 + RS-485 + niezależny P17 wymaga dodatkowego
kontrolera UART, odpowiedniego bridge albo innej zmiany sprzętowej.
Wyprowadzenie TX jednego UART na dwa piny nie daje dwóch niezależnych
strumieni. Multipleksowanie czasowe nie jest równoczesnym odbiorem.
Nie przełączać pinów kontrolera podczas aktywnej transmisji jako obejścia.

Dotychczasowe `ttlRx=12` i `ttlTx=13` w
[include/kc868_a6_pins.h](./include/kc868_a6_pins.h) są odwrotne do wybranego
nowego profilu. Obecny firmware nie uruchamia na nich UART, więc dokument
nie zmienia teraz kodu. Zadanie P17-01 ma dodać jednoznaczne `p17Tx=12`,
`p17Rx=13` i sprawdzić wszystkie użycia starych nazw przed usunięciem lub
deprecjacją. Nie zmieniać ich niejawnie w sterownikach zależnych od legacy.

Obecny `pn532SpiCs=13` **koliduje z RX P17**. W profilu P17 PN532 na tym CS
jest wyłączony; równoległy NFC wymaga zatwierdzonego innego połączenia CS
albo czytnika obsługiwanego przez koprocesor. Nie pozwalać na jednoczesny
TX koprocesora i sterowanie CS13 przez główny MCU.

GPIO12 jest pinem strapping wpływającym na konfigurację zasilania flash
klasycznego ESP32. Gdy jest TX hosta, koprocesor powinien mieć na tej linii
wejście bez wymuszania nieodpowiedniego poziomu podczas resetu; sprawdzić
pull-upy modułu, boot behavior i ewentualny translator. Wybór TX12 nie usuwa
ryzyka strap. Nie zmieniać eFuse jako automatycznej „naprawy”.

### 21.2. Konfiguracja linku P17

Przykład **planowanej** konfiguracji, nie gotowy wpis obecnego firmware:

```json
{
  "id": "coprocessor.p17",
  "enabled": true,
  "transport": {
    "kind": "hardware_uart",
    "controller": 2,
    "txPin": 12,
    "rxPin": 13,
    "baud": 115200,
    "dataBits": 8,
    "parity": "none",
    "stopBits": 1,
    "flowControl": "none",
    "rxBufferBytes": 2048,
    "txBufferBytes": 2048
  },
  "target": "esp32-h2",
  "firmwareRole": "custom_bridge",
  "protocol": "kc-bridge-v1",
  "expectedPeerId": "bridge.main",
  "required": false
}
```

- `controller` 1 lub 2 rezerwowany globalnie; routing zastępuje domyślną
  rolę tego UART. GPIO Matrix, pinMode i `Serial*.end/begin` koordynuje owner.
- P17 ma dwie linie danych bez RTS/CTS/reset/boot. Default flow control
  `none`; hardware RTS/CTS wymaga dodatkowych przewodów i rezerwacji.
- Rozszerzyć katalog baud: co najmniej 9600/19200/38400/57600/115200/
  230400/460800. Wyższe prędkości, np. 921600, publikować jako obsługiwane
  dopiero po pomiarze błędu baud i testach obu końców/kabla/obciążenia.
- Default 115200 dla pierwszego połączenia; 460800 to docelowa testowana
  opcja, nie gwarancja dowolnego przewodu i peer firmware.
- Dla 8N1: 115200 daje teoretycznie 11 520 B/s w każdym kierunku,
  460800 daje 46 080 B/s. Użyteczny payload jest niższy przez framing,
  ACK i scheduling. Pełny duplex nie dzieli automatycznie limitu między TX/RX.
- Przy 460800/8N1 bufor RX 2048 B daje około 44 ms na opróżnienie przy
  ciągłym odbiorze. Stack/flash stalls i timeouts mogą go przepełnić;
  używać bounded work, software credits/backpressure i counters.
- Zmiana baud: tylko jawna negocjacja supported rates obu stron z ACK,
  flush, terminem przełączenia i recovery. Nie skanować prędkości przez
  wysyłanie przypadkowych komend do nieznanego urządzenia.
- Handshake timeout nie blokuje startu IO/WebGUI. Link disabled/offline
  jest widoczny; `required` zawiesza tylko funkcje od niego zależne.
- Zasilanie z 3V3 P17 dopiero po weryfikacji wydajności regulatora i prądu
  radiowego modułu; H2/C5/C6 nie mają jednakowego poboru. Nie łączyć dwóch
  aktywnych źródeł 3V3 bez projektu zasilania.

### 21.3. Profile H2, C6 i C5

Konfiguracja hosta osobno określa **target chip** i **firmware role**.
Nie wyciągać możliwości z samej nazwy układu.

| Target | Naturalne zastosowanie | Role do rozważenia i ograniczenia |
| --- | --- | --- |
| ESP32-H2 | Zigbee/Thread/BLE oraz obsługa lokalnych peryferiów | Brak Wi-Fi. Pełny Zigbee NCP/application bridge albo Thread RCP/wybrany application bridge, zależnie od firmware. |
| ESP32-C6 | 802.15.4/BLE + Wi-Fi 2,4 GHz i własne usługi | Custom bridge może obsłużyć MQTT/TLS/network lokalnie. RCP nie przenosi hostowego stosu Thread poza główny ESP. |
| ESP32-C5 | Dual-band Wi-Fi 2,4/5 GHz, BLE/802.15.4, wybrane warianty PSRAM | Dobry kandydat do network/TLS/protocol bridge; rzeczywiste wsparcie UART/NCP/RCP/protokołów sprawdzić w przypiętym SDK. Nie zakładać, że każdy przykład C6 kompiluje się na C5. |

Planowane role:

- `zigbee_ncp`: konkretny wspierany protokół NCP/firmware; host utrzymuje
  adapter i ograniczony inventory, peer stos Zigbee według implementacji.
- `thread_rcp`: standardowy transport/protokół wspieranego RCP; OpenThread
  host i ewentualny Border Router nadal obciążają główne MCU.
- `thread_app_bridge`: peer utrzymuje wybraną aplikację Thread i eksponuje
  jej properties/actions; nie jest to automatycznie IPv6 Border Router.
- `custom_bridge`: własny wersjonowany protokół aplikacyjny do capability
  offload; peer może utrzymywać sensor timing, radio stacks, MQTT/TLS/HTTP.

Standardowych RCP/NCP nie opakowywać arbitralnie w `kc-bridge-v1` ani nie
wysyłać do nich custom handshake. `protocol` musi odpowiadać firmwareRole;
każdy driver ma własny handshake, wersję, framing i limity. C5/H2/C6 boot ROM
UART nie jest z definicji portem aplikacyjnego NCP.

Profile H2/C6/C5 mają osobne projekty firmware koprocesora, manifest SDK,
pinout strony peer i feature flags. Oznaczenia baud i TX/RX hosta nie
narzucają numerów GPIO koprocesora; jego RX/TX podać w schemacie modułu.

### 21.4. Custom bridge — odciążenie, nie drugi evaluator bez kontroli

Docelowy `CoprocessorBridgePeripheral`:

1. Tworzy jeden owner linku P17 i ograniczony parser strumienia.
2. Weryfikuje protocol version/peer ID i negocjuje możliwości/limity.
3. Udostępnia zwalidowane proxy properties/triggers/actions/config.
4. Obsługuje timeout/reconnect, reset session, backpressure i wyniki operacji.
5. Nie uruchamia pełnego nieużywanego stosu protokołu na hoście.

**Podział odpowiedzialności:**

- Główny MCU: przekaźniki/DI/ADC, bezpieczny startup RAM/NVS, TimeService,
  harmonogram i centralny IFTTT, konfiguracja/UI/API oraz bridge proxies.
- Koprocesor: określone radio/protocol stack, local sensors/IR/PWM timing,
  network parsing/TLS lub inne jawnie delegowane usługi.
- Callbacki/zdarzenia peer wracają do centralnego EventBus. Ten sam IFTTT
  nie jest oceniany niezależnie na dwóch MCU, aby nie powielać akcji.
- Wybrana lokalna automatyzacja peer może być osobną zdolnością z własnym
  ID i właścicielem. Krytyczny fail-safe peer działa niezależnie od hosta
  według jawnej konfiguracji, nie przez czekanie na serial command.
- Odłączenie peer nie wpływa na domyślne bezpieczeństwo lokalnych
  przekaźników; reguły zależne od niego stają się suspended/unavailable.

**Protokół `kc-bridge-v1` — minimum do doprecyzowania i implementacji:**

- Ramka ma jawny delimiter/framing, length, protocol version, message type,
  session ID, request/event ID, bounded payload i CRC. Propozycja:
  COBS + separator `0x00`, sprawdzony nagłówek i CRC32; limit decoded frame
  1024 B, payload maksymalnie 768 B w pierwszej wersji.
- Wybrać typowany binarny codec, np. nanopb; opisać schema w jednym
  wersjonowanym źródle dla obu firmware. Nie robić parsera COBS/CRC od zera,
  jeśli dostępna jest odpowiednia sprawdzona implementacja.
- Komunikaty: hello/capabilities, get/set config, property snapshot,
  event, execute action, accepted/completed/error, heartbeat i credits.
- Bounded inventory descriptor: maksymalnie 2 eksportowane logiczne
  moduły w profilu początkowym, fragmentowany w strony; każda strona
  sprawdzana przed dołączeniem do rejestru. Driver catalog określa allowed
  types/actions/param limits. Nie ufać dowolnej schema dostarczonej przez peer.
- ID proxy: np. `bridge.main/sensor.env/property/temperature`; zmiana
  sesji nie zmienia stabilnego ID urządzenia, ale unieważnia stare event IDs.
- Telemetria agregowana/decymowana: peer przesyła pomiary i istotne
  zdarzenia, nie każdy surowy sample. Audio i duże ciągłe raw captures nie
  mieszczą się w tym samym lekkim linku bez osobnego profilu.
- Credits/queue limits, prioritization commands/results vs telemetry,
  timeouts i liczniki CRC/framing/drops są obowiązkowe bez RTS/CTS.
- Akcje rozróżniają queued/accepted/applied/confirmed/error; bridge link
  ACK nie potwierdza fizycznego wykonania ani radio delivery.
- Request dedupe w danej sesji. Nie replayować automatycznie po reconnect
  nieidempotentnych poleceń. Po resecie peer operation state może być unknown.
- Timestamps peer mają jawny clock domain. Host dopisuje receive monotonic
  time; pozycja czasowa peer nie staje się UTC bez synchronizacji.
- CRC wykrywa błędy linii, nie uwierzytelnia. Wymagania trusted link/peer
  zapisane w profilu; dla niezaufanych połączeń osobny mechanizm auth,
  nie deklaracja bezpieczeństwa dzięki `expectedPeerId`.

Proponowany dodatkowy budżet hosta dla ograniczonego custom bridge:
około 30–150 KiB flash i 8–32 KiB RAM, zależnie od codec/proxy/queues.
To szacunek do pomiaru. Odciążenie jest realne dopiero, gdy host **nie**
linkuje/utrzymuje pełnego stosu wyniesionego na peer. Bridge transport nie
gwarantuje mniejszego flash, jeśli zostawi się wszystkie lokalne biblioteki.

### 21.5. WebGUI i bezpieczna konfiguracja peer

Interfaces→P17/Coprocessor:

- routing UART1/UART2, GPIO hosta, baud/buffers, target, firmwareRole,
  protocol version, peer ID, status/session, firmware/SDK i supported features;
- wyjaśnienie „wyłącza RS-485” albo „wyłącza RS-232/GPS” przed zatwierdzeniem;
- konflikt PN532 CS13, strapping GPIO12 i brak RTS/CTS na P17;
- capabilities/offload inventory, last heartbeat, error/CRC/drop counters,
  host/peer RAM jeśli peer raportuje, operacje pending/unknown;
- formularze peer z lokalnie dozwolonych schematów i negotiated capabilities,
  nie wykonanie obcego HTML/JS i nie dowolna konsola komend AT;
- jawna walidacja config host+peer: validate → peer prepare →
  host persist pending → peer apply/confirm → active revision.
  Gdy link zerwie się w trakcie, config pozostaje pending/unknown i
  reconciles przez odczyt revision, nie komunikat „zapisano wszystko”.
- API auth/scopes identyczne z lokalnym sterowaniem; sekrety peer redacted.
- Zmiana roli custom/NCP/RCP wymaga zgodnego firmware peer. Checkbox nie
  flashuje automatycznie drugiego ESP i nie zmienia jego stosu radiowego.
- Aktualizacja firmware peer osobne przyszłe zadanie. P17 bez reset/boot
  nie gwarantuje automatycznego flashowania; nie dodawać go jako cichej
  zależności pierwszego bridge.

Remote moduł ze standardowym property/action dostaje widget i pozycję IFTTT
przez te same registry kontrakty. Diagnostic pokazuje powiązanie:
UART controller → P17 → peer → eksportowane peryferia i ich resource owner.

### 21.6. Zmieniony budżet równoczesnych urządzeń i optymalizacja profilu

Założenie projektowe użytkownika: wiele urządzeń I2C oraz zwykle **jedno,
maksymalnie dwa aktywne rozszerzenia** serial/RS-232/P17/SPI, zależnie od
podłączonych peryferiów. Ten limit traktujemy jako docelowy workload,
**nie fizyczną uniwersalną granicę KC868-A6**. Nie obejmuje stałych relay/DI
expanderów, wbudowanego Wi-Fi ani narzuca limitu dwóch sensorów I2C.

- I2C `n` jest ograniczone adresami, elektryką, pull-upami, pojemnością,
  prędkością i sumą czasu transakcji; nie oznacza nieskończonej liczby.
  Publikować limit aktywnych instancji zgodny z pamięcią i manifestem.
- Domyślny profil zoptymalizowany dla jednego rozszerzenia; profil dual dla
  konkretnych dwóch kompatybilnych ról, bez arbitralnego przydziału pinów.
- Przykłady: P17 bridge + radio SPI; P17 bridge + RS-232 przy UART2→P17;
  P17 bridge + RS-485 przy UART1→P17; radio SPI + RS-232.
  NFC na CS13 z P17 jest niekompatybilne bez zmiany połączenia.
- Dwa niezależne moduły TTL nie dzielą bezpośrednio TX/RX P17. „H2 lub/i C5/C6”
  realizować jako warianty peer, a dwa MCU jednocześnie przez osobne
  transporty albo główny peer-bridge z własnym downstream linkiem.
  Drugi przypadek wymaga jawnego inventory/proxy routing i osobnego
  projektu, nie zwarcia dwóch TX do RX hosta.
- Peer obsługujący więcej peryferiów nie obciąża wszystkich hostowych
  GPIO, ale nadal zużywa link bandwidth, host proxy RAM i IFTTT budget.
- Rozróżnić `maxActiveExpansions` hosta i liczbę eksportowanych modułów peer.
  W pierwszym custom bridge profile limit 2 logiczne moduły jest osobny
  od limitu 1–2 fizycznych rozszerzeń na KC868.

**Dwa poziomy optymalizacji:**

1. **Build-time:** profile zawierają tylko potrzebne lokalne sterowniki/
   dekodery i bridge adapters; nieużywane dependency/feature flag wyłączone,
   także ich globalne obiekty i ścieżki rejestracji. Dead-code elimination
   potwierdzić mapą linkera, nie zakładać, że sama flaga `enabled=false`
   usunęła kod biblioteki.
2. **Runtime:** compiled factory uruchamia tylko skonfigurowane/wykryte
   wspierane instancje, tworzy ich bufory/tasks i zwalnia zasoby przez stop.
   Gdy moduł jest niedostępny, health i reguły suspended pozostają widoczne.

Na standardowym ESP32 firmware nie „dociąga” dowolnego sterownika C++ z
WebGUI. Runtime wybór zmniejsza pracę/RAM, ale nie usuwa linkowanego flash.
Zmiana na profil z innymi bibliotekami wymaga nowego obrazu/OTA; UI pokazuje
`requiresFirmwareProfile`, nie obiecuje dynamicznego ładowania kodu.

Detekcja nie może polegać na samym ACK I2C lub próbie różnych baud.
Jawna konfiguracja i bezpieczny driver probe/handshake ustalają urządzenie.
I2C sensorów ani SPI nie hot-swapować elektrycznie bez potwierdzonego hardware;
odłączenie/reconnect to przede wszystkim recovery drivera.

Przy **1–2 wybranych rozszerzeniach i custom offload** zachowanie obecnego
ESP32 staje się realnym pierwszym wariantem; nie trzeba wymieniać MCU tylko
z powodu długiej listy potencjalnie wspieranych modeli. Wąski profil może
zmieścić się po usunięciu nieużywanych stosów, ale obecne 92% slotu nadal
wymaga fresh build/map/heap pomiarów. H2 lepiej odciąża 802.15.4, C6/C5 mogą
wynieść również sieć/TLS; profil RCP odciąża mniej niż application bridge.
Decyzję o S3 podjąć po pomiarze właściwego workload, nie sumie wszystkich
opcji katalogu.

### 21.7. Zadania mostka i testy integracyjne

Zadania P17-01, BRIDGE-01, PEER-01, BRIDGEUI-01, PROFILE-01 i P17TEST-01
znajdują się w głównej checkliście w rozdziale 13, po swoich prerekwizytach.
Poniższe zasady i testy uzupełniają ich kryteria ukończenia.

Do manifestu integracji EXT-01 dołączyć P17TEST-01, gdy wybrano P17/custom
bridge. ZB-01/THREAD-01 wybierają odpowiedni P17/peer driver jako zależność
po zatwierdzeniu roli; custom bridge i standardowy RCP/NCP nie są zamienne.

| Test P17 / bridge / profile | Wymagany wynik |
| --- | --- |
| Controller conflict | Brak możliwości P17 + poprzednia rola tego samego UART; USB/log zachowany. |
| GPIO conflict | RX13 + PN532 CS13 odrzucone; TX12 i poziom podczas resetu sprawdzone. |
| Baud/full-duplex | Realny test obu kierunków 115200/460800, liczba bytes/errors/drops i baud tolerance; większe baud osobno. |
| Link faults | Partial/corrupt frame, heartbeat timeout, CRC error, flow credits exhaustion i reset peer nie blokują IO. |
| Actions/config | Accepted ≠ applied; session reset daje unknown, no unsafe replay, pending revision odczytana po recovery. |
| Proxy registry | Bounded schemas/IDs, typ nieobsługiwany odrzucony, remote property invalid/stale po utracie linku. |
| Single/dual profiles | Niezależny budżet physical expansions / I2C instances / exported peer modules, brak konfliktu chosen pair. |
| Offload effect | Flash/map/heap/task/bus pomiary hosta przed/po, pełny wyniesiony stos nie linkowany lokalnie. |

Rozdział 21 korzysta z dokumentacji ESP32 UART i H2/C5/C6 wymienionej
w rozdziale 20.2. Informacje o złączu P17 pochodzą z uściślenia użytkownika;
nie stanowią nowego pomiaru PCB ani zatwierdzenia zasilania/poziomów.
