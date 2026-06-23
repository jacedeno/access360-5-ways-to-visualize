# MQTT → InfluxDB 3 Mapping (the stack's history schema)

How the stack's **Node-RED ingestion** maps the MQTT payloads to InfluxDB 3
measurements. This is the schema behind the durable history — the lightweight
methods can still read MQTT live, but anything that needs **trends, backfill, or
"time since last seen"** reads these measurements, and Node-RED writes them.

- **Query rule:** InfluxDB 3 is queried with **SQL via Flight SQL** — never Flux.
- **Database (this repo's 5-ways stack):** **`ctc43250372`** — the per-gateway database
  the Node-RED ingestion flow writes to, and what Method 3's dashboard reads.
  > The `spectra` database (consolidated, multi-gateway) belongs to the separate
  > **`spectra-io`** project's ingester — out of scope here, never written or modified.
  > Both use the identical measurement/tag schema below.
- **Mandatory tags on every point:** `gateway_sn`, `sensor_id`, `site`
  (`Cement_Plant`), plus a per-measurement discriminator (`model` / `metric` /
  `data_type`).

---

## Measurements

| Measurement | From channels | Purpose |
|---|---|---|
| `vibration` | `dyn/vib/notify(/lite)`, `proc/reading/notify` | Scalar vibration metrics |
| `vibration_fft` | `dyn/vib/notify` (full waveform) | Spectral arrays + sampling frequency |
| `sensor_health` | `dyn/batt/notify`, `proc/checkin/notify`, `rssi/notify` | Battery, RSSI, heartbeat |
| `gateway_events` | `error/notify`, `status/notify` | Gateway error/status events |

> Note: Node-RED writes scalar fields into `vibration` with a `has_waveform` flag
> and keeps spectral data in `vibration_fft`. The tag set is identical across both.

---

## `vibration`

**Tags:** `gateway_sn`, `sensor_id`, `site`, `model` (`WS300`/`WS200`/`WS100`/
`WS_DYNAMIC`), `data_type` (`full`/`lite`/`process`).

**Fields** (payload key → influx field):

| Payload | Influx field | Unit |
|---|---|---|
| `Xpk`/`Xpp`/`Xrms` | `x_pk`/`x_pp`/`x_rms` | g |
| `Ypk`/`Ypp`/`Yrms` | `y_pk`/`y_pp`/`y_rms` | g |
| `Zpk`/`Zpp`/`Zrms` | `z_pk`/`z_pp`/`z_rms` | g |
| `VelXpk`…`VelZrms` | `vel_x_pk`…`vel_z_rms` | in/s |
| `ID` | `reading_id` | — |
| `Samples` | `samples` | — |
| `Fs` | `sampling_freq` | Hz |
| (arrays present?) | `has_waveform` | 0/1 |
| `Temp` (WS100) | `temperature` | °C |
| `Batt` (WS100) | `battery` | % |

Line-protocol example:

```text
vibration,gateway_sn=43250372,model=WS300,sensor_id=22255728,site=Cement_Plant,data_type=full \
  x_rms=0.037,y_rms=0.056,z_rms=0.042,vel_x_pk=-0.031,has_waveform=1.0,samples=6400.0,sampling_freq=12989.0 \
  1782050280000000000
```

---

## `sensor_health`

**Tags:** `gateway_sn`, `sensor_id`, `site`, `metric` (`battery`/`checkin`/`rssi`).

| metric | Field | Unit |
|---|---|---|
| `battery` | `battery_pct` | % |
| `checkin` | `online` | const 1.0 |
| `rssi` | `rssi_dbm` | dBm |

```text
sensor_health,gateway_sn=43250372,metric=rssi,sensor_id=11252280,site=Cement_Plant rssi_dbm=-57.0
```

---

## `gateway_events`

**Tags:** `gateway_sn`, `site`, `event_type` (`error`/`status`). No `sensor_id`.

| event_type | Field |
|---|---|
| `error` | `error_payload` (JSON string), `severity` (const 1.0) |
| `status` | `status_payload` (JSON string) |

---

## Timestamps

CTC `Time` is `"yyyy-mm-dd hh:MM"` (or with seconds on `rssi/notify`), treated as
**UTC** and converted to epoch nanoseconds for the line-protocol timestamp. If a
payload has no time, Node-RED uses server ingest time.

---

## Write endpoint (reference)

InfluxDB 3 Core line-protocol write:

```
POST http://192.168.68.150:8181/api/v3/write_lp?db=ctc43250372&precision=nanosecond
Content-Type: text/plain
Authorization: Bearer <INFLUX_TOKEN>   # secret — env var, never commit
```
