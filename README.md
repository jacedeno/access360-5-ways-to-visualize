# 5 Ways to Visualize Your CTC Connect ACCESS360 Series

Five independent, self-contained ways to **see** the live telemetry from a
[CTC Connect ACCESS360](https://www.ctconline.com/) wireless vibration
condition-monitoring fleet — from a quick desktop topic sniff to a hardware
"Fleet Health" panel that sits on your desk.

Every method here subscribes **directly to the existing HiveMQ MQTT broker**.
There is no aggregator, no database, and no custom backend service in the path —
each tool talks straight to the broker topics the gateway already publishes. The
goal is to show how much observability you can get out of the raw MQTT stream
with off-the-shelf tools plus a little firmware.

> **Status:** Phase 2 — building out the methods. Method 4 (Node-RED FFT/waterfall)
> is **delivered and running live** on the IoT stack; the rest are scaffolded with
> full setup docs and land next. This repository is intentionally self-contained:
> all the backend context (broker, topics, payload schemas, sensor IDs, and the
> Fleet Health metric set) lives under [`docs/`](docs/) so each method can be built
> without the original platform repository.
>
> | # | Method | Status |
> |---|---|---|
> | 1 | MQTTX | 📄 documented |
> | 2 | IoT MQTT Panel | 📄 documented |
> | 3 | Grafana Live | 📄 documented |
> | 4 | Node-RED FFT/waterfall ⭐ | ✅ **delivered + live** |
> | 5 | SenseCAP Indicator D1L ⭐ | 📄 documented |

---

## The system in one diagram

```
 BLE vibration sensors          4G LTE (Hologram SIM)        Private plane (LAN / VPN)
 WS100 / WS200 / WS300   ──BLE──►  ConnectBridge gateway  ──MQTT──►  HiveMQ CE broker
 (ACCESS360 series)                (serial 43250372)                 192.168.68.150:1883
                                                                          │
                          ┌───────────────────────────────────────────────┤  topics:
                          │                                                 │  access360/<gw>/<channel>
        ┌─────────────────┴───────┬──────────────┬──────────────────┬──────┴──────────────┐
        ▼                         ▼              ▼                  ▼                     ▼
   1. MQTTX               2. IoT MQTT Panel  3. Grafana Live   4. Node-RED FFT   5. SenseCAP Indicator D1L
   (desktop sniff)        (Android phone)    (web streaming)   ⭐ waterfall      ⭐ on-device Health monitor
```

All five tools are MQTT subscribers. They differ only in **where they run** and
**what they draw**.

---

## The 5 methods

| # | Method | Platform | What it shows | Tier |
|---|---|---|---|---|
| 1 | [**MQTTX**](methods/01-mqttx/) | Desktop (Win/macOS/Linux) | Live topic tree + raw JSON payload capture. The "is data actually flowing?" tool. | Quick look |
| 2 | [**IoT MQTT Panel**](methods/02-iot-mqtt-panel/) | Android phone | A pocket dashboard: gauges/values bound to topics. Glance at the fleet from your phone. | Quick look |
| 3 | [**Grafana Live**](methods/03-grafana-live/) | Web (browser) | Real-time streaming panels fed straight from HiveMQ via the MQTT data source. | Dashboard |
| 4 | [**Node-RED — FFT / Waterfall**](methods/04-node-red-fft-waterfall/) ⭐ | Web (browser) | The hero. Reassembles the raw waveform, computes the FFT, and renders a scrolling **spectrogram / waterfall** from the vibration topics. | Hero |
| 5 | [**SenseCAP Indicator D1L**](methods/05-sensecap-indicator-d1l/) ⭐ | ESP32-S3 hardware | The hero. A multi-screen on-device **"Health" monitor** mirroring Spectra Fleet Health (broker status, sensors online, battery %, RSSI, last-seen age, 4G usage). Status only — no raw plots. | Hero |

Each folder has its own README with **purpose, prerequisites, and setup steps**.

---

## Two flavors of visualization

The five methods split cleanly into two intents:

- **Signal visualization** (what is the machine doing?) — Method 4 (Node-RED FFT /
  waterfall) is the one that draws the actual vibration spectrum. It consumes the
  raw time-domain waveform arrays on `dyn/vib/notify` and turns them into a
  frequency-domain waterfall.
- **Fleet-health visualization** (is the system healthy?) — Methods 1, 2, 3, and 5
  surface operational health: broker up/down, sensors online, battery, RSSI,
  last-seen age, message rate, and 4G data usage. Method 5 (SenseCAP Indicator)
  is a dedicated, always-on hardware version of this view.

See [`docs/fleet-health-metrics.md`](docs/fleet-health-metrics.md) for the exact
metric definitions and thresholds the health views reproduce.

---

## Backend context (read this before building any method)

All five methods need the same four facts: where the broker is, what topics exist,
what the payloads look like, and which sensors/gateways are live. That context is
documented once, here:

| Doc | Contents |
|---|---|
| [`docs/backend-context.md`](docs/backend-context.md) | Broker connection (host, port, TLS, auth), network topology, deployment notes, the `192.168.68.150` Docker host. |
| [`docs/mqtt-topics.md`](docs/mqtt-topics.md) | Every MQTT topic with its exact JSON payload schema, types, units, and example messages. |
| [`docs/sensors-and-gateways.md`](docs/sensors-and-gateways.md) | Gateway serials and sensor IDs currently in use, model conventions (WS100/200/300). |
| [`docs/fleet-health-metrics.md`](docs/fleet-health-metrics.md) | The Spectra Fleet Health metric set: definitions, queries, thresholds, and 4G cost model. |
| [`docs/influx-mapping.md`](docs/influx-mapping.md) | Reference: how the platform maps MQTT payloads to InfluxDB measurements (optional for these methods). |

---

## Quick start (any method)

1. Be on the **private plane** — the broker lives on the LAN at
   `192.168.68.150:1883` and is **not** exposed to the internet. Connect over the
   local network or the homelab VPN.
2. Confirm data is flowing with a one-liner before building anything fancy:
   ```bash
   mosquitto_sub -h 192.168.68.150 -t 'access360/#' -v
   ```
3. Pick a method folder and follow its README.

---

## Repository layout

```
.
├── README.md                       ← you are here
├── LICENSE                         ← MIT
├── docs/                           ← self-contained backend context
│   ├── backend-context.md
│   ├── mqtt-topics.md
│   ├── sensors-and-gateways.md
│   ├── fleet-health-metrics.md
│   └── influx-mapping.md
└── methods/
    ├── 01-mqttx/
    ├── 02-iot-mqtt-panel/
    ├── 03-grafana-live/
    ├── 04-node-red-fft-waterfall/   ⭐
    └── 05-sensecap-indicator-d1l/   ⭐
```

## Deployment note

If a method needs a helper service (e.g. Grafana or Node-RED), it **reuses the
existing IoT stack on host `192.168.68.150`** rather than standing up new
infrastructure. Method 4, for example, deploys *into* the Node-RED that already runs
there — added as a separate flow/dashboard page, leaving the production flow
untouched (see [`methods/04-node-red-fft-waterfall/deploy/`](methods/04-node-red-fft-waterfall/deploy/)).
No method requires a custom aggregator microservice — they all subscribe to HiveMQ
directly.

> The IoT stack on `.150` itself (HiveMQ, Node-RED, InfluxDB 3, Grafana) is a
> separate project and will be written up on its own; this repo only consumes it.

## License

[MIT](LICENSE) © 2026 Jose Cedeno
