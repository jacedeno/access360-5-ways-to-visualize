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
> real dashboard reads from where the data is durably stored: **InfluxDB 3**, which
> the stack's **Node-RED ingestion** fills from the MQTT stream (readings *and*
> derived fleet-health series). This is exactly the gap the stack closes — the broker
> alone can't give you trends.

> **Direction (Node-RED-only stack).** In the consolidated stack, **Node-RED is the
> only ingestion glue**: it writes both the sensor readings and the fleet-health
> series to InfluxDB 3, and Grafana reads everything from InfluxDB via SQL. A
> Prometheus backend is **optional** and can be added later if wanted.
> **Current state:** the committed dashboard + `deploy.sh` still wire the **System
> Health** KPIs to **Prometheus** (fed by the legacy `spectra-ingester`, now being
> retired) alongside the InfluxDB readings — migrating those KPIs to InfluxDB
> (written by Node-RED) is the open work item for this method.

![Fleet Health & Readings — top (System Health)](docs-img/fleet-health-readings-top.png)
![Fleet Health & Readings — bottom (Sensor Readings)](docs-img/fleet-health-readings-bottom.png)

## How it works

**Target (Node-RED-only stack):**

```
HiveMQ (…:1883) ─► Node-RED ─► InfluxDB 3 (spectra) ─ SQL ─┬─► System Health KPIs (top)
                  (ingest +                                 └─► Sensor Readings (bottom)
                   health)
```

- **System Health (top):** fleet-health series Node-RED derives from the MQTT
  stream and writes to InfluxDB (`sensor_health`, `gateway_events`, plus SIM usage
  from the Hologram poller) — broker up, sensors online, last-seen age, battery,
  RSSI, message/error rates, 4G usage. Definitions/thresholds in
  [`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md).
- **Sensor Readings (bottom):** an **InfluxDB 3** data source (SQL/FlightSQL) over the
  `spectra` database. The **`$sensor`** template variable feeds
  `WHERE sensor_id IN (${sensor:singlequote})`; the **Z axis** is the cross-sensor
  comparison metric (common to single-axis WS200 and triaxial WS300).

> **Current committed wiring** (pending the migration above): the System Health
> KPIs read **Prometheus** (`ingester_mqtt_connected`,
> `ingester_sensor_last_seen_timestamp_seconds`, `ingester_sensor_battery_percent`,
> `hologram_sim_*`, message/point/error rates) exposed by the legacy
> `spectra-ingester`; the Sensor Readings already read InfluxDB. Both data-source
> UIDs are templated in the dashboard JSON.

> **Two ingestion gotchas the dashboard exposed** (the stack's Node-RED ingestion
> must handle both): `dyn/temp/notify` was being **dropped** (0 temperature rows in
> InfluxDB), and dynamic-sensor readings were stamped with the sensors' **stuck RTC**
> (~5 months behind). The ingestion must **store temperature** and **clamp skewed
> payload timestamps to ingest time** so readings land "now" and the temperature
> panel populates. (See the clock-skew note in
> [`../../docs/sensors-and-gateways.md`](../../docs/sensors-and-gateways.md).)

## Prerequisites

This uses the stack's **`iot-grafana`** (Grafana **12.4.1**) — no extra container.
It needs:

- A Grafana **service-account token** (role Admin/Editor) and an **InfluxDB 3 token**
  (`INFLUX_TOKEN`) for `deploy/deploy.sh`.
- `iot-grafana` shares the stack's Docker network with `iot-hivemq` and
  `iot-influxdb3` (and, until the KPI migration, the legacy Prometheus that backs the
  System Health strip).

## What's in this folder (Phase 2 — delivered)

| Path | What it is |
|---|---|
| `dashboards/fleet-health-readings.json` | **The featured dashboard** (15 panels). DS uids templated `${DS_PROM_UID}` / `${DS_INFLUX_UID}`. |
| `deploy/gen_readings_dashboard.py` | Generator for the featured dashboard (documents every KPI/reading query). |
| `deploy/deploy.sh` | Idempotent deploy into iot-grafana: upserts the InfluxDB data source and imports the dashboard. |
| `docs-img/fleet-health-readings-top.png` / `-bottom.png` | Screenshots of the dashboard. |

### Deploy

```bash
GRAFANA_TOKEN=<grafana-sa-token> INFLUX_TOKEN=<influx3-token> ./deploy/deploy.sh
# open http://192.168.68.150:3000/d/access360-fleet-readings
```

## Panels

**System Health (top — compact KPIs, Prometheus):** Broker up · Sensors online
(last 10 min) · Worst last-seen age (*time since last activity*) · Messages/s ·
Errors/s · 4G data used % · Points written/s · Battery % by sensor · RSSI by sensor.

**Sensor Readings (bottom — InfluxDB, `$sensor`-filtered):**

| Panel | Source | Field | Notes |
|---|---|---|---|
| Vibration velocity — Z RMS (in/s) | `vibration` | `vel_z_rms` | overlay sensors to compare amplitude |
| Vibration acceleration — Z RMS (g) | `vibration` | `z_rms` | — |
| Temperature (°F) | `sensor_health` (`metric=temperature`) | `temperature_c` → °F | stored in °C, shown in **°F** |

Z axis is the cross-sensor comparison metric (common to single-axis WS200 and
triaxial WS300). The `Sensor` dropdown is a multi-select: **All** overlays every
sensor; pick one to drill in.

## Findings

- **Verified live:** top KPIs populate (broker UP, 5 online, worst last-seen,
  battery 76/88/96 %, 4G 51.6 %); vibration overlays two sensors with distinct
  amplitude; temperature overlays three sensors (68 / 69.8 / 71.6 °F). The `$sensor`
  dropdown drills to a single sensor.
- **Two ingestion requirements this surfaced:** temperature (`dyn/temp/notify`) must
  be **stored** (it was being dropped, so no temperature rows), and dynamic-sensor
  readings carry the BLE sensors' **stuck RTC** (~5 months behind), so the ingestion
  must **clamp skewed payload timestamps to ingest time** or they land in the past.
  The stack's Node-RED ingestion must do both (store temperature in °C; clamp
  timestamps). Temperature accrues going forward — no backfill.
- **InfluxDB 3 data source quirks:** Grafana's `influxdb` (version=SQL/FlightSQL)
  reads the SQL from **`rawSql`** (not `query`), every query **must select a `time`
  column**, and results must be **time-ascending**. All queries filter
  `gateway_sn='43250372'` (excludes the synthetic `99999999` test rows).

## Still to do

- **Migrate the System Health KPIs from Prometheus to InfluxDB** (written by
  Node-RED) per the Node-RED-only direction above, after which the optional
  Prometheus backend can be dropped.
- Capture vibration/temperature over a longer window for a fuller trend (currently a
  handful of seeded points).
- Optional: a `gateway_events` errors/`ap` panel; an InfluxDB "last-seen age" table.
