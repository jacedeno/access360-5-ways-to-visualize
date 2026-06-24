# The IoT Stack — reference & deploy

The single, self-contained stack that powers all five visualization methods. **HiveMQ
(MQTT) is the heart** — an ACCESS360 ConnectBridge gateway connects to it natively, and
everything else hangs off the broker. This doc is the canonical reference for the stack
itself (services, ports, volumes, config, deploy), so it can be reused on its own.

> **Deploy:** `cp .env.example .env` (fill the secrets), then `docker compose up -d`
> from the repo root — or import [`../docker-compose.yml`](../docker-compose.yml) as a
> Portainer stack. To reuse the stack in another repo, copy: `docker-compose.yml`,
> `.env.example`, and the `prometheus/` folder.

## Services

| Service (container) | Image | Host→container | Role |
|---|---|---|---|
| **hivemq** (`iot-hivemq`) | hivemq/hivemq-ce | `1883:1883` (`8000` WS optional) | MQTT 5.0 broker — the gateway + every method connect here |
| **nodered** (`iot-nodered`) | nodered/node-red | `1880:1880` | The glue: MQTT→InfluxDB ingestion, Method 4 FFT, `/metrics` exporter, Hologram 4G poll |
| **influxdb3** (`iot-influxdb3`) | influxdb:3-core | `8086:8181` | Time-series store (SQL via Flight SQL); db `ctc43250372` |
| **influxdb3-ui** (`iot-influxdb3-ui`) | influxdata/influxdb3-ui | `8888:80` | InfluxDB 3 admin/query UI |
| **grafana** (`iot-grafana`) | grafana/grafana | `3000:3000` | Dashboards (Method 3) |
| **prometheus** (`iot-prometheus`) | prom/prometheus | `9091:9090` | Scrapes Node-RED `/metrics` for Method 3's System Health KPIs |

All on one bridge network (compose project `iot` → Docker network `iot_iot`).

## Data flow

```
ACCESS360 gateway ──MQTT──► iot-hivemq ──► iot-nodered ─┬─► InfluxDB 3 (ctc43250372): readings + temperature
                                            (flows)      ├─► /metrics ──► iot-prometheus: System Health KPIs
                                                         ├─► Method 4: reassemble → FFT → waterfall (Dashboard)
                                                         └─► Hologram API poll → MQTT sim/usage (4G)
                                          iot-grafana ◄── InfluxDB (SQL) + iot-prometheus (PromQL) + live MQTT
```

### Node-RED flows (in `iot-nodered`)
- **CTC Vibration Observability** — ingests MQTT → InfluxDB `ctc43250372`.
- **Method 4 — FFT / Waterfall** — the spectrogram (`/dashboard/access360`).
- **ACCESS360 — Stack metrics + temp** — `/metrics` System-Health exporter + temperature ingest. See [`../methods/03-grafana-live/stack-metrics/`](../methods/03-grafana-live/stack-metrics/).
- **ACCESS360 — Hologram 4G poll** — publishes `sim/usage`. See [`../methods/05-sensecap-indicator-d1l/hologram-flow/`](../methods/05-sensecap-indicator-d1l/hologram-flow/).

## Config / secrets (`.env`)

| Var | Used by | Notes |
|---|---|---|
| `TZ` | all | timezone |
| `INFLUXDB3_ADMIN_TOKEN` | influxdb3-ui (+ write clients) | InfluxDB 3 admin token (`influxdb3 create token --admin`) |
| `INFLUXDB3_UI_SESSION_SECRET` | influxdb3-ui | long random string |
| `GF_ADMIN_USER` / `GF_ADMIN_PASSWORD` | grafana | Grafana admin login |

The Node-RED flows also read `INFLUX_TOKEN` (write to `ctc43250372`) and
`HOLOGRAM_API_KEY` as **flow-tab env vars** set in Node-RED (not in `.env`).

## Volumes

Repo template uses named volumes: `hivemq-data`, `hivemq-log`, `nodered-data`,
`influxdb3-data`, `influxdb3-ui-db`, `grafana-data`, `prometheus-data`. (The author's
homelab runs the same services as a Portainer stack with host bind-mounts under
`/opt/iot-stack/data/` — equivalent.)

## Boundary with `spectra-io` (do not touch)

`spectra-io` is a **separate project**: its `spectra-ingester` (writes the `spectra`
InfluxDB db) and its own `spectra-prometheus` happen to share the `iot_iot` Docker
network, but they are **not part of this stack** and must not be modified. The five
methods here use **only** the IoT-stack services above — sensor data comes from the
Node-RED ingestion into `ctc43250372`, never from the `spectra-io` ingester.

## Ports quick-reference

`1883` MQTT · `1880` Node-RED · `8086` InfluxDB 3 API · `8888` InfluxDB UI ·
`3000` Grafana · `9091` Prometheus.
