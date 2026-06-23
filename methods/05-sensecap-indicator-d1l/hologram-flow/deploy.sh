#!/usr/bin/env bash
# Additive, idempotent deploy of the Hologram flow into iot-nodered, injecting the
# Hologram env vars into the flow tab (secret stays out of the repo). jq-free.
set -euo pipefail
cd "$(dirname "$0")"

NODERED="${NODERED:-http://192.168.68.150:1880}"
: "${HOLOGRAM_API_KEY:?set HOLOGRAM_API_KEY in the environment}"
HOLOGRAM_DEVICE_ID="${HOLOGRAM_DEVICE_ID:-4435961}"
GATEWAY_SN="${GATEWAY_SN:-43250372}"
[ -f flow.json ] || { echo "flow.json missing — run: python3 gen_flow.py"; exit 1; }

NODERED="$NODERED" HOLOGRAM_API_KEY="$HOLOGRAM_API_KEY" \
HOLOGRAM_DEVICE_ID="$HOLOGRAM_DEVICE_ID" GATEWAY_SN="$GATEWAY_SN" \
FLOW_JSON="$PWD/flow.json" BACKUP="$PWD/flows.backup.json" python3 - <<'PY'
import os, json, urllib.request
NR=os.environ["NODERED"]
cur=json.load(urllib.request.urlopen(NR+"/flows",timeout=20))
nodes=cur["flows"] if isinstance(cur,dict) else cur
json.dump(nodes,open(os.environ["BACKUP"],"w"))
keep=[n for n in nodes if n.get("z")!="holo_tab" and n.get("id") not in ("holo_tab","holo_hivemq")]
mine=json.load(open(os.environ["FLOW_JSON"]))
for n in mine:
    if n.get("id")=="holo_tab":
        n["env"]=[{"name":"HOLOGRAM_API_KEY","value":os.environ["HOLOGRAM_API_KEY"],"type":"str"},
                  {"name":"HOLOGRAM_DEVICE_ID","value":os.environ["HOLOGRAM_DEVICE_ID"],"type":"str"},
                  {"name":"GATEWAY_SN","value":os.environ["GATEWAY_SN"],"type":"str"}]
merged=keep+mine
req=urllib.request.Request(NR+"/flows",data=json.dumps(merged).encode(),method="POST",
    headers={"Content-Type":"application/json","Node-RED-Deployment-Type":"full"})
print("deploy HTTP",urllib.request.urlopen(req,timeout=40).status,"| nodes:",len(merged))
PY
echo "Done. Verify: mosquitto_sub -h localhost -t access360/${GATEWAY_SN}/sim/usage -v -C 1"
