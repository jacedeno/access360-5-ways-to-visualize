# Method 3 — Grafana Live (Web Streaming)

**Tier:** Dashboard · **Platform:** Web browser (Grafana)

Real-time streaming panels fed **straight from HiveMQ** — no database in the path.
Grafana subscribes to the MQTT topics and pushes updates to the browser over
Grafana Live (WebSocket), so values move as messages arrive.

## Purpose

- A proper web dashboard for **fleet health** and **overall vibration trends**,
  shareable by URL on the private plane.
- Live streaming (sub-second) via Grafana Live, reading MQTT directly.
- A stepping stone to the platform's full "Spectra Fleet Health" board, but with
  zero ingestion dependency — useful for demos and quick setups.

## How it works

Grafana has a first-party **MQTT data source**
([grafana-mqtt-datasource](https://github.com/grafana/mqtt-datasource)). It
connects to the broker, subscribes to topics you configure, and streams each
message into a Grafana Live channel that panels render in real time.

```
HiveMQ (192.168.68.150:1883) ──MQTT──► Grafana MQTT data source ──Grafana Live (WS)──► browser panels
```

## Prerequisites

- A Grafana instance (v10+). Deploy it as a **Docker container on
  `192.168.68.150`** so it shares the network with `iot-hivemq` (see the compose
  snippet below).
- The **MQTT data source plugin** installed:
  `grafana-cli plugins install grafana-mqtt-datasource` (or set
  `GF_INSTALL_PLUGINS` as below).
- Broker details — [`../../docs/backend-context.md`](../../docs/backend-context.md):
  `192.168.68.150:1883` (or `iot-hivemq:1883` from inside the Docker network), TLS
  off, anonymous.

## Setup

1. **Run Grafana on `.150`** (helper service, Docker):

   ```yaml
   # docker-compose.yml (Phase 2 will commit a finished version)
   services:
     grafana:
       image: grafana/grafana:latest
       container_name: grafana-live
       ports:
         - "3000:3000"
       environment:
         GF_INSTALL_PLUGINS: grafana-mqtt-datasource
       networks:
         - iot            # same network as iot-hivemq
   networks:
     iot:
       external: true
   ```

2. **Add the MQTT data source** (Connections → Data sources → MQTT):
   - URI: `tcp://iot-hivemq:1883` (in-network) or `tcp://192.168.68.150:1883`.
   - No TLS, no credentials.
3. **Build panels.** Each panel subscribes to a topic; the MQTT data source emits
   the JSON fields as a streaming frame. Suggested panels (mirroring
   [`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md)):

   | Panel | Topic | Field(s) | Type |
   |---|---|---|---|
   | Battery % by sensor | `access360/43250372/dyn/batt/notify` | `Batt` | Stat / Bar gauge |
   | RSSI | `access360/43250372/rssi/notify` | `Rssi` | Time series |
   | Overall RMS | `access360/43250372/dyn/vib/notify/lite` | `Xrms`,`Yrms`,`Zrms` | Time series |
   | Heartbeats | `access360/43250372/proc/checkin/notify` | `Serial` | State timeline |
   | Errors | `access360/43250372/error/notify` | `Error` | Logs |

4. **Thresholds:** color battery `< 20 %` and RSSI `< -75 dBm` red, per the
   fleet-health doc.

## Notes

- **Streaming vs. history.** The MQTT data source streams *live* — it does not
  store history. For trend/history panels, point Grafana at **InfluxDB 3** instead
  (SQL via Flight SQL; see [`../../docs/influx-mapping.md`](../../docs/influx-mapping.md)).
  This method is deliberately the no-database, live-only path.
- **Browser transport:** Grafana Live pushes to the browser over its own
  WebSocket; the *Grafana server* is the MQTT TCP client, so you do **not** need
  MQTT-over-WebSockets on HiveMQ for this method.
- **Waveform:** skip `dyn/vib/notify` (multipart). Use the FFT/waterfall method
  (4) for spectra.

## Phase 2 (later)

Commit the finished `docker-compose.yml`, a provisioned data source YAML, and an
exported dashboard JSON (`fleet-health-live.json`).
