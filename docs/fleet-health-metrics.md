# Spectra Fleet Health — Metric Set

The exact operational metrics the "Fleet Health" view tracks, so the
health-oriented methods (1 IoT MQTT Panel, 2, 3 Grafana, **5 SenseCAP Indicator**)
can reproduce the same picture. Each metric below lists **what it means**, **how to
derive it from the MQTT stream**, and **the alert threshold**.

> Every metric here is **derivable from raw MQTT** — that is the point. So this doc
> gives both the canonical definition/threshold and how to compute it from the
> topics. In the stack, **Node-RED** derives these from the MQTT stream and can
> persist them to InfluxDB 3 for trends; a device or quick-look method that only
> subscribes to MQTT computes them locally. (A Prometheus metrics backend is
> optional and can be added to the stack later — it is not required for any metric
> below.) Topic/payload details are in [`mqtt-topics.md`](mqtt-topics.md).

---

## The metric set at a glance

| Metric | Unit | Source signal | "Healthy" | Alert threshold |
|---|---|---|---|---|
| Broker up/down | bool | MQTT connection state | connected | down for 2 min → CRITICAL |
| Sensors online | count | `proc/checkin/notify` recency | all expected | last-seen ≥ 600 s ⇒ offline |
| Messages /s | msg/s | rate of any `access360/#` message | nonzero | — |
| Points written /s | pts/s | Node-RED → InfluxDB 3 write rate | nonzero | — (stack-side) |
| Errors /s | err/s | `error/notify` + parse failures | 0 | > 0 for 5 min → WARNING |
| Battery % | % | `dyn/batt/notify` / `proc/reading/notify` `Batt` | ≥ 20 % | < 20 % for 30 min → WARNING |
| RSSI | dBm | `rssi/notify` `Rssi` | ≥ -75 dBm | < -75 dBm for 30 min → WARNING |
| Last-seen age | s | now − last message time per sensor | < 600 s | > 46 800 s (13 h) → WARNING |
| 4G data used | bytes / % | Hologram SIM (or cumulative payload bytes) | < 80 % of cap | > 80 % of limit → WARNING |
| SIM live | bool | Hologram SIM state | LIVE | paused with usage → CRITICAL |

---

## 1. Broker up/down

- **Meaning:** is the MQTT broker reachable and is the gateway connected.
- **From MQTT:** the client's own connection callback (connected/disconnected). For
  the **gateway** specifically, watch `ap/notify` (`Connected`) and the `will`
  Last-Will message.
- **Threshold:** broker disconnected for **2 minutes** → CRITICAL.

## 2. Sensors online

- **Meaning:** how many sensors have reported recently.
- **From MQTT:** maintain a per-`Serial` "last message time" updated on **any**
  message for that sensor (especially `proc/checkin/notify`). A sensor is **online**
  if `now − last_seen < 600 s` (10 minutes).
- **Online cutoff:** **600 seconds**.

## 3. Messages per second

- **Meaning:** broker/gateway throughput; a flatline means data stopped.
- **From MQTT:** count received messages on `access360/#` over a sliding window;
  divide by the window. Optionally break down **by gateway** and **by channel**
  (the topic suffix).

## 4. Points written per second

- **Meaning:** InfluxDB write throughput — how fast Node-RED is persisting points.
- **From MQTT:** not directly observable from the broker. A method that only
  subscribes to MQTT should treat **messages/s** as its proxy. Documented here for
  parity with the stack's history view.

## 5. Errors per second

- **Meaning:** processing/gateway errors.
- **From MQTT:** count `error/notify` messages over a window; add any local JSON
  parse/multipart-reassembly failures.
- **Threshold:** rate **> 0 sustained 5 minutes** → WARNING.

## 6. Battery %

- **Meaning:** per-sensor battery charge.
- **From MQTT:** the `Batt` field on `dyn/batt/notify` (and on `proc/reading/notify`
  for WS100). Push a `dyn/batt/trigger {Serial}` to refresh on demand.
- **Threshold:** **< 20 %** for 30 minutes → WARNING.

## 7. RSSI (BLE signal strength)

- **Meaning:** sensor-to-gateway BLE link quality.
- **From MQTT:** the `Rssi` field on `rssi/notify` (dBm; closer to 0 = stronger).
- **Threshold:** **< -75 dBm** for 30 minutes → WARNING (CTC practical drop point).

## 8. Last-seen age

- **Meaning:** seconds since a sensor last said anything.
- **From MQTT:** `now − last_message_time[Serial]`. Use the payload `Time` /
  `Timestamp` (UTC) or local receive time.
- **Threshold:** **> 46 800 s (13 h)** → WARNING ("sensor silent").

## 9. 4G data usage (Hologram SIM)

The gateway backhaul is a Hologram 4G SIM, so cellular data is a cost/operational
metric.

- **Authoritative source:** the **Hologram API** (`https://dashboard.hologram.io/api/1`),
  per-SIM `cur_billing_data_used` vs `overagelimit`. Requires a `HOLOGRAM_API_KEY`
  (**secret — inject via env var, never commit**). MQTT can't carry this directly;
  a small helper polling the API (it runs in the stack — see Method 5's
  `hologram-poller/`, and Node-RED can do the same) republishes it for any method
  that wants true SIM usage.
- **MQTT-only proxy:** accumulate the **byte size of every received payload** per
  sensor/channel. The waveform on `dyn/vib/notify` (~514 KB per full reading)
  dominates, so this closely tracks real cellular cost.
- **Cost model (constants):**

  ```text
  projected_monthly_cost_usd =
      (cumulative_bytes / seconds_observed) * 2_592_000   # bytes/month
      / 1_000_000                                          # → MB
      * 0.03                                               # $0.03 per MB (Hologram)
  ```

  Constants: `2_592_000` s/month (30 d), `1 MB = 1_000_000 bytes`, **$0.03/MB**.
- **Thresholds:**
  - data used **> 80 %** of SIM limit → WARNING (early warning).
  - SIM **paused while usage > 0** → CRITICAL (gateway likely offline).
  - Hologram API poll stale **> 30 min** → WARNING.

---

## Alert thresholds (consolidated)

| Alert | Condition | For | Severity |
|---|---|---|---|
| Broker down | not connected | 2 min | CRITICAL |
| Errors | error rate > 0 | 5 min | WARNING |
| Sensor silent | last-seen age > 46 800 s (13 h) | 10 min | WARNING |
| Low battery | battery % < 20 | 30 min | WARNING |
| Weak signal | RSSI < -75 dBm | 30 min | WARNING |
| SIM data high | used / limit > 0.8 | 5 min | WARNING |
| SIM paused | live == 0 and used > 0 | 5 min | CRITICAL |
| Hologram poll failing | now − last_poll > 1800 s | 5 min | WARNING |

---

## Suggested layout for the SenseCAP Indicator "Health" screens

Method 5 mirrors this metric set on-device (status only, **no** raw vibration/FFT
plots). A natural multi-screen split with back/next navigation:

1. **Broker / Gateway** — broker up/down, gateway connected, messages/s, last error.
2. **Sensors** — online count, per-sensor last-seen age.
3. **Power & Signal** — per-sensor battery %, RSSI (dBm).
4. **4G / SIM** — data used vs limit (%), SIM live/paused, last connect age.
