# Method 3 — Grafana Dashboard (ACCESS360 — Fleet Health & Readings)

**Tier:** Dashboard · **Platform:** Web browser (Grafana)

The featured board is **"ACCESS360 — Fleet Health & Readings"** (uid
`access360-fleet-readings`): a compact **System Health** strip on top (broker up,
sensors online, *time since last activity*, 4G usage, throughput, per-sensor battery
& RSSI) over a **Sensor Readings** section below (vibration in/s & g, and
temperature) with a **`Sensor` dropdown** to overlay all sensors (compare amplitude)
or drill into one.

> **Why not "Grafana straight from the broker"?** That was the original idea, but a
> broker-only board **cannot** show history, "time since last activity", or stored
> readings — Grafana Live is ephemeral (no backfill) and the fleet is bursty. So the
> real dashboard reads from where the data is durably stored: **Prometheus** (the
> spectra-ingester's fleet metrics) for the health KPIs and **InfluxDB 3** for the
> readings. A broker-direct live board is still included as a secondary demo (below).

![Fleet Health & Readings — top (System Health)](docs-img/fleet-health-readings-top.png)
![Fleet Health & Readings — bottom (Sensor Readings)](docs-img/fleet-health-readings-bottom.png)

## How it works

```
                         ┌─ ingester /metrics ─► Prometheus ─► System Health KPIs (top)
HiveMQ (…150:1883) ──────┤
                         └─ spectra-ingester ─► InfluxDB 3 (spectra) ─ SQL ─► Sensor Readings (bottom)
```

- **System Health (top):** Prometheus metrics the ingester exposes
  (`ingester_mqtt_connected`, `ingester_sensor_last_seen_timestamp_seconds`,
  `ingester_sensor_battery_percent`, `hologram_sim_*`, message/point/error rates).
- **Sensor Readings (bottom):** an **InfluxDB 3** data source (SQL/FlightSQL) over the
  `spectra` database. The **`$sensor`** template variable feeds
  `WHERE sensor_id IN (${sensor:singlequote})`; the **Z axis** is the cross-sensor
  comparison metric (common to single-axis WS200 and triaxial WS300).

> **Prerequisite the dashboard exposed:** the ingester was **dropping
> `dyn/temp/notify`** (0 temperature rows in InfluxDB) and stamping dynamic-sensor
> readings with the sensors' **stuck RTC** (~5 months behind). Both were fixed in the
> `spectra-io` ingester (store temperature + clamp skewed timestamps to ingest time)
> so the readings land "now" and the temperature panel populates. See that repo.

## Prerequisites

This **reuses the existing `iot-grafana`** on the `.150` IoT stack (Grafana
**12.4.1**) — no new container. It needs:

- A Grafana **service-account token** (role Admin/Editor) and an **InfluxDB 3 token**
  (`INFLUX_TOKEN`) for `deploy/deploy.sh`. The **Prometheus** data source ("Spectra
  Prometheus") already exists.
- (For the secondary broker-direct board only) the **`grafana-mqtt-datasource`**
  plugin: `ssh root@192.168.68.150 'docker exec iot-grafana grafana cli plugins install grafana-mqtt-datasource && docker restart iot-grafana'`.
- iot-grafana shares the `iot_iot` network with `iot-hivemq`, `iot-influxdb3` and
  `spectra-prometheus`.

## What's in this folder (Phase 2 — delivered)

| Path | What it is |
|---|---|
| `dashboards/fleet-health-readings.json` | **The featured dashboard** (15 panels). DS uids templated `${DS_PROM_UID}` / `${DS_INFLUX_UID}`. |
| `deploy/gen_readings_dashboard.py` | Generator for the featured dashboard (documents every KPI/reading query). |
| `deploy/deploy.sh` | Idempotent deploy into iot-grafana: upserts the data sources and imports both dashboards. |
| `docs-img/fleet-health-readings-top.png` / `-bottom.png` | Screenshots of the featured dashboard. |
| `dashboards/fleet-health-live.json` + `deploy/gen_dashboard.py` + `normalizer/flow.json` | The **secondary, broker-direct** live board (Grafana Live via MQTT + velocity normalizer). Kept as a demo; see below. |
| `provisioning/datasource.yml` | MQTT data source reference for the broker-direct board. |

### Deploy

```bash
GRAFANA_TOKEN=<grafana-sa-token> INFLUX_TOKEN=<influx3-token> ./deploy/deploy.sh
# open http://192.168.68.150:3000/d/access360-fleet-readings
```

---

## Secondary: the broker-direct live board (`access360-live`)

The original "Grafana straight from the broker" experiment is kept as a demo of
Grafana Live. It streams from HiveMQ with **no database in the path**, which is neat
but **ephemeral** — it only shows messages that arrive while a panel is open, so it
reads "No data" whenever the bursty fleet is quiet. Details below.

### Deploy

```bash
# 1) the velocity normalizer (adds a flow to the existing iot-nodered; see normalizer/)
#    additive deploy, same pattern as methods/04-.../deploy/deploy.sh
# 2) both data sources (MQTT live + InfluxDB 3 history) + the dashboard:
GRAFANA_TOKEN=<grafana-sa-token> INFLUX_TOKEN=<influx3-token> ./deploy/deploy.sh
# open http://192.168.68.150:3000/d/access360-live
```

`INFLUX_TOKEN` is optional — omit it to deploy only the live (MQTT) half; the
history panels then read empty.

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

### Panels

**Fleet history (InfluxDB 3 — always populated):** RSSI by sensor (7d) · last
recorded velocity (IPS, ISO-zone stat) · battery % by sensor (bar gauge) · sensors
reporting per hour (7d).

**Live stream (Grafana Live / MQTT):**

| Panel | Topic | Field(s) | Type |
|---|---|---|---|
| Velocity RMS per axis — IPS | `…/derived/vib` | `vel_x_rms`,`vel_y_rms`,`vel_z_rms` (in/s) | Time series |
| Overall velocity — IPS | `…/derived/vib` | `vel_overall` (in/s) | Gauge, ISO-10816 zones |
| Acceleration RMS per axis (g) | `…/dyn/vib/notify/lite` | `Xrms`,`Yrms`,`Zrms` (g) | Time series |
| BLE RSSI by sensor | `…/rssi/notify` | `Rssi` | Time series (partitioned by `Serial`) |
| Temperature | `…/dyn/temp/notify` | `Temp` | Time series |
| Heartbeats | `…/proc/checkin/notify` | `Serial`,`Time` | Table |
| Battery % / RSSI (latest) | `…/dyn/batt/notify`, `…/rssi/notify` | `Batt`,`Rssi` | Stat |

### Findings from deploying live (2026-06-22)

- **Verified end-to-end:** the InfluxDB history section renders real data (RSSI 7d,
  battery 76/88/96 %, last IPS) and the live MQTT IPS time series + ISO gauge render
  from `…/derived/vib`. The MQTT data source reports *MQTT Connected*.
- **The "No data" cause:** the latest real RSSI was **~28 h old** at deploy time and
  Grafana Live doesn't backfill — so any live-only window read empty. History windows
  were widened to where the data lives (RSSI 7d had 3171 rows) and the InfluxDB
  section was added.
- **InfluxDB 3 data source quirks:** Grafana's `influxdb` (version=SQL/FlightSQL)
  reads the SQL from **`rawSql`** (not `query`), every query **must select a `time`
  column**, and results must be **time-ascending**. All history queries filter
  `gateway_sn='43250372'` to exclude synthetic test rows.
- **MQTT data source doesn't flatten nested JSON** → the velocity normalizer above.
  It also turns *every* JSON key into a field, so panels hide `Serial`/`ID` or pin a
  stat/gauge to one field (`/^vel_overall$/`).

![Grafana Live — velocity IPS](docs-img/grafana-live-ips.png)

## Notes

- **Streaming vs. history.** The MQTT data source streams *live* and stores nothing;
  the InfluxDB 3 section (SQL via Flight SQL — see
  [`../../docs/influx-mapping.md`](../../docs/influx-mapping.md)) provides the
  history. The live half is still the "no-DB, straight-from-broker" showcase.
- **Browser transport:** Grafana Live pushes to the browser over its own
  WebSocket; the *Grafana server* is the MQTT TCP client, so you do **not** need
  MQTT-over-WebSockets on HiveMQ for this method.
- **Waveform:** skip `dyn/vib/notify` (multipart). Use the FFT/waterfall method
  (4) for spectra.

## Still to do

- Capture a screenshot of the **live** section during real fleet activity (the live
  IPS shot was driven by synthetic readings; the fleet was quiet at deploy time).
- Optional: an `error/notify` logs panel; an InfluxDB "last seen age" per-sensor stat.
- The synthetic `99999999` test rows remain in InfluxDB (excluded by the
  `gateway_sn='43250372'` filter); purge them if/when InfluxDB 3 Core supports it.
