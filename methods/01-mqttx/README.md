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

## Phase 2 (later)

Add a saved MQTTX `connections.json` export and a short screen capture / GIF of the
live stream to this folder.
