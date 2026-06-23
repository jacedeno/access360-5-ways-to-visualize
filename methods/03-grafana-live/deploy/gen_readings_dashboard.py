#!/usr/bin/env python3
"""Generate "ACCESS360 — Fleet Health & Readings" (uid access360-fleet-readings).

Top "System Health" row: compact KPI gauges/stats from Prometheus (the
spectra-ingester's metrics). Bottom "Sensor Readings" row: vibration (velocity in/s,
acceleration g) + temperature from InfluxDB 3, filtered by a `$sensor` multi-select
so you overlay all sensors (compare amplitude) or drill into one.

Usage: gen_readings_dashboard.py <PROM_DS_UID> <INFLUX_DS_UID>
"""
import json, sys
PROM = {"type": "prometheus", "uid": sys.argv[1] if len(sys.argv) > 1 else "${DS_PROM_UID}"}
IDS = {"type": "influxdb", "uid": sys.argv[2] if len(sys.argv) > 2 else "${DS_INFLUX_UID}"}
GW = "43250372"
_id = 0
def nid():
    global _id; _id += 1; return _id

# ---- Prometheus panels ----
def pstat(title, expr, x, y, w, h, unit, steps, desc, mappings=None, graph="none"):
    fc = {"defaults": {"unit": unit, "thresholds": {"mode": "absolute", "steps": steps},
                       "color": {"mode": "thresholds"}}, "overrides": []}
    if mappings:
        fc["defaults"]["mappings"] = mappings
    return {"id": nid(), "type": "stat", "title": title, "description": desc, "datasource": PROM,
            "targets": [{"refId": "A", "datasource": PROM, "expr": expr, "instant": True}],
            "gridPos": {"x": x, "y": y, "w": w, "h": h}, "fieldConfig": fc,
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": False},
                        "colorMode": "background", "graphMode": graph, "textMode": "value_and_name",
                        "justifyMode": "auto"}}

def pgauge(title, expr, x, y, w, h, unit, steps, desc, mx=100):
    return {"id": nid(), "type": "gauge", "title": title, "description": desc, "datasource": PROM,
            "targets": [{"refId": "A", "datasource": PROM, "expr": expr, "instant": True}],
            "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit, "min": 0, "max": mx,
                "thresholds": {"mode": "absolute", "steps": steps}, "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": False},
                        "showThresholdMarkers": True}}

def pbar(title, expr, legend, x, y, w, h, unit, steps, desc, mx=100):
    return {"id": nid(), "type": "bargauge", "title": title, "description": desc, "datasource": PROM,
            "targets": [{"refId": "A", "datasource": PROM, "expr": expr, "legendFormat": legend, "instant": True}],
            "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit, "min": 0, "max": mx,
                "thresholds": {"mode": "absolute", "steps": steps}, "color": {"mode": "thresholds"}}, "overrides": []},
            "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": False},
                        "displayMode": "gradient", "orientation": "horizontal"}}

# ---- InfluxDB readings panel ----
def ireadings(title, sql, x, y, w, h, unit, desc):
    return {"id": nid(), "type": "timeseries", "title": title, "description": desc, "datasource": IDS,
            "targets": [{"refId": "A", "datasource": IDS, "rawSql": sql, "resultFormat": "table", "rawQuery": True}],
            "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "fieldConfig": {"defaults": {"unit": unit, "custom": {
                "drawStyle": "line", "showPoints": "always", "pointSize": 7,
                "lineInterpolation": "linear", "spanNulls": True, "lineWidth": 2}}, "overrides": []},
            "options": {"legend": {"displayMode": "table", "placement": "right", "showLegend": True,
                        "calcs": ["lastNotNull", "max", "mean"]}, "tooltip": {"mode": "multi", "sort": "desc"}}}

def text(content, x, y, w, h):
    return {"id": nid(), "type": "text", "title": "", "gridPos": {"x": x, "y": y, "w": w, "h": h},
            "options": {"mode": "markdown", "content": content}}

def row(title, y):
    return {"id": nid(), "type": "row", "title": title, "collapsed": False,
            "gridPos": {"x": 0, "y": y, "w": 24, "h": 1}, "panels": []}

UP_MAP = [{"type": "value", "options": {"1": {"text": "UP", "color": "green"}, "0": {"text": "DOWN", "color": "red"}}}]
OK_RED = [{"color": "red", "value": None}, {"color": "green", "value": 1}]
ERR = [{"color": "green", "value": None}, {"color": "red", "value": 0.001}]
AGE = [{"color": "green", "value": None}, {"color": "yellow", "value": 1800}, {"color": "red", "value": 7200}]
BATT = [{"color": "red", "value": None}, {"color": "yellow", "value": 20}, {"color": "green", "value": 40}]
USE = [{"color": "green", "value": None}, {"color": "yellow", "value": 80}, {"color": "red", "value": 95}]
PLAIN = [{"color": "blue", "value": None}]

# InfluxDB filters: real gateway + the $sensor multi-select; Z-axis is common to
# WS200 (single-axis) and WS300 (triaxial), so it is the clean cross-sensor metric.
WHERE = f"gateway_sn='{GW}' AND sensor_id IN (${{sensor:singlequote}})"

def vib_sql(field, valias):
    return (f"SELECT date_bin(interval '30 seconds', time) AS time, sensor_id, avg({field}) AS {valias} "
            f"FROM vibration WHERE {WHERE} AND {field} IS NOT NULL AND time >= now() - interval '6 hours' "
            f"GROUP BY 1, sensor_id ORDER BY 1")

# Temperature is stored in Celsius (temperature_c); convert to Fahrenheit for display.
TEMP_SQL = ("SELECT date_bin(interval '1 minute', time) AS time, sensor_id, avg(temperature_c) * 9.0 / 5.0 + 32 AS temp_f "
            f"FROM sensor_health WHERE metric='temperature' AND {WHERE} AND time >= now() - interval '6 hours' "
            "GROUP BY 1, sensor_id ORDER BY 1")

panels = []
panels.append(text(
    "## ACCESS360 — Fleet Health & Readings\n"
    "**Top:** system condition at a glance (broker, sensors online, last activity, battery, 4G, throughput) — from the "
    "Spectra ingester (Prometheus). **Bottom:** what the sensors send — vibration (in/s & g) and temperature — from "
    "InfluxDB. Use the **Sensor** dropdown to overlay all sensors (compare amplitude) or pick one.", 0, 0, 24, 3))

# --- System Health (Prometheus) ---
panels.append(row("System Health", 3))
panels.append(pstat("Broker", "max(ingester_mqtt_connected)", 0, 4, 3, 4, "none", OK_RED,
                    "MQTT broker connection (ingester_mqtt_connected).", mappings=UP_MAP))
panels.append(pstat("Sensors Online", "count(time() - ingester_sensor_last_seen_timestamp_seconds < 600) or vector(0)",
                    3, 4, 3, 4, "none", OK_RED, "Sensors heard from in the last 10 minutes."))
panels.append(pstat("Worst Last-Seen", "max(time() - ingester_sensor_last_seen_timestamp_seconds)",
                    6, 4, 3, 4, "s", AGE, "Oldest 'time since last activity' across the fleet."))
panels.append(pstat("Messages/s", "sum(rate(ingester_messages_total[5m]))", 9, 4, 3, 4, "none", PLAIN,
                    "MQTT messages ingested per second (5m rate).", graph="area"))
panels.append(pstat("Errors/s", "sum(rate(ingester_errors_total[5m])) or vector(0)", 12, 4, 3, 4, "none", ERR,
                    "Processing errors per second (5m rate)."))
panels.append(pgauge("4G Data Used", "100 * sum(hologram_sim_data_used_bytes) / clamp_min(sum(hologram_sim_data_limit_bytes), 1)",
                    15, 4, 4, 4, "percent", USE, "Cellular data used vs plan limit (Hologram SIMs)."))
panels.append(pstat("Points Written/s", "rate(ingester_points_written_total[5m])", 19, 4, 5, 4, "none", PLAIN,
                    "InfluxDB points written per second.", graph="area"))
panels.append(pbar("Battery % by sensor", "ingester_sensor_battery_percent", "{{sensor_id}}",
                   0, 8, 12, 5, "percent", BATT, "Latest battery per sensor (red < 20%)."))
panels.append(pbar("RSSI (dBm) by sensor", "ingester_sensor_rssi_dbm + 100", "{{sensor_id}}",
                   12, 8, 12, 5, "none", [{"color": "red", "value": None}, {"color": "yellow", "value": 20}, {"color": "green", "value": 30}],
                   "BLE signal per sensor (shown as dBm+100; higher = stronger).", mx=60))

# --- Sensor Readings (InfluxDB, $sensor-filtered) ---
panels.append(row("Sensor Readings — vibration & temperature", 13))
panels.append(ireadings("Vibration velocity — Z RMS (in/s) by sensor", vib_sql("vel_z_rms", "vel_z"),
                        0, 14, 12, 9, "suffix: in/s",
                        "Per-sensor Z-axis velocity RMS (IPS). Overlay to compare amplitude between machines; ISO-10816 territory."))
panels.append(ireadings("Vibration acceleration — Z RMS (g) by sensor", vib_sql("z_rms", "accel_z"),
                        12, 14, 12, 9, "suffix: g",
                        "Per-sensor Z-axis acceleration RMS (g)."))
panels.append(ireadings("Temperature (°F) by sensor", TEMP_SQL, 0, 23, 24, 8, "fahrenheit",
                        "Per-sensor temperature from dyn/temp/notify (stored in °C by the ingester, shown in °F)."))

dashboard = {
    "uid": "access360-fleet-readings", "title": "ACCESS360 — Fleet Health & Readings",
    "tags": ["access360", "fleet-health", "readings", "method-3"], "timezone": "browser",
    "schemaVersion": 39, "version": 0, "refresh": "30s",
    "time": {"from": "now-6h", "to": "now"}, "panels": panels,
    "editable": True, "graphTooltip": 1,
    "templating": {"list": [{
        "name": "sensor", "type": "custom", "label": "Sensor", "multi": True, "includeAll": True,
        "allValue": None, "current": {"text": ["All"], "value": ["$__all"]},
        "query": "22255728,11251423,11251280,11251722,11252280",
        "options": [], "datasource": None,
    }]},
}
print(json.dumps({"dashboard": dashboard, "folderUid": "", "overwrite": True}, indent=2))
