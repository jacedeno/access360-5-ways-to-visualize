#!/usr/bin/env bash
# Additive deploy of the System Health exporter (+ temperature ingest) flow into
# iot-nodered, injecting InfluxDB env into the flow tab. Then deploy iot-prometheus
# separately (docker compose up -d here) and add a Grafana "IoT Prometheus" datasource.
# jq-free.
set -euo pipefail
cd "$(dirname "$0")"

NODERED="${NODERED:-http://192.168.68.150:1880}"
: "${INFLUX_TOKEN:?set INFLUX_TOKEN (write access to ctc43250372)}"
INFLUX_DB="${INFLUX_DB:-ctc43250372}"
GATEWAY_SN="${GATEWAY_SN:-43250372}"
[ -f flow.json ] || { echo "flow.json missing — run: python3 gen_flow.py"; exit 1; }

NODERED="$NODERED" INFLUX_TOKEN="$INFLUX_TOKEN" INFLUX_DB="$INFLUX_DB" GATEWAY_SN="$GATEWAY_SN" \
FLOW_JSON="$PWD/flow.json" BACKUP="$PWD/flows.backup.json" python3 - <<'PY'
import os, json, urllib.request
NR=os.environ["NODERED"]
cur=json.load(urllib.request.urlopen(NR+"/flows",timeout=20))
nodes=cur["flows"] if isinstance(cur,dict) else cur
json.dump(nodes,open(os.environ["BACKUP"],"w"))
keep=[n for n in nodes if n.get("z")!="sm_tab" and n.get("id") not in ("sm_tab","sm_hivemq")]
mine=json.load(open(os.environ["FLOW_JSON"]))
for n in mine:
    if n.get("id")=="sm_tab":
        n["env"]=[{"name":"INFLUX_TOKEN","value":os.environ["INFLUX_TOKEN"],"type":"str"},
                  {"name":"INFLUX_DB","value":os.environ["INFLUX_DB"],"type":"str"},
                  {"name":"GATEWAY_SN","value":os.environ["GATEWAY_SN"],"type":"str"}]
merged=keep+mine
req=urllib.request.Request(NR+"/flows",data=json.dumps(merged).encode(),method="POST",
    headers={"Content-Type":"application/json","Node-RED-Deployment-Type":"full"})
print("flow deploy HTTP",urllib.request.urlopen(req,timeout=40).status,"| nodes:",len(merged))
PY

echo "Next: deploy iot-prometheus and the Grafana datasource:"
echo "  scp prometheus.yml alerts.yml docker-compose.yml root@<host>:/opt/iot-prometheus/"
echo "  ssh root@<host> 'cd /opt/iot-prometheus && docker compose up -d'   # binds host :9091"
echo "  Grafana: add datasource 'IoT Prometheus' (prometheus) -> http://<host>:9091,"
echo "           then point the dashboard's 9 System Health panels at it."
echo "Verify: curl -s $NODERED/metrics | head ; curl -s http://<host>:9091/api/v1/targets"
