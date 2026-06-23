#!/usr/bin/env bash
# Deploy "ACCESS360 — Fleet Health & Readings" INTO the existing iot-grafana on .150
# — reuses the stack's Grafana, no new container. Idempotent: upserts the InfluxDB 3
# data source and imports the dashboard. Leaves all other dashboards/datasources
# (incl. the pre-existing Prometheus) untouched.
#
# Usage:
#   GRAFANA_TOKEN=<sa-token> INFLUX_TOKEN=<influx3-token> ./deploy/deploy.sh [GRAFANA_URL]
#     GRAFANA_URL defaults to http://192.168.68.150:3000
#   - GRAFANA_TOKEN (required): Grafana service-account token (role Admin/Editor).
#   - INFLUX_TOKEN  (required): InfluxDB 3 token for the readings data source.
#   The Prometheus data source ("Spectra Prometheus") must already exist (KPI panels).
set -euo pipefail
cd "$(dirname "$0")/.."

G="${1:-http://192.168.68.150:3000}"
: "${GRAFANA_TOKEN:?set GRAFANA_TOKEN to a Grafana service-account token (role Admin/Editor)}"
: "${INFLUX_TOKEN:?set INFLUX_TOKEN to the InfluxDB 3 token (spectra database)}"
AUTH=(-H "Authorization: Bearer ${GRAFANA_TOKEN}")

echo "→ upserting InfluxDB 3 data source (uid access360-influx)"
DS_BODY=$(python3 -c "import json,os;print(json.dumps({'name':'ACCESS360 InfluxDB3','uid':'access360-influx','type':'influxdb','access':'proxy','url':'http://iot-influxdb3:8181','jsonData':{'version':'SQL','dbName':'spectra','insecureGrpc':True},'secureJsonData':{'token':os.environ['INFLUX_TOKEN']}}))")
code=$(curl -s -m 10 -o /tmp/ds_out.json -w '%{http_code}' "${AUTH[@]}" -H 'Content-Type: application/json' -X POST "$G/api/datasources" -d "$DS_BODY")
[ "$code" = "409" ] && curl -fsS -m 10 "${AUTH[@]}" -H 'Content-Type: application/json' -X PUT "$G/api/datasources/uid/access360-influx" -d "$DS_BODY" -o /tmp/ds_out.json
INFLUX_UID=$(python3 -c 'import json;d=json.load(open("/tmp/ds_out.json"));print(d.get("datasource",d).get("uid","access360-influx"))')
echo "  InfluxDB data source uid: $INFLUX_UID"

# Prometheus data source (pre-existing "Spectra Prometheus") — look up its uid.
PROM_UID=$(curl -s -m 10 "${AUTH[@]}" "$G/api/datasources" \
  | python3 -c 'import sys,json;ds=json.load(sys.stdin);print(next((d["uid"] for d in ds if d["type"]=="prometheus"), ""))')
[ -n "$PROM_UID" ] || { echo "  !! no Prometheus data source found (KPI panels need it)"; exit 1; }
echo "  Prometheus data source uid: $PROM_UID"

echo "→ importing dashboard: ACCESS360 — Fleet Health & Readings (uid access360-fleet-readings)"
python3 - "$PROM_UID" "$INFLUX_UID" <<'PY' > /tmp/dash_import.json
import json, sys
dash = json.load(open("dashboards/fleet-health-readings.json"))
s = json.dumps(dash).replace("${DS_PROM_UID}", sys.argv[1]).replace("${DS_INFLUX_UID}", sys.argv[2])
print(json.dumps({"dashboard": json.loads(s), "overwrite": True, "folderUid": ""}))
PY
curl -fsS -m 12 "${AUTH[@]}" -H 'Content-Type: application/json' -X POST "$G/api/dashboards/db" -d @/tmp/dash_import.json \
  | python3 -c 'import sys,json;d=json.load(sys.stdin);print("  ->",d.get("status"),d.get("url"))'

echo "✓ done — open ${G}/d/access360-fleet-readings"
