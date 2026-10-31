# Manufacturer documentation and compatibility

[Documentation index](./README.md) · [Project README](../README.md)

External references below remain manufacturer-owned. This repository contains
original implementation notes and links, not redistributed images, PDF pages
or copies of manufacturer firmware.

| Reference | Purpose |
| --- | --- |
| [KC868-A6 ESP32 I/O pin definition](https://www.kincony.com/forum/showthread.php?tid=1962) | Board-specific pin assignments and evolving corrections |
| [KC868-A6 forum](https://www.kincony.com/forum/forumdisplay.php?fid=22) | Board wiring, examples and manufacturer discussion |
| [KCS v2 firmware](https://www.kincony.com/esp32-kcsv2-firmware.html) | Manufacturer firmware overview and documentation |
| [KCS firmware forum](https://www.kincony.com/forum/forumdisplay.php?fid=40) | Manufacturer firmware support and usage discussion |
| [KCS firmware for ESP32 boards](https://www.kincony.com/kcs-firmware-esp32-board.html) | Manufacturer firmware family and supported boards |
| [KC868-A6 product and schematic](https://www.kincony.com/esp32-6-channel-relay-module-kc868-a6.html) | Board product resources and schematic download |

KinConductor is a separate firmware implementation. These references are
hardware evidence and feature context, not a promise of KCS API/protocol,
configuration, binary-image or electrical compatibility.

## Revision checks from the supplied schematic

- Six relay channels, eight digital inputs and ADC GPIO36/39/34/35 match the
  current firmware. Do not substitute an unrelated eight-relay block diagram.
- The supplied schematic shows all four analog circuits as **0–5 V** inputs.
  No dedicated 4–20 mA shunt for A3/A4 was established; see
  [current-measurement hardware requirements](./current-measurement.md).
- The DA terminal paths include an LM258DR dual op-amp powered from the board's
  12 V supply. DA terminals are conditioned analog outputs, distinct from the
  ESP32 3.3 V GPIO/DAC domain. Nominal 0–10 V operation and usable output
  limits must be checked for the actual revision/load.
- A GPIO PWM waveform is not automatically a clean calibrated 0–10 V DC signal
  at the DA terminal. Verify amplification, filtering, polarity, startup state
  and controller requirements before using it with a fan controller.
- The current manufacturer pin-definition post agrees with schematic LoRa
  RESET GPIO21/DIO0 GPIO2; older replies contain different assignments.
- Manufacturer RS232 TX17/RX16 conflicts with the existing firmware's
  TX16/RX17 convention. DAC connector naming also warrants revision checking.
  Runtime UART/DAC routing was **not changed from an unverified diagram**.
  Confirm terminal orientation and the actual schematic/PCB before a peer test.
- P17 GPIO12/13 can be routed to an existing hardware UART, not a fourth UART.
  GPIO12 is a strapping pin and GPIO13 is currently PN532 SPI CS. Coprocessor
  role selection must respect these conflicts.

## KC868-Ax models listed with ESP32 KCS documentation

The manufacturer KCS ESP32 pages list KC868 models including:

- KC868-A4
- KC868-A6
- KC868-A8
- KC868-A8S
- KC868-A16
- KC868-A32
- KC868-A64
- KC868-A128

Source pages:

- [KCS firmware for ESP32 boards](https://www.kincony.com/kcs-firmware-esp32-board.html)
- [KCS v2 firmware](https://www.kincony.com/esp32-kcsv2-firmware.html)

These pages also distinguish ESP32-WROOM-32E/32UE vs ESP32-S3 targets in the
flashing tool. Treat model-family inclusion as firmware-family compatibility
context, not a guarantee that one pin map or one image is safe for all boards.
