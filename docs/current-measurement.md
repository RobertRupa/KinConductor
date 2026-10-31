# Calibrated current-measurement interfaces

[Documentation index](./README.md) · [Project README](../README.md)

## What is implemented

The **Sensors** page configures four independent channels mapped to the existing
ADC1 inputs A1/GPIO36, A2/GPIO39, A3/GPIO34 and A4/GPIO35. Each channel supports:

- OFF, voltage signal (0–5 V at the **conditioned board terminal**, not GPIO),
  or current-loop signal (for example 4–20 mA through a suitable receiver).
- Two measured raw ADC endpoints, corresponding signal endpoints in V/mA,
  and known measured-current endpoints in amperes.
- An explicit calibration confirmation; every channel is OFF and uncalibrated
  by default. Changing the signal or capturing an endpoint clears confirmation.
- Capturing the current **valid** ADC reading for either endpoint.
- NVS persistence in a separately versioned `current_cfg` record.

The `current.main` module implements lifecycle, properties and IFTTT condition
interfaces. Each channel exposes `current.main/channel/N/amperes` (A) and
`current.main/channel/N/signal` (V or mA), usable in IO, canonical IFTTT
conditions and OLED grid widgets. There are no current-control actions.

Conversion is linear:

```text
ratio = (raw - rawLow) / (rawHigh - rawLow)
signal = signalLow + ratio * (signalHigh - signalLow)
amperes = currentLow + ratio * (currentHigh - currentLow)
```

Endpoints must be ordered, finite and inside the documented range. Raw ADC
counts must be 0–4095. Samples outside the calibrated interval become errors:
they are not extrapolated or clamped to a plausible current. Raw quality and
timestamps propagate to the derived properties. Disabled channels report
`disabled`; enabled but uncalibrated channels report `warming_up`. Storage or
ADC errors do not produce valid measurements.

`GET /api/current` returns `{schemaVersion:1,channels:[...]}` with exactly four
channels. Authenticated `POST /api/current` accepts the same shape; each channel
requires `signal` (0/1/2), `calibrated` (boolean), `rawLow`, `rawHigh`,
`signalLow`, `signalHigh`, `currentLow` and `currentHigh`.
Bad input returns HTTP 400 without applying the candidate. Write/readback
failures mark calibration unavailable. A valid POST repairs it. When reading
fails, the UI offers an **explicit replacement** form with disabled channels;
saving it replaces the previous record, never silently resets it.

## Hardware and calibration

The supplied manufacturer schematic labels **all four** analog conditioning
circuits as 0–5 V and shows 5.1 kΩ/10 kΩ components and BAT54S clamps.
It does not establish a built-in 4–20 mA receiver for A3/A4. A block diagram
alone is insufficient evidence. Verify the actual PCB revision before wiring.

For inexpensive **low-voltage DC** experiments, a genuine ACS712 5 A module
is a common starting point: typically a 5 V supply, approximately 2.5 V
zero-current output and nominal 185 mV/A sensitivity. Use its actual datasheet
and measured output, not those nominal values as a calibration. Configure a
voltage channel and measure two known currents with a reference meter, capturing
the corresponding ADC counts and signal voltages. Select a current range that
never drives the board terminal outside its allowed voltage.

Cheap breakout modules are **not a recommendation for mains wiring** or a
certified safety/protection measurement. Supply, ground, isolation, terminals
and protection need appropriate electrical design. Keep all ADC wiring away
from mains voltages.

For AC fan-load measurement, prefer a suitably rated **isolated true-RMS
transducer with a 4–20 mA output**. Its internal waveform processing produces
the slow signal that this firmware can scale. A raw ACS712/SCT-013 waveform
cannot produce AC RMS with this sampling/conversion module.

A voltage-only board needs a compatible loop receiver. For example a properly
specified external 150 Ω precision shunt converts 4–20 mA to 0.6–3.0 V
(`V = I × R`); this is a scaling example, not a complete wiring design.
Loop power, return path, shunt loading/dissipation, compliance voltage and
isolation must be verified for the selected transmitter and board. Capture
both raw endpoints through the **complete assembled input chain**.

Do not derive terminal voltage from `raw/4095 × 3.3 V`: board dividers,
ADC attenuation/nonlinearity and receiver tolerances are part of calibration.
Two-point scaling does not correct every ADC nonlinearity. There is no waveform
RMS integration, filtering, automatic reference calibration, electrical
overcurrent shutdown or certified accuracy.
