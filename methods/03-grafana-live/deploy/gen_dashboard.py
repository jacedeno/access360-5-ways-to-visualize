#!/usr/bin/env python3
"""Generate a Grafana Live (MQTT) dashboard for ACCESS360 fleet health."""
import json, sys
DS_UID = sys.argv[1]
DS = {"type": "grafana-mqtt-datasource", "uid": DS_UID}
GW = "access360/43250372"

def tgt(topic, ref="A"):
    return {"datasource": DS, "topic": f"{GW}/{topic}", "refId": ref}

def _hide(name):
    return {"matcher": {"id": "byName", "options": name},
            "properties": [{"id": "custom.hideFrom", "value": {"viz": True, "legend": True, "tooltip": True}}]}

def ts_panel(pid, title, topic, x, y, w=12, h=8, unit="none", desc="", hide=("Serial", "ID")):
    return {
        "id": pid, "type": "timeseries", "title": title, "description": desc,
        "datasource": DS, "targets": [tgt(topic)],
        "gridPos": {"x": x, "y": y, "w": w, "h": h},
        "fieldConfig": {"defaults": {
            "unit": unit, "custom": {"drawStyle": "line", "showPoints": "always",
            "pointSize": 6, "lineInterpolation": "linear", "spanNulls": True}},
            "overrides": [_hide(n) for n in hide]},
        "options": {"legend": {"displayMode": "list", "placement": "bottom", "showLegend": True},
            "tooltip": {"mode": "multi", "sort": "none"}},
    }

def stat_panel(pid, title, topic, field, x, y, w=6, h=6, unit="none", thresholds=None):
    steps = thresholds or [{"color": "green", "value": None}]
    return {
        "id": pid, "type": "stat", "title": title, "datasource": DS,
        "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
        "fieldConfig": {"defaults": {"unit": unit,
            "thresholds": {"mode": "absolute", "steps": steps},
            "color": {"mode": "thresholds"}}, "overrides": []},
        "options": {"reduceOptions": {"calcs": ["lastNotNull"], "fields": f"/^{field}$/", "values": False},
            "colorMode": "value", "graphMode": "area", "textMode": "value_and_name",
            "justifyMode": "auto", "orientation": "auto"},
    }

def table_panel(pid, title, topic, x, y, w=12, h=8):
    return {
        "id": pid, "type": "table", "title": title, "datasource": DS,
        "targets": [tgt(topic)], "gridPos": {"x": x, "y": y, "w": w, "h": h},
        "fieldConfig": {"defaults": {"custom": {"filterable": True}}, "overrides": []},
        "options": {"showHeader": True, "cellHeight": "sm",
            "footer": {"show": False}, "sortBy": []},
    }

panels = [
    {"id": 1, "type": "text", "title": "", "gridPos": {"x": 0, "y": 0, "w": 24, "h": 3},
     "options": {"mode": "markdown", "content":
        "### ACCESS360 — Live Fleet Health (Grafana Live via MQTT)\n"
        "Streaming straight from **HiveMQ** (`iot-hivemq:1883`) — no database in the path. "
        "Gateway `43250372`. Panels update as messages arrive over Grafana Live (WebSocket)."}},
    ts_panel(2, "BLE RSSI — live (dBm)", "rssi/notify", 0, 3, 12, 8, "dBm",
             "Rssi field from rssi/notify, per message as it streams."),
    table_panel(3, "Sensor heartbeats — live", "proc/checkin/notify", 12, 3, 12, 8),
    ts_panel(4, "Temperature — live (°C)", "dyn/temp/notify", 0, 11, 12, 8, "celsius"),
    ts_panel(5, "Overall vibration RMS — live (g)", "dyn/vib/notify/lite", 12, 11, 12, 8, "accG",
             "Xrms/Yrms/Zrms from dyn/vib/notify/lite (flat overall reading; published when a "
             "dynamic sensor takes a reading)."),
    stat_panel(6, "Battery % (latest)", "dyn/batt/notify", "Batt", 0, 19, 8, 6, "percent",
               [{"color": "red", "value": None}, {"color": "yellow", "value": 20}, {"color": "green", "value": 40}]),
    stat_panel(7, "RSSI (latest)", "rssi/notify", "Rssi", 8, 19, 8, 6, "dBm",
               [{"color": "red", "value": None}, {"color": "yellow", "value": -80}, {"color": "green", "value": -70}]),
]

# Optional self-test panel on a throwaway gateway topic NOT ingested by production
# (production subscribes access360/43250372/#). Used only to prove the live path.
if len(sys.argv) > 2 and sys.argv[2] == "selftest":
    p = ts_panel(99, "Live self-test (synthetic — access360/99999999)", "rssi/notify", 0, 25, 24, 8, "dBm",
                 "Synthetic messages published to access360/99999999/rssi/notify to prove "
                 "the MQTT datasource -> Grafana Live -> panel path; not real sensor data.")
    p["targets"] = [{"datasource": DS, "topic": "access360/99999999/rssi/notify", "refId": "A"}]
    panels.append(p)

dashboard = {
    "uid": "access360-live", "title": "ACCESS360 — Live Fleet Health (MQTT)",
    "tags": ["access360", "mqtt", "live", "method-3"], "timezone": "browser",
    "schemaVersion": 39, "version": 0, "refresh": "",
    "time": {"from": "now-5m", "to": "now"}, "panels": panels,
    "editable": True, "fiscalYearStartMonth": 0, "graphTooltip": 0,
}
print(json.dumps({"dashboard": dashboard, "folderUid": "", "overwrite": True}, indent=2))
