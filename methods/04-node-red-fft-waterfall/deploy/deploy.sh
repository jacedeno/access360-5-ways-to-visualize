#!/usr/bin/env bash
# Deploy Method 4 INTO the existing iot-nodered on .150 (Dashboard 2.0) — additive
# and idempotent. Does NOT spin up a new container; reuses the iot stack's Node-RED
# and its iot-hivemq broker config. Re-running replaces this method's nodes cleanly
# (no duplicate tabs) and leaves the production "CTC Vibration" flow untouched.
#
# Usage:   ./deploy/deploy.sh [NODE_RED_HOST]      (default 192.168.68.150:1880)
# Needs:   python3, curl. The offline "Load fixture" inject also needs the fixture
#          inside the container (one-time):
#            ssh root@192.168.68.150 'docker exec iot-nodered mkdir -p /data/fixtures'
#            scp fixtures/ws300-dyn-vib-notify.json root@192.168.68.150:/tmp/
#            ssh root@192.168.68.150 'docker cp /tmp/ws300-dyn-vib-notify.json iot-nodered:/data/fixtures/'
set -euo pipefail
cd "$(dirname "$0")/.."

NR="http://${1:-192.168.68.150:1880}"
TS="$(date +%Y%m%d-%H%M%S)"
BACKUP="deploy/flows-backup-${TS}.json"

echo "→ backing up current flows from ${NR} to ${BACKUP}"
curl -fsS -m 15 "${NR}/flows" -o "${BACKUP}"

echo "→ merging flow.json additively (idempotent) and POSTing"
python3 - "$BACKUP" flow.json "$NR" <<'PY'
import json, sys, urllib.request
backup, flowfile, nr = sys.argv[1], sys.argv[2], sys.argv[3]
existing = json.load(open(backup)); existing = existing.get("flows", existing) if isinstance(existing, dict) else existing
mine = json.load(open(flowfile))
my_tab = next(n["id"] for n in mine if n.get("type") == "tab")
my_ids = {n["id"] for n in mine}
# drop any prior copy of this method (by our tab id, our node ids, or a360_ prefix)
def is_mine(n):
    return n.get("id") in my_ids or n.get("z") == my_tab or str(n.get("id","")).startswith("a360_")
kept = [n for n in existing if not is_mine(n)]
# sanity: the reused broker config must still be present in what we keep
assert any(n.get("id") == "a599e2ecbf5f669d" for n in kept), "reused broker config a599e2ecbf5f669d missing!"
merged = kept + mine
req = urllib.request.Request(nr + "/flows", data=json.dumps(merged).encode(),
    method="POST", headers={"Content-Type": "application/json", "Node-RED-Deployment-Type": "full"})
r = urllib.request.urlopen(req, timeout=30)
print(f"  POST /flows -> {r.status}  ({len(kept)} kept + {len(mine)} mine = {len(merged)} nodes)")
PY

echo "→ verifying tabs"
curl -fsS -m 10 "${NR}/flows" | python3 -c '
import sys, json
d = json.load(sys.stdin)
print("  tabs:", [n.get("label") for n in d if n.get("type") == "tab"])
print("  dashboard page:", [n.get("path") for n in d if n.get("type") == "ui-page"])
'
echo "✓ done — dashboard at ${NR}/dashboard/access360"
