# Method 2 — IoT MQTT Panel (Android)

**Tier:** Quick look · **Platform:** Android phone / tablet

A pocket dashboard. [IoT MQTT Panel](https://snrlab.in/iot/iot-mqtt-panel-user-guide/)
is an Android app that binds UI widgets (gauges, values, LED indicators, line
graphs) directly to MQTT topics. Point it at the broker and you get a glanceable
fleet-health view on your phone — no server, no backend.

## Purpose

- A phone-sized **fleet-health** glance: battery %, RSSI, last reading, online LED.
- Walk the plant floor and watch live sensor values next to the machine.
- Zero infrastructure — the phone is the MQTT client.

## Prerequisites

- Android device with [IoT MQTT Panel](https://play.google.com/store/apps/details?id=snr.lab.iotmqttpanel.prod)
  installed.
- The phone must be on the **private plane**: same Wi-Fi LAN as `192.168.68.150`,
  or connected through the homelab VPN.
- Broker details from [`../../docs/backend-context.md`](../../docs/backend-context.md):
  `192.168.68.150:1883`, TLS off, anonymous.
- Sensor IDs to bind to — see [`../../docs/sensors-and-gateways.md`](../../docs/sensors-and-gateways.md)
  (`22255728` WS300, `11251280` / `11251423` WS200).

## Setup

1. **Add a connection** (Connections → +):
   - Connection name: `ACCESS360`
   - Broker Web/Host address: `192.168.68.150`
   - Port: `1883`
   - Client ID: auto / `iotpanel-<yourname>`
   - Username / Password: leave blank
   - Uncheck SSL/TLS.
2. **Create a dashboard** and add panels. Bind each widget to a topic and a JSON
   field (IoT MQTT Panel supports JSON path extraction):

   | Widget | Topic | JSON field | Notes |
   |---|---|---|---|
   | Value / Gauge | `access360/43250372/dyn/batt/notify` | `Batt` | Battery %, 0–100 |
   | Gauge | `access360/43250372/rssi/notify` | `Rssi` | dBm, range -90…-40 |
   | Value | `access360/43250372/dyn/vib/notify/lite` | `Xrms` | Overall RMS (g) |
   | LED Indicator | `access360/43250372/proc/checkin/notify` | `Serial` | Lights on heartbeat = online |
   | Log / Text | `access360/43250372/error/notify` | `Error` | Last error string |

   Tip: create one set of widgets **per sensor** by filtering on the `Serial`
   field, or use separate topic subscriptions with the sensor in a sub-panel.
3. **(Optional) Battery refresh button.** Add a Button widget that **publishes**
   `{ "Serial": 22255728 }` to `access360/43250372/dyn/batt/trigger`. Tapping it
   forces a fresh `dyn/batt/notify` within ~10 s.
4. **Apply thresholds / colors** to match the fleet-health rules in
   [`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md):
   battery red `< 20 %`, RSSI red `< -75 dBm`.

## Notes

- Subscribe only to the small JSON topics. **Do not** subscribe to
  `dyn/vib/notify` (full waveform, multipart) — the phone can't reassemble it and
  it wastes data. Use `dyn/vib/notify/lite` for scalar values.
- IoT MQTT Panel can export/import the dashboard as JSON — keep that export in
  this folder in Phase 2.

## Phase 2 (later)

Add the exported panel configuration (`iot-mqtt-panel-config.json`) and a phone
screenshot of the live dashboard.
