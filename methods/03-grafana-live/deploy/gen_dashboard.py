#!/usr/bin/env python3
"""Generate the ACCESS360 — Live Fleet Health dashboard.

Hybrid: a top "Fleet history" section backed by InfluxDB 3 (always populated, since
the spectra-ingester persists everything) + a "Live stream" section fed straight
from HiveMQ via the grafana-mqtt-datasource (Grafana Live, no DB).

Velocity (IPS, in/s) live comes from access360/43250372/derived/vib, a flat topic
produced by the Node-RED "velocity normalizer" (the MQTT datasource cannot read the
nested Reading wrapper of the raw dyn/vib/notify). History comes from the InfluxDB
`vibration` table's vel_*_rms columns.

Usage: gen_dashboard.py <MQTT_DS_UID> [INFLUX_DS_UID] [selftest]
"""
import json, sys
MQTT_UID = sys.argv[1]
INFLUX_UID = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2] == "selftest" else "access360-influx"
SELFTEST = "selftest" in sys.argv[2:]
DS = {"type": "grafana-mqtt-datasource", "uid": MQTT_UID}
IDS = {"type": "influxdb", "uid": INFLUX_UID}
GW = "access360/43250372"
_id = 0
def nid():
    global _id; _id += 1; return _id

# ---------- live (MQTT) helpers ----------
def tgt(topic):
    return {"datasource": DS, "topic": f"{GW}/{topic}", "refId": "A"}

def hide(*names):
    return [{"matcher": {"id": "byName", "options": n},
             "properties": [{"id": "custom.hideFrom", "value": {"viz": True, "legend": True, "tooltip": True}}]}
            for n in names]

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
        p["transformations"] = [{"id": "partitionByValues",
            "options": {"fields": ["Serial"], "keepFields": False, "naming": {"asLabels": True}}}]
    return p

def gauge(title, topic, field, x, y, w, h, unit, steps, desc=""):
    return {"id": nid(), "type": "gauge", "title": title, "description": desc,
            "datasource": DS, "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit, "min": 0,
                "thresholds": {"mode": "absolute", "steps": steps}, "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": f"/^{field}$/", "values": False},
                        "showThresholdLabels": False, "showThresholdMarkers": True}}

def stat(title, topic, field, x, y, w, h, unit, steps):
    return {"id": nid(), "type": "stat", "title": title, "datasource": DS,
            "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit,
                "thresholds": {"mode": "absolute", "steps": steps}, "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": f"/^{field}$/", "values": False},
                        "colorMode": "value", "graphMode": "area", "textMode": "value_and_name"}}

def mqtt_table(title, topic, x, y, w, h):
    return {"id": nid(), "type": "table", "title": title, "datasource": DS,
            "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"custom": {"filterable": True}}, "overrides": []},
            "options": {"showHeader": True, "cellHeight": "sm", "footer": {"show": False}}}

# ---------- history (InfluxDB) helpers ----------
def itarget(sql):
    return {"refId": "A", "datasource": IDS, "rawSql": sql, "resultFormat": "table", "rawQuery": True}

def isql_ts(title, sql, x, y, w, h, unit, ptype="timeseries", steps=None, desc=""):
    fc = {"defaults": {"unit": unit, "custom": {"drawStyle": "line", "showPoints": "always",
          "pointSize": 6, "spanNulls": True, "lineWidth": 2}}, "overrides": []}
    if steps:
        fc["defaults"]["thresholds"] = {"mode": "absolute", "steps": steps}
        fc["defaults"]["color"] = {"mode": "thresholds"}
    return {"id": nid(), "type": ptype, "title": title, "description": desc,
            "datasource": IDS, "targets": [itarget(sql)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": fc,
            "options": {"legend": {"displayMode": "list", "placement": "bottom", "showLegend": True},
                        "tooltip": {"mode": "multi"}}}

def igauge_bar(title, sql, x, y, w, h, unit, steps):
    return {"id": nid(), "type": "bargauge", "title": title, "datasource": IDS,
            "targets": [itarget(sql)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit, "min": 0, "max": 100,
                "thresholds": {"mode": "absolute", "steps": steps}, "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": False},
                        "displayMode": "gradient", "orientation": "horizontal"}}

def istat(title, sql, field, x, y, w, h, unit, steps, desc=""):
    return {"id": nid(), "type": "stat", "title": title, "description": desc, "datasource": IDS,
            "targets": [itarget(sql)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit,
                "thresholds": {"mode": "absolute", "steps": steps}, "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": f"/^{field}$/", "values": False},
                        "colorMode": "value", "graphMode": "area", "textMode": "value_and_name"}}

# ---------- shared ----------
def text(content, x, y, w, h):
    return {"id": nid(), "type": "text", "title": "", "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "options": {"mode": "markdown", "content": content}}

def row(title, y):
    return {"id": nid(), "type": "row", "title": title, "collapsed": False,
            "gridPos": {"x": 0, "y": y, "w": 24, "h": 1}, "panels": []}

VEL_STEPS = [{"color": "green", "value": None}, {"color": "yellow", "value": 0.157},
             {"color": "orange", "value": 0.275}, {"color": "red", "value": 0.44}]
BATT_STEPS = [{"color": "red", "value": None}, {"color": "yellow", "value": 20}, {"color": "green", "value": 40}]
RSSI_STEPS = [{"color": "red", "value": None}, {"color": "yellow", "value": -80}, {"color": "green", "value": -70}]
IN_S = "suffix: in/s"
G = "suffix: g"
W = f"gateway_sn='43250372'"  # excludes synthetic 99999999 test rows

panels = []
panels.append(text(
    "### ACCESS360 — Fleet Health\n"
    "**Top:** history from **InfluxDB 3** (`spectra`) — always populated. "
    "**Bottom:** **Grafana Live** straight from HiveMQ (no DB) — fills as sensors stream. "
    "Velocity (IPS) live is flattened from the on-demand `dyn/vib/notify` by the Node-RED normalizer.", 0, 0, 24, 3))

# ===== Fleet history (InfluxDB 3) — windows widened to where data exists =====
panels.append(row("Fleet history — InfluxDB 3 (always populated)", 3))
panels.append(isql_ts("BLE RSSI by sensor — last 7d",
    f"SELECT date_bin(interval '30 minutes', time) AS time, sensor_id, avg(rssi_dbm) AS rssi "
    f"FROM sensor_health WHERE metric='rssi' AND {W} AND time >= now() - interval '7 days' "
    f"GROUP BY 1, sensor_id ORDER BY 1", 0, 4, 16, 8, "dBm", desc="Per-sensor RSSI history (7 days)."))
panels.append(istat("Last recorded velocity — IPS",
    f"SELECT time, sqrt(vel_x_rms*vel_x_rms + vel_y_rms*vel_y_rms + vel_z_rms*vel_z_rms) AS vel_overall "
    f"FROM vibration WHERE {W} AND vel_x_rms IS NOT NULL AND time >= now() - interval '365 days' ORDER BY time",
    "vel_overall", 16, 4, 8, 8, IN_S, VEL_STEPS,
    desc="Most recent full-reading overall velocity. Full readings are on-demand, so this is the last known value."))
panels.append(igauge_bar("Battery % by sensor (latest)",
    f"SELECT time, sensor_id, battery_pct FROM sensor_health WHERE metric='battery' AND {W} "
    f"AND time >= now() - interval '60 days' ORDER BY time", 0, 12, 12, 7, "percent", BATT_STEPS))
panels.append(isql_ts("Sensors reporting per hour — 7d",
    f"SELECT date_bin(interval '1 hour', time) AS time, count(distinct sensor_id) AS sensors "
    f"FROM sensor_health WHERE {W} AND time >= now() - interval '7 days' GROUP BY 1 ORDER BY 1",
    12, 12, 12, 7, "none", ptype="timeseries", desc="Distinct sensors seen each hour (7 days)."))

# ===== Live stream (Grafana Live / MQTT) =====
panels.append(row("Live stream — Grafana Live via MQTT (no database)", 19))
panels.append(ts("Velocity RMS per axis — IPS (live)", "derived/vib", 0, 20, 16, 8, IN_S,
                 hidden=("Serial", "ID", "x_rms", "y_rms", "z_rms", "vel_overall"),
                 desc="Live from the velocity normalizer; fills when a full reading is taken."))
panels.append(gauge("Overall velocity — IPS (ISO zones)", "derived/vib", "vel_overall", 16, 20, 8, 8, IN_S, VEL_STEPS))
panels.append(ts("Acceleration RMS per axis (g) — live", "dyn/vib/notify/lite", 0, 28, 16, 7, G, hidden=("Serial", "ID")))
panels.append(stat("Z-axis RMS (latest, g)", "dyn/vib/notify/lite", "Zrms", 16, 28, 8, 7, G,
                   [{"color": "green", "value": None}, {"color": "yellow", "value": 0.5}, {"color": "red", "value": 1.0}]))
panels.append(ts("BLE RSSI by sensor — live", "rssi/notify", 0, 35, 12, 7, "dBm", hidden=("ID",), per_sensor=True))
panels.append(ts("Temperature — live", "dyn/temp/notify", 12, 35, 12, 7, "celsius", hidden=("Serial", "ID")))
panels.append(mqtt_table("Sensor heartbeats — live", "proc/checkin/notify", 0, 42, 12, 6))
panels.append(stat("Battery % (latest)", "dyn/batt/notify", "Batt", 12, 42, 6, 6, "percent", BATT_STEPS))
panels.append(stat("RSSI (latest)", "rssi/notify", "Rssi", 18, 42, 6, 6, "dBm", RSSI_STEPS))

if SELFTEST:
    p = ts("Live self-test — synthetic velocity (access360/99999999)", "derived/vib", 0, 48, 24, 7, IN_S,
           hidden=("Serial", "ID", "x_rms", "y_rms", "z_rms"))
    p["targets"] = [{"datasource": DS, "topic": "access360/99999999/derived/vib", "refId": "A"}]
    panels.append(p)

dashboard = {
    "uid": "access360-live", "title": "ACCESS360 — Live Fleet Health (MQTT)",
    "tags": ["access360", "mqtt", "live", "influxdb", "method-3"], "timezone": "browser",
    "schemaVersion": 39, "version": 0, "refresh": "30s",
    "time": {"from": "now-7d", "to": "now"}, "panels": panels,
    "editable": True, "graphTooltip": 0,
}
print(json.dumps({"dashboard": dashboard, "folderUid": "", "overwrite": True}, indent=2))
