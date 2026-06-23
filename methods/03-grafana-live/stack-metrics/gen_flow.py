#!/usr/bin/env python3
"""Generate flow.json — the IoT-stack System Health exporter (+ temperature ingest)
for Method 3, running in the existing iot-nodered. Lets the dashboard's System Health
KPIs come from OUR stack (scraped by iot-prometheus) instead of spectra-io's ingester.

Two jobs, one MQTT subscription:
  1. Computes the 9 System Health KPIs from the MQTT stream + the Hologram flow's
     `sim/usage`, and serves them at `GET /metrics` (Prometheus text; same metric
     names the panels already use: ingester_* / hologram_sim_*).
  2. Writes the ONE reading the "CTC Vibration Observability" flow doesn't:
     temperature → `sensor_health metric=temperature temperature_c` in `ctc43250372`
     (fills the Temperature panel). It does NOT re-write vibration/battery/rssi/checkin
     (the CTC flow already writes those — no double-write).

Config (Node-RED env vars on the flow tab; no secret committed):
  INFLUX_URL    default http://iot-influxdb3:8181
  INFLUX_DB     default ctc43250372
  INFLUX_TOKEN  (secret) write token for ctc43250372
  GATEWAY_SN    default 43250372

Run: `python3 gen_flow.py` → flow.json. Core + function nodes only.
"""
import json

TAB = "sm_tab"
BROKER = "sm_hivemq"

FN_PROCESS = r"""
// Update System Health state for /metrics, and (only) write temperature to InfluxDB.
const m = flow.get('m') || {
    mqtt_connected: 0, messages_total: 0, errors_total: 0,
    points_written_total: 0, sensors: {}, sim: {}
};
m.messages_total++;

const parts = (msg.topic || '').split('/');     // access360/<gw>/<channel...>
const gw = parts[1] || env.get('GATEWAY_SN') || '43250372';
const channel = parts.slice(2).join('/');

let p = msg.payload;
if (Buffer.isBuffer(p)) p = p.toString();
if (typeof p === 'string') { try { p = JSON.parse(p); } catch (e) { m.errors_total++; flow.set('m', m); return null; } }
const r = (p && p.Reading) ? p.Reading : (p || {});

if (channel.indexOf('error/notify') === 0) m.errors_total++;

// Hologram SIM usage (published by the Hologram flow). Accept poller schema or raw.
if (channel === 'sim/usage') {
    const dev = String(r.device || gw);
    const e = {};
    if (r.used_mb != null) e.used = Number(r.used_mb) * 1000000;
    else if (r.cur_billing_data_used != null) e.used = Number(r.cur_billing_data_used);
    else if (r.used != null) e.used = Number(r.used);
    if (r.limit_mb != null) e.limit = Number(r.limit_mb) * 1000000;
    else if (r.overagelimit != null) e.limit = Number(r.overagelimit);
    else if (r.limit != null) e.limit = Number(r.limit);
    if (r.paused !== undefined) e.live = r.paused ? 0 : 1;
    else if (r.state !== undefined) e.live = (String(r.state).toLowerCase() === 'live') ? 1 : 0;
    m.sim[dev] = e;
    m.hologram_poll_last_success = Math.floor(Date.now() / 1000);
    flow.set('m', m);
    return null;
}

const serial = (r.Serial != null) ? String(r.Serial) : null;
const nowS = Math.floor(Date.now() / 1000);     // broker arrival time (RTC is skewed)
if (serial) {
    const s = m.sensors[serial] || {};
    s.last_seen_s = nowS;
    s.gateway_sn = gw;
    const batt = (r.Batt != null) ? r.Batt : (r.Battery != null ? r.Battery : undefined);
    if (batt !== undefined) s.battery = Number(batt);
    if (r.Rssi != null) s.rssi = Number(r.Rssi);
    m.sensors[serial] = s;
}

// Count points the stack persists (the CTC flow writes these to ctc43250372) — a
// proxy for write throughput so the "Points written/s" panel is meaningful.
const writeCh = ['dyn/vib/notify', 'dyn/vib/notify/lite', 'proc/reading/notify',
                 'dyn/batt/notify', 'rssi/notify', 'proc/checkin/notify', 'dyn/temp/notify'];
if (writeCh.some(c => channel.indexOf(c) === 0)) m.points_written_total++;
flow.set('m', m);

// Temperature is the ONLY measurement we write (CTC flow doesn't store it).
let temp = null;
if ((channel.indexOf('dyn/temp/notify') === 0 || channel.indexOf('proc/reading/notify') === 0)
    && serial && r.Temp != null) temp = Number(r.Temp);
if (temp === null) return null;

function esc(v) { return String(v).replace(/[ ,=]/g, '\\$&'); }
const ts = Date.now() * 1000000;   // clamp to ingest time (ns, ms precision)
const lp = 'sensor_health,gateway_sn=' + esc(gw) + ',sensor_id=' + esc(serial) +
           ',site=Cement_Plant,metric=temperature temperature_c=' + temp + ' ' + ts;

const base = env.get('INFLUX_URL') || 'http://iot-influxdb3:8181';
const db = env.get('INFLUX_DB') || 'ctc43250372';
const token = env.get('INFLUX_TOKEN') || '';
msg.method = 'POST';
msg.url = base + '/api/v3/write_lp?db=' + encodeURIComponent(db) + '&precision=nanosecond';
msg.headers = { 'Content-Type': 'text/plain' };
if (token) msg.headers['Authorization'] = 'Bearer ' + token;
msg.payload = lp;
return msg;
"""

FN_WRITERESULT = r"""
const code = msg.statusCode;
if (code !== 200 && code !== 204) {
    const m = flow.get('m'); if (m) { m.errors_total++; flow.set('m', m); }
    node.warn('InfluxDB temp write failed: ' + code);
}
return null;
"""

FN_STATUS = r"""
const m = flow.get('m') || {
    mqtt_connected: 0, messages_total: 0, errors_total: 0,
    points_written_total: 0, sensors: {}, sim: {}
};
const txt = (msg.status && msg.status.text || '').toLowerCase();
m.mqtt_connected = txt.indexOf('connected') >= 0 ? 1 : 0;
flow.set('m', m);
return null;
"""

FN_METRICS = r"""
const m = flow.get('m') || { sensors: {}, sim: {} };
const L = [];
L.push('# HELP ingester_mqtt_connected Node-RED broker connection (1=up).');
L.push('# TYPE ingester_mqtt_connected gauge');
L.push('ingester_mqtt_connected ' + (m.mqtt_connected || 0));
L.push('# TYPE ingester_messages_total counter');
L.push('ingester_messages_total ' + (m.messages_total || 0));
L.push('# TYPE ingester_errors_total counter');
L.push('ingester_errors_total ' + (m.errors_total || 0));
L.push('# TYPE ingester_points_written_total counter');
L.push('ingester_points_written_total ' + (m.points_written_total || 0));

const sensors = m.sensors || {};
const ids = Object.keys(sensors);
const lbl = (id) => '{sensor_id="' + id + '",gateway_sn="' + (sensors[id].gateway_sn || '') + '"}';
L.push('# TYPE ingester_sensor_last_seen_timestamp_seconds gauge');
for (const id of ids) if (sensors[id].last_seen_s != null) L.push('ingester_sensor_last_seen_timestamp_seconds' + lbl(id) + ' ' + sensors[id].last_seen_s);
L.push('# TYPE ingester_sensor_battery_percent gauge');
for (const id of ids) if (sensors[id].battery != null) L.push('ingester_sensor_battery_percent' + lbl(id) + ' ' + sensors[id].battery);
L.push('# TYPE ingester_sensor_rssi_dbm gauge');
for (const id of ids) if (sensors[id].rssi != null) L.push('ingester_sensor_rssi_dbm' + lbl(id) + ' ' + sensors[id].rssi);

const sim = m.sim || {};
const sims = Object.keys(sim);
const dl = (id) => '{device="' + id + '"}';
L.push('# TYPE hologram_sim_data_used_bytes gauge');
for (const id of sims) L.push('hologram_sim_data_used_bytes' + dl(id) + ' ' + (sim[id].used || 0));
L.push('# TYPE hologram_sim_data_limit_bytes gauge');
for (const id of sims) L.push('hologram_sim_data_limit_bytes' + dl(id) + ' ' + (sim[id].limit || 0));
L.push('# TYPE hologram_sim_live gauge');
for (const id of sims) if (sim[id].live != null) L.push('hologram_sim_live' + dl(id) + ' ' + sim[id].live);
if (m.hologram_poll_last_success != null) {
    L.push('# TYPE hologram_poll_last_success_timestamp_seconds gauge');
    L.push('hologram_poll_last_success_timestamp_seconds ' + m.hologram_poll_last_success);
}
msg.payload = L.join('\n') + '\n';
msg.statusCode = 200;
msg.headers = { 'Content-Type': 'text/plain; version=0.0.4' };
return msg;
"""

flow = [
    {"id": TAB, "type": "tab", "label": "ACCESS360 — Stack metrics + temp", "disabled": False,
     "info": "System Health exporter (/metrics, scraped by iot-prometheus) + temperature "
             "ingest into ctc43250372. Set INFLUX_TOKEN (+ INFLUX_DB/URL, GATEWAY_SN) as env "
             "vars on this tab."},
    {"id": BROKER, "type": "mqtt-broker", "name": "iot-hivemq", "broker": "iot-hivemq",
     "port": "1883", "clientid": "nodered-stackmetrics", "autoConnect": True, "usetls": False,
     "protocolVersion": "5", "keepalive": "60", "cleansession": True},

    {"id": "sm_mqtt", "type": "mqtt in", "z": TAB, "name": "access360/+/#",
     "topic": "access360/+/#", "qos": "1", "datatype": "auto-detect", "broker": BROKER,
     "x": 150, "y": 120, "wires": [["sm_proc"]]},
    {"id": "sm_proc", "type": "function", "z": TAB, "name": "state + temp write",
     "func": FN_PROCESS, "outputs": 1, "x": 380, "y": 120, "wires": [["sm_http"]]},
    {"id": "sm_http", "type": "http request", "z": TAB, "name": "InfluxDB temp write",
     "method": "use", "ret": "txt", "url": "", "persist": True, "x": 620, "y": 120, "wires": [["sm_res"]]},
    {"id": "sm_res", "type": "function", "z": TAB, "name": "write result",
     "func": FN_WRITERESULT, "outputs": 0, "x": 850, "y": 120, "wires": []},

    {"id": "sm_status", "type": "status", "z": TAB, "name": "mqtt status",
     "scope": ["sm_mqtt"], "x": 150, "y": 200, "wires": [["sm_fnstatus"]]},
    {"id": "sm_fnstatus", "type": "function", "z": TAB, "name": "set mqtt_connected",
     "func": FN_STATUS, "outputs": 0, "x": 400, "y": 200, "wires": []},

    {"id": "sm_httpin", "type": "http in", "z": TAB, "name": "GET /metrics",
     "url": "/metrics", "method": "get", "x": 160, "y": 300, "wires": [["sm_fnmetrics"]]},
    {"id": "sm_fnmetrics", "type": "function", "z": TAB, "name": "render exposition",
     "func": FN_METRICS, "outputs": 1, "x": 410, "y": 300, "wires": [["sm_httpres"]]},
    {"id": "sm_httpres", "type": "http response", "z": TAB, "name": "", "statusCode": "",
     "headers": {}, "x": 650, "y": 300, "wires": []},
]

with open("flow.json", "w") as f:
    json.dump(flow, f, indent=2)
print("wrote flow.json (%d nodes)" % len(flow))
