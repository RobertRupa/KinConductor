# First start and web access

[Documentation index](./README.md) · [Project README](../README.md)

1. Connect the controller over USB, identify its serial port and build/flash
   the **matching** firmware and LittleFS assets. For the base KC868-A6:

	```sh
	pio run -e kc868-a6
	pio run -e kc868-a6 -t upload --upload-port /dev/ttyUSB0
	pio run -e kc868-a6 -t uploadfs --upload-port /dev/ttyUSB0
	```

	Use the correct COM port on Windows. Firmware and filesystem are separate
	uploads; do not combine the targets in one command.
2. Join the open `KC868-A6-XXXX` access point and open `http://192.168.4.1`.
	Setup mode does not request Wi-Fi or web login passwords.
3. Choose a Wi-Fi network in the panel. Credentials are stored in ESP32 NVS;
	after a successful connection, open the IP shown in serial or
	`http://kinconductor.local`.

The open setup access point starts when no saved network is available. It
requires no console to discover a password. Web authentication is bypassed only
while this AP is active; anyone nearby can access setup and relay controls, so
finish provisioning promptly. On the regular Wi-Fi network, the dashboard uses
HTTP Basic Authentication with the generated `admin` password stored in NVS
and printed to serial. HTTP is not encrypted; keep it on a trusted LAN and do
not forward its port to the internet.

The dashboard's right-hand event stream reports IFTTT triggers and action
failures, relay/DI/S2 changes, GPIO sensor configuration, and beeper tests. It
keeps the 24 most recent events in RAM and refreshes with the status view.

## Hostname, mDNS and UDP

**Settings → Network** saves a hostname in NVS, default `kinconductor`.
Use one DNS label (1–63 ASCII letters/digits/hyphens), without `.local`;
no leading/trailing hyphen. Names normalize to lowercase. Saving a changed
name reconnects Wi-Fi after the HTTP response; DHCP may assign a different IP.
Use `http://<hostname>.local/` if the client's network supports mDNS.
The responder advertises HTTP/TCP port 80. Its actual readiness is reported,
not assumed; it is stopped on disconnect and retried after reconnection/failure.
There is no static-IP/DNS-server/multicast-interface editor in this slice.

**Interfaces → UDP** sets `udpListenPort` (0 disables receiving); the same
field is available in schema-driven Settings. Binding is fixed to `0.0.0.0`,
meaning all local IPv4 interfaces, **not the remote destination**. Send a
test packet to the board IP and that port. Configure outgoing destination
host/IP and port separately in the UDP Send action in IO/IFTTT.
`GET /api/network` and `/api/status` expose hostname, mDNS readiness, IP and
listener address/port. There is no configurable UDP bind-address selector yet.

The WebGUI pages and their controls are described in the
[WebGUI guide](./webgui.md), which includes screenshots from a working device.
The header reports chip model, core count and configured MHz, free/total heap,
PSRAM availability and physical flash/application size. Newer firmware may
also report remaining OTA application space and per-core CPU utilization.
MHz is not CPU load, and heap capacity is not a claim about all physical SRAM.

## Next configuration steps

After initial Wi-Fi setup, open **Diagnostics** first and verify the board
identity, pin map and resource conflicts. Configure **Sensors** and
**Interfaces** before using physical actions, then inspect **IO** and
add IFTTT rules cautiously. Use [WebGUI walkthrough](./webgui.md),
[Settings and parameter ranges](./settings-reference.md) and the
[HTTP API reference](./api-reference.md). If the UI is missing or the API
returns HTML, see [Troubleshooting](./troubleshooting.md); do not erase NVS.
