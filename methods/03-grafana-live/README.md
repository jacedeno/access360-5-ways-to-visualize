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

This **reuses the existing `iot-grafana`** on the `.150` IoT stack (Grafana
**12.4.1**) — no new container. It needs:

- The **`grafana-mqtt-datasource`** plugin (signed, in the catalog). Install once
  (this restarts Grafana):
  ```bash
  ssh root@192.168.68.150 'docker exec iot-grafana grafana cli plugins install grafana-mqtt-datasource && docker restart iot-grafana'
  ```
- A Grafana **service-account token** (role Admin/Editor) for `deploy/deploy.sh`.
- iot-grafana already shares the `iot_iot` network with `iot-hivemq`, so it reaches
  the broker at `iot-hivemq:1883` (TLS off, anonymous).

## What's in this folder (Phase 2 — delivered)

| Path | What it is |
|---|---|
| `provisioning/datasource.yml` | The MQTT data source (`uri: tcp://iot-hivemq:1883`), for file-provisioning or reference. |
| `dashboards/fleet-health-live.json` | The dashboard (13 panels, 3 rows). The data source uid is templated as `${DS_MQTT_UID}`. |
| `deploy/deploy.sh` | Idempotent deploy into the existing iot-grafana via API: upserts the data source, imports the dashboard. |
| `deploy/gen_dashboard.py` | Generator that builds the dashboard JSON (documents how each panel is wired). |
| `normalizer/flow.json` | Node-RED flow that flattens the **velocity (IPS)** out of `dyn/vib/notify` to a flat topic so the dashboard can plot it (see below). |
| `docs-img/grafana-live-ips.png` | Screenshot: live velocity (IPS) — per-axis time series + ISO-zone gauge. |

### Deploy

```bash
# 1) the velocity normalizer (adds a flow to the existing iot-nodered; see normalizer/)
#    additive deploy, same pattern as methods/04-.../deploy/deploy.sh
# 2) the Grafana data source + dashboard:
GRAFANA_TOKEN=<service-account-token> ./deploy/deploy.sh
# open http://192.168.68.150:3000/d/access360-live
```

### Getting velocity (IPS) into a live dashboard — the normalizer

The headline condition-monitoring metric is **velocity in in/s (IPS)**. It only
exists in the **full `dyn/vib/notify`** record, where it is **nested under
`Reading`** (`Reading.VelXrms`…) — and the Grafana MQTT data source **does not
flatten nested JSON** (a nested panel shows *"Data is missing a number field"*).

So `normalizer/flow.json` adds a tiny Node-RED flow on the existing `iot-nodered`:
it subscribes to `dyn/vib/notify`, pulls `VelXrms/VelYrms/VelZrms` (and computes an
overall vector magnitude), and republishes a **flat** JSON to
`access360/43250372/derived/vib`. The dashboard's IPS panels read that flat topic.
This stays "straight from the broker" — the normalizer is a pass-through, not an
aggregator/DB. (Full readings are **on-demand**, so IPS panels fill when a reading
is taken; trigger one with `…/dyn/vib/trigger`.)

### Panels (mirroring [`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md))

| Row / Panel | Topic | Field(s) | Type |
|---|---|---|---|
| **IPS** · Velocity RMS per axis | `…/derived/vib` | `vel_x_rms`,`vel_y_rms`,`vel_z_rms` (in/s) | Time series |
| **IPS** · Overall velocity | `…/derived/vib` | `vel_overall` (in/s) | Gauge, ISO-10816 zones |
| **g** · Acceleration RMS per axis | `…/dyn/vib/notify/lite` | `Xrms`,`Yrms`,`Zrms` (g) | Time series |
| **g** · Z-axis RMS (latest) | `…/dyn/vib/notify/lite` | `Zrms` | Stat |
| **Health** · BLE RSSI by sensor | `…/rssi/notify` | `Rssi` | Time series (partitioned by `Serial`) |
| **Health** · Temperature | `…/dyn/temp/notify` | `Temp` | Time series |
| **Health** · Heartbeats | `…/proc/checkin/notify` | `Serial`,`Time` | Table |
| **Health** · Battery % / RSSI (latest) | `…/dyn/batt/notify`, `…/rssi/notify` | `Batt`,`Rssi` | Stat |

### Findings from deploying live (2026-06-22)

- **Verified end-to-end:** data source reports *MQTT Connected*; the IPS time series
  plots `vel_x/y/z_rms` and the gauge reads overall velocity in the ISO red zone
  (see screenshot). Verified with **synthetic messages on a throwaway gateway topic**
  (`access360/99999999/…`) plus the real `…/derived/vib` — the production ingester
  only knows the standard channels, so the test never writes bad data.
- **MQTT data source doesn't flatten nested JSON** → the velocity normalizer above.
- **Hide non-metric fields:** the data source turns *every* JSON key into a field, so
  a naive panel also plots `Serial`, wrecking the axis. Panels hide `Serial`/`ID`
  (time series) or pin a stat/gauge to one field (`/^vel_overall$/`).
- **Live-only, no backfill:** Grafana Live shows only messages that arrive *after*
  you open the panel; ACCESS360 sensors publish in **bursts**, so panels fill as
  traffic flows. For history/trends, point Grafana at **InfluxDB 3** instead (Notes).

![Grafana Live — velocity IPS](docs-img/grafana-live-ips.png)

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

## Still to do

- Capture a full-dashboard screenshot during **live fleet activity** (the committed
  shot is the single-panel self-test, since the fleet was quiet at deploy time).
- Optional: an `error/notify` logs panel and a per-sensor "last seen" stat.
