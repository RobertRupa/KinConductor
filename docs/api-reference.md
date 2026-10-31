# HTTP API reference

[Documentation index](./README.md) · [Settings](./settings-reference.md) · [WebGUI](./webgui.md)

The firmware implements a synchronous HTTP server on port 80. Its WebGUI
assets are served from LittleFS (`/`, `/app.js`, `/capabilities.js`,
`/app.css`). All routes use **HTTP Basic Auth** (`admin` and the
device-generated NVS password) in station mode; authentication is bypassed
while the open setup AP is active. The transport is plaintext HTTP and
is not suitable for exposure to the internet.

This route inventory comes from `WifiWebUi::startServer()` in
[src/wifi_web_ui.cpp](../src/wifi_web_ui.cpp). GET endpoints are read-only
unless otherwise noted. POST operations may change persistent settings,
relays, PWM, sends or rules; use them only with a safe, verified device.

| Method | Route | Purpose / notes |
| --- | --- | --- |
| GET | `/api/status` | Live status, event log, Wi-Fi, hostname/mDNS, device telemetry |
| GET | `/api/board` | Board options, GPIO/resource plan, conflicts |
| GET | `/api/capabilities` | Dynamic modules, typed properties with quality, triggers and action descriptors |
| POST | `/api/actions` | Execute a typed capability action; see example below |
| GET | `/api/settings/schema` | All 33 settings with range, allowed values, `derived` flag |
| GET / POST | `/api/settings` | Flat settings JSON (POST may update a subset) |
| GET / POST | `/api/network` | Hostname and network readiness (POST expects versioned record) |
| GET / POST | `/api/current` | Exactly four calibrated current channel records |
| GET / POST | `/api/screen` | OLED grid record with pages/widgets |
| GET | `/api/scan` | Wi-Fi network scan for provisioning |
| POST | `/api/wifi` | Store Wi-Fi credentials/connect |
| POST | `/api/relay` | Legacy physical relay control |
| POST | `/api/beeper/test` | Start test beep, when enabled |
| GET / POST | `/api/ifttt` | Rule array, maximum four |
| GET | `/api/bluetooth/scan` | BLE scan results; mode must permit scanning |
| GET / POST | `/api/bluetooth/target` | Legacy single-MAC record; cannot overwrite a multi-target list |
| GET / POST | `/api/bluetooth/targets` | Versioned multi-MAC record (up to eight) |
| GET / POST | `/api/rf24/packets` | RF24 history/saved replay entries, **optional compiled feature** |
| POST | `/api/rf24/replay` | RF24 transmit from saved packet, **optional compiled feature** |

**RF24 is not included in either maintained A6 profile.** Treat a route
existing in the server as distinct from an active radio driver; the UI
hides unsupported RF24 functions based on capabilities. Avoid using the
legacy BLE target POST on configurations with more than one saved MAC.

## Contract examples

`GET /api/capabilities` returns `{schemaVersion:1,revision,modules:[...]}`.
Each module reports its ID/type, `ready`, `error`, typed `properties`,
`triggers` and `actions`. Property entries include `id`, `type`,
`quality`, `updatedAtMs`, `error`, `unit` and `value`. Non-valid
quality states (`stale`, `unavailable`, `error`, `disabled`,
`warming_up`) have a null value, not a valid zero. Action parameter
descriptors specify ID/type/required and optional min/max.

An action POST is **exactly** an object containing `id` and
`parameters`:

```json
{"id":"fan.main/set","parameters":{"percent":50}}
```

This is an **example of a state-changing request**, not an instruction
to run it on an active load. The response includes `applied` and
`confirmed` booleans; a successful HTTP response alone must not
be interpreted as electrical feedback. Other implemented action IDs
and parameter schemas must be discovered from `/api/capabilities`
rather than guessed. Example outgoing transport actions:

```json
{"id":"transport.main/udp/send","parameters":{"host":"192.168.1.20","port":1234,"data":"hello"}}
```

The settings POST is *not* wrapped in `{settings:...}`:

```json
{"sensorIntervalMs":1000,"udpListenPort":0}
```

The normalized settings object returned by the server contains all
fields, including values derived from sensor types. `/api/network`
uses a separate versioned request body:

```json
{"schemaVersion":1,"hostname":"kinconductor"}
```

`/api/current` uses `{schemaVersion:1,channels:[...]}`; send the
complete validated four-channel record as described in
[Current measurement](./current-measurement.md).
`/api/screen` uses `{schemaVersion:1,pages:[...],widgets:[...]}`;
see [Screen configuration](./webgui.md).
The `/api/ifttt` payload is an array, not a wrapped object.
Current, screen, BLE and IFTTT write formats should be read back from
their respective GET endpoint before editing.

## Failures and diagnostics

Invalid JSON, unknown setting, out-of-range parameter and malformed rule
requests produce JSON errors (often HTTP **400**). Conflicting resources,
UDP port collisions or unavailable features can give **409**/**503**;
oversized requests can give **413**; serialization/storage failures can
give **500**. A missing action ID returns **404**. Do not treat all
non-200 results as password failures. The JSON error response may include
`error` and a `field` indicating the rejected item.

If an API request returns **HTML** or the SPA displays a JSON parsing error,
check the device address and firmware/UI version. A preview of HTML files on
a PC cannot supply ESP32 API endpoints. `UI file not found; upload LittleFS
data` indicates missing assets; `/api/settings/schema` or
`/api/capabilities` returning 404 after an asset update usually indicates
firmware/UI mismatch. See [Troubleshooting](./troubleshooting.md).

`GET` example using a prompted password (trusted LAN):

```bash
read -r -s -p 'Web password: ' KC_WEB_PASSWORD; echo
curl -i --user "admin:$KC_WEB_PASSWORD" http://kinconductor.local/api/status
unset KC_WEB_PASSWORD
```

Do not commit API responses containing IPs, Wi-Fi names, BLE addresses or
other private data.
