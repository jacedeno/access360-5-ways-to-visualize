#!/usr/bin/env bash
# Deploy Method 3 (Grafana Live) INTO the existing iot-grafana on .150 — reuses the
# stack's Grafana, no new container. Idempotent: creates/updates the MQTT data source
# and imports the dashboard. Leaves all other dashboards/datasources untouched.
#
# Usage:
#   GRAFANA_TOKEN=<service-account-token> ./deploy/deploy.sh [GRAFANA_URL]
#     GRAFANA_URL defaults to http://192.168.68.150:3000
#
# One-time prerequisite (needs a container restart, so it is NOT done here):
#   ssh root@192.168.68.150 'docker exec iot-grafana grafana cli plugins install grafana-mqtt-datasource && docker restart iot-grafana'
set -euo pipefail
cd "$(dirname "$0")/.."

G="${1:-http://192.168.68.150:3000}"
: "${GRAFANA_TOKEN:?set GRAFANA_TOKEN to a Grafana service-account token (role Admin/Editor)}"
AUTH=(-H "Authorization: Bearer ${GRAFANA_TOKEN}")

echo "→ checking grafana-mqtt-datasource plugin"
if ! curl -fsS -m 10 "${AUTH[@]}" "$G/api/plugins/grafana-mqtt-datasource/settings" >/dev/null 2>&1; then
  echo "  !! plugin not installed. Install it once (restarts Grafana):"
  echo "     ssh root@192.168.68.150 'docker exec iot-grafana grafana cli plugins install grafana-mqtt-datasource && docker restart iot-grafana'"
  exit 1
fi

echo "→ upserting MQTT data source (uid access360-mqtt)"
DS_BODY='{"name":"ACCESS360 MQTT","uid":"access360-mqtt","type":"grafana-mqtt-datasource","access":"proxy","isDefault":false,"jsonData":{"uri":"tcp://iot-hivemq:1883"}}'
# create; if it already exists, update by uid
code=$(curl -s -m 10 -o /tmp/ds_out.json -w '%{http_code}' "${AUTH[@]}" -H 'Content-Type: application/json' -X POST "$G/api/datasources" -d "$DS_BODY")
if [ "$code" = "409" ]; then
  curl -fsS -m 10 "${AUTH[@]}" -H 'Content-Type: application/json' -X PUT "$G/api/datasources/uid/access360-mqtt" -d "$DS_BODY" -o /tmp/ds_out.json
fi
DSUID=$(python3 -c 'import json;d=json.load(open("/tmp/ds_out.json"));print(d.get("datasource",d).get("uid","access360-mqtt"))')
echo "  data source uid: $DSUID"

echo "→ importing dashboard (uid access360-live)"
python3 - "$DSUID" <<'PY' > /tmp/dash_import.json
import json, sys
ds_uid = sys.argv[1]
dash = json.load(open("dashboards/fleet-health-live.json"))
# substitute the DS placeholder
s = json.dumps(dash).replace("${DS_MQTT_UID}", ds_uid)
print(json.dumps({"dashboard": json.loads(s), "overwrite": True, "folderUid": ""}))
PY
curl -fsS -m 12 "${AUTH[@]}" -H 'Content-Type: application/json' -X POST "$G/api/dashboards/db" -d @/tmp/dash_import.json \
  | python3 -c 'import sys,json;d=json.load(sys.stdin);print("  ->",d.get("status"),d.get("url"))'

echo "✓ done — open ${G}/d/access360-live"
