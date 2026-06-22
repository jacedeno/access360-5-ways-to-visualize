# Method 5 — SenseCAP Indicator D1L (On-Device Health Monitor) ⭐

**Tier:** Hero (fleet-health visualization) · **Platform:** Seeed Studio SenseCAP
Indicator D1L (ESP32-S3, 4" touchscreen)

A dedicated piece of hardware that sits on your desk and shows the **health of the
whole fleet** at a glance — the physical embodiment of "Spectra Fleet Health." The
[SenseCAP Indicator D1L](https://www.seeedstudio.com/SenseCAP-Indicator-D1L-p-5644.html)
is an ESP32-S3 device with a 4" 480×480 capacitive touchscreen. It runs custom
firmware that connects **directly to HiveMQ** and renders a multi-screen,
swipe/tap-navigable status panel.

> **Scope (important):** this device shows **status / health only** — broker up,
> sensors online, battery %, RSSI, last-seen age, 4G usage. It deliberately renders
> **NO raw vibration waveforms and NO FFT/spectrogram plots**. Signal visualization
> is Method 4's job; this is the always-on "is everything OK?" monitor.

## Purpose

- An always-on, no-browser, no-PC hardware indicator of fleet health.
- Mirror the Spectra Fleet Health metric set (see
  [`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md)) on a
  small touchscreen with **back / next** navigation across screens.
- Prove the broker stream alone (plus a tiny SIM-usage helper) is enough to drive a
  genuine operations panel on a $40 device.

## Toolchain (exact)

| Component | Version / tool | Notes |
|---|---|---|
| Framework | **ESP-IDF v5.1.x** (exact — use the 5.1 release branch) | Espressif IoT Development Framework |
| GUI library | **LVGL 9** | UI widgets / screens |
| UI design | **SquareLine Studio** | Visual layout → exports LVGL 9 C code |
| MQTT client | **esp-mqtt** (native ESP-IDF component) | Direct TCP MQTT to HiveMQ |
| Target SoC | ESP32-S3 (SenseCAP Indicator D1L) | 4" 480×480 touchscreen |
| Dev/build host | **Fedora dev laptop** | ESP-IDF toolchain on Fedora; build + flash over USB-C |

## Prerequisites

- SenseCAP Indicator D1L hardware + USB-C cable.
- A **Fedora** development laptop with:
  - ESP-IDF **v5.1.x** installed and exported (`. $IDF_PATH/export.sh`).
  - USB access to flash the ESP32-S3 (`dialout` group / udev rules).
- **SquareLine Studio** for designing the LVGL 9 screens (exports C into the
  project).
- Wi-Fi credentials for a network on the **private plane** (same LAN as
  `192.168.68.150`, or routed via the homelab VPN) — the device must reach the
  broker.
- Broker details — [`../../docs/backend-context.md`](../../docs/backend-context.md):
  `192.168.68.150:1883`, **no TLS, anonymous**, MQTT 5.0 (esp-mqtt speaks 3.1.1/5).
- Fleet identifiers to display — [`../../docs/sensors-and-gateways.md`](../../docs/sensors-and-gateways.md):
  gateway `43250372`; sensors `22255728` (WS300), `11251280`, `11251423` (WS200).

## Screen design (multi-screen "Health" monitor)

Four screens with back/next navigation, mirroring the fleet-health metric set:

1. **Broker / Gateway**
   - Broker connected (LED/badge) — from the esp-mqtt connection state.
   - Gateway online — from `access360/43250372/ap/notify` (`Connected`) and the
     `will` Last-Will topic.
   - Messages/s — count of any `access360/#` message over a sliding window.
   - Last error — latest `error/notify` `Error` string.
2. **Sensors**
   - Online count — sensors with `now − last_seen < 600 s`.
   - Per-sensor **last-seen age** (s) — `now − last_message_time[Serial]`, updated
     on any message (esp. `proc/checkin/notify`).
3. **Power & Signal**
   - Per-sensor **battery %** — `Batt` from `dyn/batt/notify` (and
     `proc/reading/notify` for WS100). Red `< 20 %`.
   - Per-sensor **RSSI (dBm)** — `Rssi` from `rssi/notify`. Red `< -75 dBm`.
4. **4G / SIM**
   - Data used vs limit (%), SIM live/paused, last-connect age.

## How it gets its data

- **Direct MQTT subscriptions** (esp-mqtt) to the health topics — no aggregator:
  - `access360/43250372/proc/checkin/notify` → presence / last-seen / online count
  - `access360/43250372/dyn/batt/notify` → battery %
  - `access360/43250372/rssi/notify` → RSSI
  - `access360/43250372/error/notify` + `status/notify` → errors / status
  - `access360/43250372/ap/notify` + `will` → gateway/broker presence
- **Local state on the device:** keep a small table keyed by `Serial` holding last
  battery, last RSSI, and last-seen timestamp; recompute "online" and "age" on a
  timer (the ESP32 needs SNTP time sync for absolute ages, or it can track relative
  ages from boot).
- **4G usage:** the SIM figure (Hologram `cur_billing_data_used` vs `overagelimit`)
  is **not** on MQTT. Either:
  - (preferred) a tiny **Hologram-API poller** publishing a summary topic, deployed
    as a **Docker container on `192.168.68.150`** (keeps the `HOLOGRAM_API_KEY`
    server-side — never on the device), and the Indicator subscribes to that topic;
    or
  - the MQTT **byte-count proxy** (sum payload sizes per sensor) for a rough usage
    estimate, computed on-device.

Thresholds for coloring come straight from
[`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md).

## Build & flash (outline — firmware lands in Phase 2)

```bash
# On the Fedora dev laptop, with ESP-IDF v5.1.x exported:
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py menuconfig        # set Wi-Fi SSID/PSK and MQTT broker 192.168.68.150:1883
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

UI is designed in SquareLine Studio and exported into the project's `ui/`
component (LVGL 9); the firmware wires esp-mqtt callbacks to the exported widgets.

## Notes

- **No raw signal plots on the device** — by design. Keep it a status panel.
- Keep the `HOLOGRAM_API_KEY` off the device; only the `.150` helper holds it.
- Anonymous broker means no credentials in firmware — only Wi-Fi creds (set via
  `menuconfig` / NVS, not committed).

## Phase 2 (later)

Commit the ESP-IDF project (`main/`, `CMakeLists.txt`, `sdkconfig.defaults`), the
SquareLine Studio export (`ui/`), the optional Hologram-poller container, and a
photo of the running device.
