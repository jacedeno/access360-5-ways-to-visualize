# Method 2 — IoT MQTT Panel (Android)

**Tier:** Quick look · **Platform:** Android phone / tablet

A pocket dashboard. [IoT MQTT Panel](https://snrlab.in/iot/iot-mqtt-panel-user-guide/)
is an Android app that binds UI widgets (gauges, values, LED indicators, line
graphs) directly to MQTT topics. Point it at the stack's broker and you get a
glanceable fleet-health view on your phone — no app server, the phone *is* the
MQTT client.

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
   | Value | `access360/43250372/dyn/vib/notify/lite` | `Reading.Xrms` | Overall RMS (g) — nested |
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

## What's in this folder (Phase 2 — delivered)

| File | What it is |
|---|---|
| [`iot-mqtt-panel-config.json`](iot-mqtt-panel-config.json) | Importable IoT MQTT Panel config: one connection to `192.168.68.150:1883` (no TLS, anonymous) plus a **Fleet Health** dashboard of panels bound to the health topics, with battery/RSSI thresholds and a battery-refresh button. |

The payloads on every health topic are **JSON**, so each panel parses a single
field via the app's JSON-path extraction. Most topics are **flat** — the field sits
at the top level, so the JSON path is just the field name (e.g. `Rssi`, `Batt` on
`dyn/batt/notify`, `Serial`). But **`proc/reading/notify` and `dyn/vib/notify/lite`
are nested under a `Reading` object** (confirmed by the live captures in
[`../01-mqttx/sample-payloads/`](../01-mqttx/sample-payloads/)), so their panels use
the **dotted** path: `Reading.Batt`, `Reading.Temp`, `Reading.Xrms`. The dashboard
avoids the heavy full `dyn/vib/notify` waveform entirely.

> If your app build can't do dotted JSON paths, point the WS100 battery/temp panels
> at the **flat** `dyn/batt/notify` (`Batt`) / `dyn/temp/notify` (`Temp`) topics
> instead.

### Import the config (Android)

1. Copy `iot-mqtt-panel-config.json` to the phone (USB, Drive, email, etc.).
2. In IoT MQTT Panel: top-right **⋮ menu → Import** (some builds: **Settings →
   Import/Export → Import configuration**).
3. Pick the file. The **ACCESS360** connection and the **Fleet Health** dashboard
   load together.
4. Open the connection and tap **Connect**.

### Panel → topic → field mapping

| Panel | Type | Topic | JSON field | Threshold / note |
|---|---|---|---|---|
| RSSI 11251722 (WS100) | Gauge | `rssi/notify` | `Rssi` | red `< -75 dBm`, range -90…-40 |
| RSSI 11252280 (WS100) | Gauge | `rssi/notify` | `Rssi` | red `< -75 dBm` |
| RSSI trend (all sensors) | Line graph | `rssi/notify` | `Rssi` | live BLE trend |
| Battery 22255728 (WS300) | Gauge | `dyn/batt/notify` | `Batt` | red `< 20 %`, 0–100 |
| Battery 11251423 (WS200) | Gauge | `dyn/batt/notify` | `Batt` | red `< 20 %` |
| Battery WS100 (proc/reading) | Gauge | `proc/reading/notify` | `Reading.Batt` | red `< 20 %` — WS100 reports battery **inside** its (nested) reading |
| Temperature WS100 | Gauge | `proc/reading/notify` | `Reading.Temp` | °C — nested |
| Temperature (dyn sensors) | Value | `dyn/temp/notify` | `Temp` | °C — flat |
| Overall RMS (X) | Value | `dyn/vib/notify/lite` | `Reading.Xrms` | g — overall only, nested under `Reading` |
| Heartbeat (online) | LED | `proc/checkin/notify` | `Serial` | lights on every heartbeat; online cutoff 600 s |
| Last sensor to check in | Value | `proc/checkin/notify` | `Serial` | most-recent heartbeat serial |
| Last error | Text/log | `error/notify` | `Error` | last gateway error string |
| Refresh battery 22255728 | Button (publish) | `dyn/batt/trigger` | — publishes `{"Serial": 22255728}` | forces a fresh `dyn/batt/notify` in ~10 s |

> **Per-sensor caveat.** The fleet publishes **all** sensors on the **same** topic
> (the sensor serial is in the payload `Serial` field, not the topic). A single
> gauge bound to `rssi/notify` therefore shows the **most recent** RSSI across all
> sensors, not one specific sensor. The two RSSI / battery gauges are labelled per
> sensor for layout, but to truly pin one sensor you need payload-`Serial`
> filtering — supported on some IoT MQTT Panel builds and not others. If your build
> lacks it, read the value next to the **Heartbeat** / **Last sensor to check in**
> panels for context, or split by adding `Serial` to a text panel.

> **Schema note / assumptions.** This config is a **best-effort** reconstruction of
> IoT MQTT Panel's export shape (`connections` / `dashboards` / `panels` arrays,
> with `jsonpathEnabled` + `jsonpath` for field extraction and `redBelow` for
> thresholds). Exact key names and panel-type strings vary by app version. If the
> import is rejected or a panel looks wrong:
> 1. Create **one** connection + **one** gauge by hand in the app (values from
>    **Setup** above and the table here).
> 2. Use the app's **Export** to produce a known-good JSON for your build.
> 3. Diff it against this file and adjust key names, then re-import the full set.
>
> The `_comment` / `comment` fields are documentation only; remove them if your
> build is strict about unknown keys.

### Still to do

- Verify the import on a real device and **re-export** to lock your app version's
  exact schema (panel-type names, threshold keys).
- Confirm the nested-path panels (`Reading.Batt`/`Reading.Temp`/`Reading.Xrms`)
  render once `proc/reading/notify` / `dyn/vib/notify/lite` fire (slow cadence; real
  samples are in [`../01-mqttx/sample-payloads/`](../01-mqttx/sample-payloads/)).
- Add per-sensor `Serial` filtering if your build supports it (otherwise the
  per-sensor RSSI/battery gauges show the latest value across the fleet).
- Add a phone screenshot of the live dashboard.
