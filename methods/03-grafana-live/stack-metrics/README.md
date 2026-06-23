# Stack metrics — System Health from our stack (no ingester) + temperature ingest

This makes Method 3's **System Health** KPIs come from the **IoT stack** instead of
`spectra-io`'s ingester, and fills the **Temperature** panel — all without touching
spectra-io.

> **Status: live (2026-06-23).** Flow deployed into `iot-nodered`; `iot-prometheus`
> up on host `:9091` scraping it; Grafana datasource "IoT Prometheus" added and the 9
> System Health panels re-pointed to it. The dashboard is now fed entirely by the
> stack (vibration + temperature → `ctc43250372`; KPIs → iot-prometheus; 4G → the
> Hologram flow). `spectra-ingester` / `spectra-prometheus` are untouched.

## How it works

```
HiveMQ ─► iot-nodered "Stack metrics + temp" flow ─┬─ GET /metrics ─► iot-prometheus (:9091) ─► 9 System Health panels
   │  (computes KPIs from MQTT; the only metrics    └─ temperature → InfluxDB ctc43250372 ─────► Temperature panel
   │   source — Prometheus just scrapes it)
   └─ Hologram flow → sim/usage ─► (4G gauges in /metrics)
```

- **System Health KPIs:** the flow keeps fleet state from the MQTT stream and serves
  `ingester_* / hologram_sim_*` at `GET /metrics` (same names the panels use, so the
  panels didn't change — only their datasource). `iot-prometheus` scrapes it.
- **Temperature:** the flow writes **only** `sensor_health metric=temperature
  temperature_c` to `ctc43250372` — the one reading the "CTC Vibration Observability"
  flow doesn't store. It does **not** re-write vibration/battery/rssi (no double-write).

## Files

| File | What |
|---|---|
| `gen_flow.py` → `flow.json` | The Node-RED flow (exporter + temperature write). Core/function nodes only. |
| `prometheus.yml` | `iot-prometheus` scrape config (target `iot-nodered:1880`). |
| `alerts.yml` | 8 fleet/SIM alert rules. |
| `docker-compose.yml` | `iot-prometheus` container, host **:9091** (spectra-prometheus keeps :9090). |
| `deploy.sh` | Additive flow deploy (injects `INFLUX_TOKEN`/`INFLUX_DB`/`GATEWAY_SN`). |

## Deploy

```bash
python3 gen_flow.py
INFLUX_TOKEN=<write-token-for-ctc43250372> ./deploy.sh
# iot-prometheus:
scp prometheus.yml alerts.yml docker-compose.yml root@192.168.68.150:/opt/iot-prometheus/
ssh root@192.168.68.150 'cd /opt/iot-prometheus && docker compose up -d'
# Grafana: add datasource "IoT Prometheus" (prometheus) -> http://192.168.68.150:9091,
#          re-point the dashboard's 9 System Health panels to it.
```

## Notes

- **System Health source moved off the ingester.** This is the piece that lets the
  whole dashboard run with **zero dependency on `spectra-io`** (see the data-sources
  note in [`../README.md`](../README.md)).
- KPIs are all MQTT-derivable; gauges (last-seen/battery/rssi) repopulate per sensor as
  each reports after a Node-RED restart (counters reset → `rate()` handles it).
- Uses broker-arrival time, never the payload `Time` (gateway RTC ~5 months behind).
- `flows.backup.json` is a deploy scratch file (gitignored).
