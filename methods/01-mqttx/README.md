# Method 1 — MQTTX (Desktop)

**Tier:** Quick look · **Platform:** Windows / macOS / Linux desktop

The fastest way to confirm the fleet is alive and to read raw payloads with your
own eyes. [MQTTX](https://mqttx.app/) is a free, cross-platform MQTT 5.0 desktop
client. You connect, subscribe to the `access360/#` tree, and watch JSON arrive in
real time — perfect for verifying topics and payload schemas before building
anything heavier.

## Purpose

- Confirm the broker is reachable and the gateway is publishing.
- Inspect exact payloads for every channel (validate the schemas in
  [`../../docs/mqtt-topics.md`](../../docs/mqtt-topics.md)).
- Eyeball message cadence per topic (how often each sensor reports).
- Keep a couple of saved subscriptions as a lightweight, always-available
  "is it working?" panel.

This is a **health / inspection** tool, not a charting tool. For spectra use
Method 4; for an on-device health panel use Method 5.

## Prerequisites

- MQTTX desktop app ([download](https://mqttx.app/downloads)) — or the `mqttx`
  CLI if you prefer a terminal.
- Network access to the **private plane**: LAN `192.168.68.0/24` or homelab VPN.
- Broker connection details — see [`../../docs/backend-context.md`](../../docs/backend-context.md):
  - Host `192.168.68.150`, Port `1883`, TLS off, no auth.

## Setup

1. **Create a connection** in MQTTX:
   - Name: `ACCESS360 broker`
   - Host: `mqtt://192.168.68.150`
   - Port: `1883`
   - Username / Password: leave empty (anonymous)
   - MQTT Version: `5.0`
   - Client ID: anything unique, e.g. `mqttx-<yourname>`
2. **Connect.**
3. **Add subscriptions** (New Subscription, QoS 1):
   - `access360/#` — everything (start here)
   - `access360/43250372/dyn/vib/notify/lite` — overall vibration scalars
   - `access360/43250372/proc/checkin/notify` — heartbeats
   - `access360/43250372/dyn/batt/notify` — battery
   - `access360/43250372/rssi/notify` — BLE signal
4. **Watch the stream.** Click a message to pretty-print the JSON. Compare against
   the schemas in [`../../docs/mqtt-topics.md`](../../docs/mqtt-topics.md).

### CLI equivalent (no GUI)

```bash
# Whole tree
mosquitto_sub -h 192.168.68.150 -t 'access360/#' -v

# Just the live gateway, formatted with jq
mosquitto_sub -h 192.168.68.150 -t 'access360/43250372/#' | jq .
```

## Notes

- The big `dyn/vib/notify` (full waveform) payload arrives **multipart**
  (`{MultiPart_ID, Data}` fragments). MQTTX shows the raw fragments — that's
  expected; reassembly is Method 4's job. For a quick look, subscribe to
  `dyn/vib/notify/lite` instead, which is a single small message.
- You can **publish** a battery refresh from MQTTX: send `{ "Serial": 22255728 }`
  to `access360/43250372/dyn/batt/trigger`, then watch `dyn/batt/notify`.
- Save the connection + subscriptions; reopening MQTTX gives you an instant fleet
  sniff.

## What's in this folder (Phase 2 — delivered)

| File | What it is |
|---|---|
| [`connection.json`](connection.json) | MQTTX-importable connection profile: host `192.168.68.150:1883`, no TLS, anonymous, MQTT v5, clientId `mqttx-access360`, with five saved subscriptions (whole-gateway tree plus the four health topics). |
| [`sample-payloads/rssi-notify.json`](sample-payloads/rssi-notify.json) | Real captured `rssi/notify` message (sensor `11252280`, WS100). "Is data flowing?" evidence. |
| [`sample-payloads/proc-checkin-notify.json`](sample-payloads/proc-checkin-notify.json) | Real captured `proc/checkin/notify` heartbeat (sensor `11251722`, WS100). |
| [`sample-payloads/dyn-batt-notify.json`](sample-payloads/dyn-batt-notify.json) | Real captured `dyn/batt/notify` (sensor `22255728`, WS300), captured by triggering a refresh. |
| [`sample-payloads/proc-reading-notify.json`](sample-payloads/proc-reading-notify.json) | Real captured `proc/reading/notify` (sensor `11252280`, WS100). **Nested** under `Reading`; carries per-axis RMS (g) + `Temp` + `Batt`. |
| [`sample-payloads/dyn-temp-notify.json`](sample-payloads/dyn-temp-notify.json) | Real captured `dyn/temp/notify` (sensor `22255728`, WS300). Flat: `Temp` (°C). |
| [`sample-payloads/dyn-vib-notify-lite.json`](sample-payloads/dyn-vib-notify-lite.json) | Real captured `dyn/vib/notify/lite` (sensor `22255728`, WS300). **Nested** under `Reading`; overall scalars in **g** *and* velocity in **in/s** (no raw arrays). |

All sample payloads were captured live from the broker on **2026-06-22** with
`mosquitto_sub` (read-only). Each file has a `_comment` field naming its source
topic and sensor. Note the payload `Time`/`Timestamp` clocks read `2026-01-28`
because the gateway/sensor clock is ~5 months behind real time — derive freshness
from **broker arrival time**, not the payload timestamp.

> **Flat vs nested payloads.** Most quick-look topics are **flat** (`Rssi`, `Batt`,
> `Serial`, `Temp` at the top level), but **`proc/reading/notify` and
> `dyn/vib/notify/lite` are nested under a `Reading` object** — confirmed by the live
> captures — as is the full `dyn/vib/notify` (Method 4). For those, read
> `Reading.Xrms`, `Reading.Batt`, etc. The live `dyn/vib/notify/lite` also carries
> velocity scalars (`Reading.VelXrms`… in in/s), which the flat schema in
> `docs/mqtt-topics.md` does not show.

### Import the connection

**MQTTX desktop app:**

1. Open MQTTX → top-left **+** menu (or the hamburger) → **Import**.
2. Choose **JSON** as the format and select [`connection.json`](connection.json).
   (Newer MQTTX builds: **Settings → Data → Import Data**, or right-click the
   connection list → **Import**.)
3. The connection **ACCESS360 broker** appears with its subscriptions attached.
4. Select it and click **Connect**.

> **Schema note.** MQTTX's import expects a JSON **array** of connection objects;
> this file is that array. Field names (`name`, `clientId`, `hostname`, `port`,
> `mqttVersion`, `subscriptions`) match the MQTTX export shape, but MQTTX versions
> have drifted slightly over releases. If your build refuses the import, create the
> connection by hand using the values in **Setup** above, add the five
> subscriptions, then use **Export** to capture your build's exact schema and
> overwrite this file.

### Subscribe & verify

After connecting, the five subscriptions load automatically. Otherwise add them
manually (New Subscription, QoS 1):

- `access360/43250372/#` — the whole live gateway (start here)
- `access360/43250372/rssi/notify` — BLE signal
- `access360/43250372/proc/checkin/notify` — heartbeats
- `access360/43250372/dyn/batt/notify` — battery
- `access360/43250372/dyn/vib/notify/lite` — overall vibration scalars

Click any message to pretty-print the JSON and compare it against the captured
samples in `sample-payloads/` and the schemas in
[`../../docs/mqtt-topics.md`](../../docs/mqtt-topics.md).

### Still to do

- All six health topics now have captured samples. A long passive window will only
  ever show `rssi/notify` + `proc/checkin/notify` when the fleet is idle; the reading
  topics (`proc/reading`, `dyn/temp`, `dyn/vib/notify/lite`) fire on the sensors'
  slower cadence — re-capture fresh ones any time with
  `mosquitto_sub -h 192.168.68.150 -t 'access360/43250372/#' -v`.
- Re-export `connection.json` from your actual MQTTX build to lock the exact schema
  for your version.
- Add a short screen capture / GIF of the live stream.
