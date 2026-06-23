# Backend Context — Broker, Topology & Deployment

Everything a visualization client needs to connect to the live CTC Connect
ACCESS360 telemetry stream. This is the canonical reference; the per-method
READMEs link back here instead of repeating it.

---

## 1. MQTT broker (HiveMQ)

| Property | Value |
|---|---|
| Software | HiveMQ CE (Community Edition) |
| Protocol | MQTT **5.0** (MQTT 3.1.1 clients also work) |
| Host | `192.168.68.150` |
| Port | `1883` |
| TLS | **None** — plaintext on the private plane |
| Authentication | **Anonymous allowed** — no username/password, no client certificate |
| Container name | `iot-hivemq` (Docker, on the `.150` IoT stack) |
| In-stack DNS name | `iot-hivemq` (use this hostname for containers on the same Docker network) |
| Keepalive (reference) | `60` seconds |
| Default QoS (reference) | `1` |

> **Security note.** The broker is **not** exposed to the internet. It lives on the
> private plane and is reachable only over the LAN (`192.168.68.0/24`) or the
> homelab VPN. There are no secrets to redact in the connection itself because it
> is anonymous; do not add port-forwarding to expose it. The public plane is the
> web application only.

### Client connection settings (copy/paste)

```
Host:      192.168.68.150
Port:      1883
TLS:       off
Username:  (leave empty)
Password:  (leave empty)
Client ID: <pick a unique id per client, e.g. mqttx-laptop, indicator-d1l>
Keepalive: 60
Clean session: true
```

When connecting **from another container on the same Docker network** on `.150`,
use host `iot-hivemq` instead of the IP.

### Subscription patterns

| Pattern | Scope |
|---|---|
| `access360/#` | Everything from every gateway (simplest for a sniff) |
| `access360/+/#` | All gateways, all channels (the stack's Node-RED ingestion wildcard) |
| `access360/43250372/#` | Only the current live gateway |
| `access360/43250372/dyn/vib/notify` | Only full dynamic vibration readings (waveform) |

---

## 2. Network topology

```
 Internet ──► Cloudflare Tunnel ──► Web app only (public plane)

 Private plane (LAN 192.168.68.0/24 + homelab VPN):
   IoT Docker host — the one stack:
     ├─ iot-hivemq      MQTT 5.0 broker        :1883   ← the protagonist
     ├─ iot-nodered     Node-RED (ingest + FFT):1880
     ├─ iot-influxdb3   InfluxDB 3 Core         :8086  (native :8181)
     └─ iot-grafana     Grafana                 :3000
```

- The **stack** holds the broker, Node-RED ingestion, the InfluxDB history store,
  and Grafana — install it once (Portainer or `docker compose`).
- Keep it on the **private plane** (LAN / homelab VPN); the broker is **not** meant
  to be exposed to the internet.
- Methods run on the LAN or VPN and connect to the stack: 1, 2, 4 and 5 straight to
  `iot-hivemq` / `<host>:1883`; method 3 (Grafana) reads InfluxDB history too.

---

## 3. The data path (where the MQTT stream comes from)

```
CTC ACCESS360 BLE sensors  ──BLE──►  ConnectBridge wireless gateway
                                     (serial 43250372)
                                            │  4G LTE (Hologram SIM)
                                            ▼
                                     HiveMQ CE broker  (192.168.68.150:1883)
                                            │  topics: access360/43250372/<channel>
                          ┌─────────────────┴─────────────────┐
                          ▼                                     ▼
              Node-RED ingestion (in the stack)      The 5 methods in this repo
              → InfluxDB 3 (history) + health         (live from MQTT; #3 also
                                                       reads InfluxDB history)
```

The gateway connects to the broker over a 4G LTE link backed by a **Hologram SIM**
(this is what makes 4G data usage a first-class health metric — see
[`fleet-health-metrics.md`](fleet-health-metrics.md)). Sensors speak BLE to the
gateway; the gateway publishes JSON over MQTT.

---

## 4. Topic structure

The gateway publishes under its serial with **no custom root**:

```
access360/<gateway_sn>/<channel>
```

- `<gateway_sn>` — the gateway serial, e.g. `43250372`.
- `<channel>` — the message type, e.g. `dyn/vib/notify`, `proc/checkin/notify`.
- The root is configurable in the gateway UI but **this deployment uses none**.
- The sensor serial is **not** in the topic — it arrives in the payload `Serial`
  field. See [`mqtt-topics.md`](mqtt-topics.md).

---

## 5. Multipart payloads (critical for waveform/FFT)

MQTT brokers enforce a `MaximumPacketSize`. When a payload exceeds it (the raw
vibration waveform arrays do), the gateway splits it into **up to 10 sub-packets**:

```json
{ "MultiPart_ID": 12345, "Data": "<fragment of the original JSON string>" }
```

A client must **group by `MultiPart_ID` and concatenate `Data` in arrival order**
until the concatenation parses as valid JSON. This applies primarily to
`dyn/vib/notify` (the full waveform). Any method that consumes the raw waveform —
notably **Method 4 (Node-RED FFT/waterfall)** — must implement reassembly. The
health-only methods (1, 2, 3, 5) can ignore it.

---

## 6. Deployment target for helper services

A method's helper (a Node-RED flow — e.g. the ingestion flow or the Hologram 4G
flow — a Grafana dashboard, an MQTT-over-WebSocket bridge) installs **into the
existing `iot-nodered` / shared stack** — the same Docker network as `iot-hivemq`,
so it reaches the broker by its in-stack name. There is no custom private aggregator
microservice in the path: ingestion is a Node-RED flow inside the stack, and the
live methods talk to HiveMQ directly.

---

## 7. WebSocket access (for browser-based clients)

Browser clients (e.g. Grafana's MQTT data source in some configs, or any
MQTT.js-based panel) need **MQTT over WebSockets**, not raw TCP 1883. HiveMQ CE
exposes a WebSocket listener; if it is not yet enabled on this broker, add a
WebSocket listener (commonly port `8000`) to the HiveMQ config, or run a small
WebSocket bridge container on `.150`. Each affected method README calls this out
in its prerequisites.
