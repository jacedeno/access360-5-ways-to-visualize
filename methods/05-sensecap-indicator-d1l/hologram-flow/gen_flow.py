#!/usr/bin/env python3
"""Generate flow.json — the Hologram 4G poll as a Node-RED flow (replaces the
standalone hologram-poller container). Reuses the existing iot-nodered.

What it does: every POLL_INTERVAL_S it GETs the gateway SIM's Hologram device record
and publishes a compact usage summary (retained) to `access360/<GATEWAY_SN>/sim/usage`
— the same topic/schema the hologram-poller used, so Method 5's device is unchanged.

Hologram is org-scoped: the `/devices` LIST endpoint is empty, so we fetch the SIM
directly by id (`GET /devices/<HOLOGRAM_DEVICE_ID>`); usage lives under
`links.cellular[0]`. The gateway SIM = device 4435961 ("GL-excemca01", the LIVE one).

Config (Node-RED env vars on the flow tab; no secret committed):
  HOLOGRAM_API_KEY   (secret) Hologram dashboard API key
  HOLOGRAM_DEVICE_ID default 4435961
  GATEWAY_SN         default 43250372
  POLL_INTERVAL_S    default 600

Run: `python3 gen_flow.py` → flow.json. Core + function nodes only (no contrib).
"""
import json

TAB = "holo_tab"
BROKER = "holo_hivemq"

FN_REQ = r"""
// Build the Hologram device-detail request (direct by id; the list endpoint is
// empty for org-scoped accounts).
const key = env.get('HOLOGRAM_API_KEY');
if (!key) { node.warn('HOLOGRAM_API_KEY not set on the flow'); return null; }
const dev = env.get('HOLOGRAM_DEVICE_ID') || '4435961';
msg.method = 'GET';
msg.url = 'https://dashboard.hologram.io/api/1/devices/' + encodeURIComponent(dev)
        + '?apikey=' + encodeURIComponent(key);
msg.headers = { 'Accept': 'application/json' };
return msg;
"""

FN_PARSE = r"""
// Parse the Hologram device record -> compact sim/usage summary (same schema as the
// standalone poller, so Method 5's firmware is unchanged).
if (msg.statusCode && msg.statusCode !== 200) {
    node.warn('Hologram HTTP ' + msg.statusCode);
    return null;
}
const d = (msg.payload && msg.payload.data) ? msg.payload.data : {};
const cl = (d.links && Array.isArray(d.links.cellular)) ? d.links.cellular : [];
const cell = cl[0] || {};

const usedBytes = Number(cell.cur_billing_data_used || 0);
const limitBytes = Number(cell.overagelimit || 0);
const usedMb = Math.round(usedBytes / 1000000);
const limitMb = Math.round(limitBytes / 1000000);
const pct = limitMb > 0 ? Math.round((100 * usedMb) / limitMb) : 0;

const state = String(cell.state || '').toLowerCase();
const paused = state ? (state !== 'live') : false;

let lastAge = -1;
const last = cell.lastsession || cell.last_connect_time;
if (last) {
    let ts = NaN;
    if (/^\d+$/.test(String(last))) ts = Number(last);
    else { const t = Date.parse(String(last).replace('Z', '+00:00')); if (!isNaN(t)) ts = Math.floor(t / 1000); }
    if (!isNaN(ts)) lastAge = Math.max(0, Math.floor(Date.now() / 1000) - ts);
}

const gw = env.get('GATEWAY_SN') || '43250372';
msg.topic = 'access360/' + gw + '/sim/usage';
msg.payload = JSON.stringify({
    device: d.id || env.get('HOLOGRAM_DEVICE_ID') || '4435961',
    used_mb: usedMb,
    limit_mb: limitMb,
    pct: pct,
    paused: paused,
    last_connect_age_s: lastAge
});
return msg;
"""

flow = [
    {"id": TAB, "type": "tab", "label": "ACCESS360 — Hologram 4G poll", "disabled": False,
     "info": "Polls the gateway SIM's Hologram usage and publishes access360/<gw>/sim/usage "
             "(retained). Replaces the standalone hologram-poller container. Set "
             "HOLOGRAM_API_KEY (and optionally HOLOGRAM_DEVICE_ID/GATEWAY_SN/POLL_INTERVAL_S) "
             "as env vars on this tab."},

    {"id": BROKER, "type": "mqtt-broker", "name": "iot-hivemq", "broker": "iot-hivemq",
     "port": "1883", "clientid": "nodered-hologram", "autoConnect": True, "usetls": False,
     "protocolVersion": "5", "keepalive": "60", "cleansession": True},

    {"id": "holo_inject", "type": "inject", "z": TAB, "name": "every 600s + at start",
     "props": [{"p": "payload"}], "repeat": "600", "once": True, "onceDelay": "5",
     "payloadType": "date", "x": 170, "y": 120, "wires": [["holo_req"]]},
    {"id": "holo_req", "type": "function", "z": TAB, "name": "build request",
     "func": FN_REQ, "outputs": 1, "x": 380, "y": 120, "wires": [["holo_http"]]},
    {"id": "holo_http", "type": "http request", "z": TAB, "name": "Hologram device",
     "method": "use", "ret": "obj", "url": "", "persist": False, "x": 600, "y": 120,
     "wires": [["holo_parse"]]},
    {"id": "holo_parse", "type": "function", "z": TAB, "name": "→ sim/usage",
     "func": FN_PARSE, "outputs": 1, "x": 800, "y": 120, "wires": [["holo_out", "holo_dbg"]]},
    {"id": "holo_out", "type": "mqtt out", "z": TAB, "name": "sim/usage (retain)",
     "topic": "", "qos": "1", "retain": "true", "broker": BROKER, "x": 1010, "y": 100, "wires": []},
    {"id": "holo_dbg", "type": "debug", "z": TAB, "name": "sim/usage", "active": False,
     "tosidebar": True, "complete": "payload", "x": 1000, "y": 150, "wires": []},
]

with open("flow.json", "w") as f:
    json.dump(flow, f, indent=2)
print("wrote flow.json (%d nodes)" % len(flow))
