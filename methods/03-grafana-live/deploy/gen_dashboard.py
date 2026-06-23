#!/usr/bin/env python3
"""Generate the ACCESS360 — Live Fleet Health dashboard (Grafana Live / MQTT).

All panels stream straight from HiveMQ via the grafana-mqtt-datasource — no
database. Velocity (IPS, in/s) comes from access360/43250372/derived/vib, a flat
topic produced by the Node-RED "velocity normalizer" (the MQTT datasource cannot
read the nested Reading wrapper of the raw dyn/vib/notify).

Usage: gen_dashboard.py <DS_UID> [selftest]
"""
import json, sys
DS_UID = sys.argv[1]
DS = {"type": "grafana-mqtt-datasource", "uid": DS_UID}
GW = "access360/43250372"
_id = 0
def nid():
    global _id; _id += 1; return _id

def tgt(topic):
    return {"datasource": DS, "topic": f"{GW}/{topic}", "refId": "A"}

def hide(*names):
    return [{"matcher": {"id": "byName", "options": n},
             "properties": [{"id": "custom.hideFrom", "value": {"viz": True, "legend": True, "tooltip": True}}]}
            for n in names]

def partition_by_serial():
    return [{"id": "partitionByValues",
             "options": {"fields": ["Serial"], "keepFields": False, "naming": {"asLabels": True}}}]

def ts(title, topic, x, y, w, h, unit, hidden=("Serial", "ID"), per_sensor=False, desc=""):
    p = {"id": nid(), "type": "timeseries", "title": title, "description": desc,
         "datasource": DS, "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
         "fieldConfig": {"defaults": {"unit": unit, "custom": {
             "drawStyle": "line", "showPoints": "always", "pointSize": 6,
             "lineInterpolation": "linear", "spanNulls": True, "lineWidth": 2}},
             "overrides": list(hide(*hidden))},
         "options": {"legend": {"displayMode": "list", "placement": "bottom", "showLegend": True},
                     "tooltip": {"mode": "multi", "sort": "none"}}}
    if per_sensor:
        p["transformations"] = partition_by_serial()
    return p

def gauge(title, topic, field, x, y, w, h, unit, steps, desc=""):
    return {"id": nid(), "type": "gauge", "title": title, "description": desc,
            "datasource": DS, "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit, "min": 0,
                "thresholds": {"mode": "absolute", "steps": steps},
                "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": f"/^{field}$/", "values": False},
                        "showThresholdLabels": False, "showThresholdMarkers": True}}

def stat(title, topic, field, x, y, w, h, unit, steps):
    return {"id": nid(), "type": "stat", "title": title, "datasource": DS,
            "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit,
                "thresholds": {"mode": "absolute", "steps": steps},
                "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": f"/^{field}$/", "values": False},
                        "colorMode": "value", "graphMode": "area", "textMode": "value_and_name"}}

def table(title, topic, x, y, w, h):
    return {"id": nid(), "type": "table", "title": title, "datasource": DS,
            "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"custom": {"filterable": True}}, "overrides": []},
            "options": {"showHeader": True, "cellHeight": "sm", "footer": {"show": False}}}

def text(content, x, y, w, h):
    return {"id": nid(), "type": "text", "title": "", "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "options": {"mode": "markdown", "content": content}}

def row(title, y):
    return {"id": nid(), "type": "row", "title": title, "collapsed": False,
            "gridPos": {"x": 0, "y": y, "w": 24, "h": 1}, "panels": []}

# ISO 10816-style velocity severity (in/s, RMS) — rough general-machine zones.
VEL_STEPS = [{"color": "green", "value": None}, {"color": "yellow", "value": 0.157},
             {"color": "orange", "value": 0.275}, {"color": "red", "value": 0.44}]
BATT_STEPS = [{"color": "red", "value": None}, {"color": "yellow", "value": 20}, {"color": "green", "value": 40}]
RSSI_STEPS = [{"color": "red", "value": None}, {"color": "yellow", "value": -80}, {"color": "green", "value": -70}]
IN_S = "suffix: in/s"
G = "suffix: g"

panels = []
panels.append(text(
    "### ACCESS360 — Live Fleet Health  ·  Grafana Live via MQTT\n"
    "Streaming straight from **HiveMQ** (`iot-hivemq:1883`) — no database. Gateway `43250372`. "
    "**Velocity (IPS)** comes from `…/derived/vib` (flattened from the on-demand full "
    "`dyn/vib/notify` by the Node-RED velocity normalizer), so IPS panels fill when a full "
    "reading is taken. Health panels stream continuously as sensors report.", 0, 0, 24, 4))

# --- Vibration velocity (IPS) ---
panels.append(row("Vibration velocity — IPS (in/s)", 4))
panels.append(ts("Velocity RMS per axis — IPS", "derived/vib", 0, 5, 16, 9, IN_S,
                 hidden=("Serial", "ID", "x_rms", "y_rms", "z_rms", "vel_overall"),
                 desc="vel_x_rms / vel_y_rms / vel_z_rms (in/s) from the velocity normalizer."))
panels.append(gauge("Overall velocity — IPS (ISO zones)", "derived/vib", "vel_overall", 16, 5, 8, 9, IN_S, VEL_STEPS,
                    desc="Vector sum of axis velocity RMS. Green/Amber/Orange/Red ~ ISO 10816 general-machine zones."))

# --- Overall acceleration (g) ---
panels.append(row("Overall acceleration (g)", 14))
panels.append(ts("Acceleration RMS per axis (g)", "dyn/vib/notify/lite", 0, 15, 16, 8, G,
                 hidden=("Serial", "ID"),
                 desc="Xrms / Yrms / Zrms (g) from the flat dyn/vib/notify/lite overall reading."))
panels.append(stat("Z-axis RMS (latest, g)", "dyn/vib/notify/lite", "Zrms", 16, 15, 8, 8, G,
                   [{"color": "green", "value": None}, {"color": "yellow", "value": 0.5}, {"color": "red", "value": 1.0}]))

# --- Fleet health (continuous) ---
panels.append(row("Fleet health — continuous stream", 23))
panels.append(ts("BLE RSSI by sensor — live", "rssi/notify", 0, 24, 12, 8, "dBm",
                 hidden=("ID",), per_sensor=True,
                 desc="One line per sensor (partitioned by Serial)."))
panels.append(ts("Temperature — live", "dyn/temp/notify", 12, 24, 12, 8, "celsius", hidden=("Serial", "ID")))
panels.append(table("Sensor heartbeats — live", "proc/checkin/notify", 0, 32, 12, 7))
panels.append(stat("Battery % (latest)", "dyn/batt/notify", "Batt", 12, 32, 6, 7, "percent", BATT_STEPS))
panels.append(stat("RSSI (latest)", "rssi/notify", "Rssi", 18, 32, 6, 7, "dBm", RSSI_STEPS))

# Optional self-test panel on a throwaway gateway (not ingested by production).
if len(sys.argv) > 2 and sys.argv[2] == "selftest":
    p = ts("Live self-test — synthetic velocity (access360/99999999)", "derived/vib", 0, 39, 24, 8, IN_S,
           hidden=("Serial", "ID", "x_rms", "y_rms", "z_rms"))
    p["targets"] = [{"datasource": DS, "topic": "access360/99999999/derived/vib", "refId": "A"}]
    panels.append(p)

dashboard = {
    "uid": "access360-live", "title": "ACCESS360 — Live Fleet Health (MQTT)",
    "tags": ["access360", "mqtt", "live", "method-3"], "timezone": "browser",
    "schemaVersion": 39, "version": 0, "refresh": "",
    "time": {"from": "now-15m", "to": "now"}, "panels": panels,
    "editable": True, "graphTooltip": 0,
}
print(json.dumps({"dashboard": dashboard, "folderUid": "", "overwrite": True}, indent=2))
