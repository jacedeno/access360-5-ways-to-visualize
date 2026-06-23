#!/usr/bin/env bash
# Deploy Method 3 (Grafana Live) INTO the existing iot-grafana on .150 — reuses the
# stack's Grafana, no new container. Idempotent: creates/updates the MQTT data source
# and imports the dashboard. Leaves all other dashboards/datasources untouched.
#
# Usage:
#   GRAFANA_TOKEN=<service-account-token> [INFLUX_TOKEN=<influx3-token>] ./deploy/deploy.sh [GRAFANA_URL]
#     GRAFANA_URL defaults to http://192.168.68.150:3000
#   - GRAFANA_TOKEN (required): Grafana service-account token (role Admin/Editor).
#   - INFLUX_TOKEN (optional): InfluxDB 3 token. If set, the InfluxDB 3 data source
#     is upserted so the "Fleet history" panels work. If unset, only the live
#     (MQTT) data source + dashboard are deployed (history panels show no data).
#
# One-time prerequisite (needs a container restart, so it is NOT done here):
#   ssh root@192.168.68.150 'docker exec iot-grafana grafana cli plugins install grafana-mqtt-datasource && docker restart iot-grafana'
set -euo pipefail
cd "$(dirname "$0")/.."

G="${1:-http://192.168.68.150:3000}"
: "${GRAFANA_TOKEN:?set GRAFANA_TOKEN to a Grafana service-account token (role Admin/Editor)}"
AUTH=(-H "Authorization: Bearer ${GRAFANA_TOKEN}")

upsert_ds() {  # $1=uid  $2=json-body
  local code
  code=$(curl -s -m 10 -o /tmp/ds_out.json -w '%{http_code}' "${AUTH[@]}" -H 'Content-Type: application/json' -X POST "$G/api/datasources" -d "$2")
  if [ "$code" = "409" ]; then
    curl -fsS -m 10 "${AUTH[@]}" -H 'Content-Type: application/json' -X PUT "$G/api/datasources/uid/$1" -d "$2" -o /tmp/ds_out.json
  fi
}

echo "→ checking grafana-mqtt-datasource plugin"
if ! curl -fsS -m 10 "${AUTH[@]}" "$G/api/plugins/grafana-mqtt-datasource/settings" >/dev/null 2>&1; then
  echo "  !! plugin not installed. Install it once (restarts Grafana):"
  echo "     ssh root@192.168.68.150 'docker exec iot-grafana grafana cli plugins install grafana-mqtt-datasource && docker restart iot-grafana'"
  exit 1
fi

echo "→ upserting MQTT data source (uid access360-mqtt)"
upsert_ds access360-mqtt '{"name":"ACCESS360 MQTT","uid":"access360-mqtt","type":"grafana-mqtt-datasource","access":"proxy","isDefault":false,"jsonData":{"uri":"tcp://iot-hivemq:1883"}}'
MQTT_UID=$(python3 -c 'import json;d=json.load(open("/tmp/ds_out.json"));print(d.get("datasource",d).get("uid","access360-mqtt"))')
echo "  MQTT data source uid: $MQTT_UID"

INFLUX_UID=access360-influx
if [ -n "${INFLUX_TOKEN:-}" ]; then
  echo "→ upserting InfluxDB 3 data source (uid access360-influx)"
  upsert_ds access360-influx "$(python3 -c "import json,os;print(json.dumps({'name':'ACCESS360 InfluxDB3','uid':'access360-influx','type':'influxdb','access':'proxy','url':'http://iot-influxdb3:8181','jsonData':{'version':'SQL','dbName':'spectra','insecureGrpc':True},'secureJsonData':{'token':os.environ['INFLUX_TOKEN']}}))")"
  INFLUX_UID=$(python3 -c 'import json;d=json.load(open("/tmp/ds_out.json"));print(d.get("datasource",d).get("uid","access360-influx"))')
  echo "  InfluxDB data source uid: $INFLUX_UID"
else
  echo "→ INFLUX_TOKEN not set — skipping InfluxDB data source (history panels will be empty)"
fi

echo "→ importing dashboard (uid access360-live)"
python3 - "$MQTT_UID" "$INFLUX_UID" <<'PY' > /tmp/dash_import.json
import json, sys
mqtt_uid, influx_uid = sys.argv[1], sys.argv[2]
dash = json.load(open("dashboards/fleet-health-live.json"))
s = json.dumps(dash).replace("${DS_MQTT_UID}", mqtt_uid).replace("${DS_INFLUX_UID}", influx_uid)
print(json.dumps({"dashboard": json.loads(s), "overwrite": True, "folderUid": ""}))
PY
curl -fsS -m 12 "${AUTH[@]}" -H 'Content-Type: application/json' -X POST "$G/api/dashboards/db" -d @/tmp/dash_import.json \
  | python3 -c 'import sys,json;d=json.load(sys.stdin);print("  ->",d.get("status"),d.get("url"))'

echo "✓ done — open ${G}/d/access360-live"
