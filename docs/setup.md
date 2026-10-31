# Requirements, installation and first build

[Documentation index](./README.md) · [Profiles](./profiles.md) · [Flashing](./build.md) · [First start](./first-start.md)

This procedure follows the repository's **1.1.3** build configuration. The
maintained and electrically documented target is the KinCony **KC868-A6 with
ESP32-WROOM-32 and 4 MiB flash**. Other Ax build aliases are not verified
hardware ports. No PSRAM is assumed. A USB-to-serial adapter/cable and access
to the board's programming serial port are required for serial flashing.

## Host requirements

| Item | Purpose |
| --- | --- |
| Ubuntu/Linux, macOS or Windows | PlatformIO host; commands below use Ubuntu/Linux |
| Git | Clone and update the repository |
| Python 3 and venv | Run pinned PlatformIO Core 6.2.0 |
| Internet access during first build | Download pinned pioarduino toolchain and libraries |
| C++ compiler, make | Native ASan/UBSan tests on Linux |
| Node.js 18+ | Optional built-in JavaScript tests and release packager; no npm install needed |
| Serial device access | Flash and monitor the ESP32; Linux account usually needs `dialout` |
| VS Code + PlatformIO IDE extension (optional) | Alternative to the `pio` CLI |

The CI reference is Ubuntu 24.04, Python 3.12 and Node 22. The
`platformio.ini` file pins pioarduino platform
`55.03.312-1`, Arduino ESP32 **3.3.12** / ESP-IDF **5.5.5**, C++17 and
specific library versions. Do not replace these with unverified versions when
reproducing a build.

## Ubuntu preparation

```bash
sudo apt update
sudo apt install -y git python3 python3-venv python3-pip build-essential nodejs npm
python3 -m venv "$HOME/.venvs/kinconductor-pio"
"$HOME/.venvs/kinconductor-pio/bin/python" -m pip install --upgrade pip
"$HOME/.venvs/kinconductor-pio/bin/python" -m pip install 'platformio==6.2.0'
export PATH="$HOME/.venvs/kinconductor-pio/bin:$PATH"
pio --version
node --version
git clone https://github.com/RobertRupa/KinConductor.git
cd KinConductor
```

If the distribution's `node --version` is older than 18, install a
supported Node.js version before running tests; firmware building does not
require Node. The Python venv is outside the repository to avoid accidentally
committing it. You can activate it in each new shell using
`source "$HOME/.venvs/kinconductor-pio/bin/activate"`.

On Linux, identify the **actual** device with `pio device list` or
`ls -l /dev/serial/by-id/` (often `/dev/ttyUSB0`). If permissions deny
access, add the current user to `dialout` with
`sudo usermod -aG dialout "$USER"`, then sign out and back in. Do not
blindly assume `/dev/ttyUSB0` belongs to the KC868-A6.

For VS Code, open the repository root containing `platformio.ini`, install
the PlatformIO IDE extension and select an environment before Build/Upload.
The CLI and VS Code use the same checked-in build configuration.

## Build and test without touching hardware

```bash
pio run -e kc868-a6
pio run -e kc868-a6 -t buildfs
pio run -e kc868-a6-coprocessor
pio run -e kc868-a6-coprocessor -t buildfs
PIO_ENV=kc868-a6 npm test
```

Build the firmware **before** native tests on a fresh checkout: the native
runner needs ArduinoJson installed by PlatformIO in `.pio/libdeps`.
Do not run dependency-resolving builds and native tests concurrently.
`npm test` runs the native ASan/UBSan and Node built-in tests without
`npm install`. Useful shortcuts are `npm run build:a6`,
`npm run build:matrix`, `npm run build:fs` and
`npm run build:ax-base`. The build artifacts are under
`.pio/build/<environment>/`; `firmware.bin` is the application and
`littlefs.bin` is the UI filesystem image.

## Choose the right build

For a standard KC868-A6 use `kc868-a6`. The default environment if `-e`
is omitted is **`kc868-a6-coprocessor`**, a diagnostic UART1 default,
not an operational Zigbee/Thread bridge. Profiles, GPIO caveats and
experimental compile-time flags are covered in [Profiles](./profiles.md).
Persisted NVS settings take precedence over a different build's defaults.

## Serial upload: two separate operations

**Only proceed after verifying board identity, port, correct power and safe
loads.** Reset/reboot can change relay outputs and activate saved IFTTT rules.
Use a safe disconnected load or test bench. Uploading is not a read-only test.

```bash
# Replace /dev/ttyUSB0 with the verified programming adapter.
pio run -e kc868-a6 -t upload --upload-port /dev/ttyUSB0
pio run -e kc868-a6 -t uploadfs --upload-port /dev/ttyUSB0
pio device monitor -p /dev/ttyUSB0 -b 115200
```

On Windows use the actual COM port (e.g. `COM5`).
Do **not** combine `upload` and `uploadfs` into one invocation. Both must
come from the same source version. Normal uploads preserve NVS; avoid
`erase-flash`, factory images and partition changes on a routine update.
`uploadfs` replaces LittleFS pages/assets, not NVS. The separate
[release chapter](./releases.md) documents factory images and flash offsets.

## First connection and verification

On a device without saved Wi-Fi, join its **open** `KC868-A6-XXXX` setup
AP, browse to `http://192.168.4.1/` and configure a network. On the LAN,
browse to `http://kinconductor.local/` (if mDNS works) or the IP address
shown in the serial log. The web user is `admin`; its generated password is
printed to serial and stored in NVS. The setup AP bypasses authentication;
disable exposure by completing provisioning promptly. HTTP Basic Auth is
**not encrypted**: use only a trusted LAN, never port-forward this UI.

Read-only API smoke check, on the trusted LAN:

```bash
read -r -s -p 'Web password: ' KC_WEB_PASSWORD; echo
curl -fsS --user "admin:$KC_WEB_PASSWORD" http://kinconductor.local/api/status
curl -fsS --user "admin:$KC_WEB_PASSWORD" http://kinconductor.local/api/board
curl -fsS --user "admin:$KC_WEB_PASSWORD" http://kinconductor.local/api/capabilities
curl -fsS --user "admin:$KC_WEB_PASSWORD" http://kinconductor.local/api/settings/schema
unset KC_WEB_PASSWORD
```

Use an IP instead if mDNS is unavailable. Do not paste credentials or
identifying network data into issues. Confirm relay state, firmware version,
GPIO resource conflicts and detected devices in **Diagnostics** before
enabling rules. Screenshots in the WebGUI guide were captured on *1.1.0*,
not proof of physical validation for firmware 1.1.3. See
[WebGUI](./webgui.md) and [Troubleshooting](./troubleshooting.md).
