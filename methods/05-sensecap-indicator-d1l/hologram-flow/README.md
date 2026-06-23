# Hologram 4G poll — Node-RED flow (preferred; reuses `iot-nodered`)

Publishes the gateway SIM's 4G usage to **`access360/<gw>/sim/usage`** (retained) so
Method 5's device (and any other subscriber) can show 4G data used / limit / state —
**without a separate container**. This is the preferred path; the standalone
[`../hologram-poller/`](../hologram-poller/) does the same job as a Docker service and
remains as an alternative.

> **Status: live (2026-06-23)** — deployed into `iot-nodered` (tab "ACCESS360 —
> Hologram 4G poll"), publishing `{used_mb, limit_mb, pct, paused, ...}` (e.g. 57%).

## What it does

```
inject (every 600s + at start) → build request (URL + apikey from env)
   → http request GET /api/1/devices/<id> → parse links.cellular[0] → mqtt out (retained)
```

Hologram here is **org-scoped**: the `/devices` *list* endpoint returns nothing, so
the flow fetches the SIM **directly by id** (`GET /devices/<HOLOGRAM_DEVICE_ID>`);
usage lives under `links.cellular[0]` (`cur_billing_data_used` / `overagelimit` /
`state`). The gateway SIM is device **`4435961`** ("GL-excemca01", the LIVE one).

Published schema matches the standalone poller, so Method 5's firmware is unchanged:
`{ device, used_mb, limit_mb, pct, paused, last_connect_age_s }`.

## Files

| File | What |
|---|---|
| `gen_flow.py` | Generates `flow.json` (function bodies as readable Python strings). |
| `flow.json` | The Node-RED flow (core + function nodes only; no contrib, no dashboard). |
| `deploy.sh` | Additive, idempotent push into `iot-nodered`, injecting the env vars. |

## Config (Node-RED env vars on the flow tab; no secret committed)

`HOLOGRAM_API_KEY` (secret), `HOLOGRAM_DEVICE_ID` (default `4435961`),
`GATEWAY_SN` (default `43250372`).

## Deploy

```bash
python3 gen_flow.py
HOLOGRAM_API_KEY=<key> NODERED=http://192.168.68.150:1880 ./deploy.sh
# verify (from the .150 host, use the host-published broker port):
mosquitto_sub -h localhost -t access360/43250372/sim/usage -v -C 1
```

The deploy is additive: it preserves the other `iot-nodered` tabs ("CTC Vibration
Observability", "Method 4") and only (re)installs this flow's tab.
