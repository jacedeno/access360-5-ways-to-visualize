#!/usr/bin/env python3
"""Hologram 4G SIM-usage poller for the SenseCAP Indicator D1L.

Polls the Hologram API for the gateway's SIM (`cur_billing_data_used` vs
`overagelimit`), normalizes it into a small JSON summary, and publishes it to
MQTT so the Indicator's "4G / SIM" screen can render it. The Indicator never
holds the Hologram API key — only this server-side container does.

Published topic (retained):  access360/<GATEWAY_SN>/sim/usage
Payload:
    {
      "used_mb": <int>,            # cur_billing_data_used, bytes -> MB
      "limit_mb": <int>,           # overagelimit (or plan cap), bytes -> MB
      "pct": <int>,               # used/limit * 100 (0 if limit unknown)
      "paused": <bool>,           # SIM not in a "live" state
      "last_connect_age_s": <int> # seconds since last cell connect (best-effort, -1 if unknown)
    }

Best-effort fields: the exact Hologram response keys vary by account/plan. The
mappings below are marked; verify against your account's /api/1 responses and
adjust HOLOGRAM_USED_KEY / HOLOGRAM_LIMIT_KEY if needed. The poller publishes
whatever it can parse and falls back to safe defaults for the rest.

Secrets: HOLOGRAM_API_KEY is read from the environment only (never hardcoded,
never committed). See .env.example.
"""

import json
import os
import sys
import time
from datetime import datetime, timezone

import requests
import paho.mqtt.client as mqtt

# --- config from environment -------------------------------------------------
HOLOGRAM_API_KEY = os.environ.get("HOLOGRAM_API_KEY", "").strip()
HOLOGRAM_API_BASE = os.environ.get("HOLOGRAM_API_BASE", "https://dashboard.hologram.io/api/1")
# Either give a specific device/SIM id, or let the poller pick the first device.
HOLOGRAM_DEVICE_ID = os.environ.get("HOLOGRAM_DEVICE_ID", "").strip()

MQTT_HOST = os.environ.get("MQTT_HOST", "iot-hivemq")   # in-stack DNS on .150
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
GATEWAY_SN = os.environ.get("GATEWAY_SN", "43250372")
POLL_INTERVAL_S = int(os.environ.get("POLL_INTERVAL_S", "300"))   # 5 min default

TOPIC = f"access360/{GATEWAY_SN}/sim/usage"

# Best-effort response key mapping. TODO: verify against your Hologram account.
HOLOGRAM_USED_KEY = os.environ.get("HOLOGRAM_USED_KEY", "cur_billing_data_used")
HOLOGRAM_LIMIT_KEY = os.environ.get("HOLOGRAM_LIMIT_KEY", "overagelimit")


def log(msg: str) -> None:
    ts = datetime.now(timezone.utc).isoformat(timespec="seconds")
    print(f"[{ts}] {msg}", flush=True)


def bytes_to_mb(b) -> int:
    try:
        return int(int(b) / 1_000_000)   # Hologram bills in MB = 1e6 bytes
    except (TypeError, ValueError):
        return 0


def fetch_device() -> dict:
    """Return the raw device record for the configured SIM (or the first one)."""
    auth = {"apikey": HOLOGRAM_API_KEY}
    if HOLOGRAM_DEVICE_ID:
        url = f"{HOLOGRAM_API_BASE}/devices/{HOLOGRAM_DEVICE_ID}"
        r = requests.get(url, params=auth, timeout=20)
        r.raise_for_status()
        data = r.json().get("data", {})
        return data if isinstance(data, dict) else {}
    # No specific device -> list and take the first.
    url = f"{HOLOGRAM_API_BASE}/devices"
    r = requests.get(url, params=auth, timeout=20)
    r.raise_for_status()
    items = r.json().get("data", [])
    return items[0] if items else {}


def parse_usage(device: dict) -> dict:
    """Map the raw Hologram device record to our compact summary.

    All mappings are best-effort; missing fields fall back to safe defaults.
    """
    # Usage/limit can live at the top level or under a nested object depending on
    # the account; probe a couple of common shapes.
    # Hologram device detail carries usage/state/sim under links.cellular[<n>]
    # (a LIST), not at the top level. The /devices list endpoint is empty for
    # org-scoped accounts, so the working path is a pinned-device fetch
    # (HOLOGRAM_DEVICE_ID) — flatten its first cellular link so the lookups find it.
    cell = {}
    _links = device.get("links")
    if isinstance(_links, dict) and isinstance(_links.get("cellular"), list) and _links["cellular"]:
        cell = _links["cellular"][0] or {}
    src = {**device, **cell}

    def dig(d: dict, key: str):
        if key in d:
            return d[key]
        for nest in ("data", "sim", "usage", "links"):
            if isinstance(d.get(nest), dict) and key in d[nest]:
                return d[nest][key]
        return None

    used_bytes = dig(src, HOLOGRAM_USED_KEY)
    limit_bytes = dig(src, HOLOGRAM_LIMIT_KEY)

    used_mb = bytes_to_mb(used_bytes)
    limit_mb = bytes_to_mb(limit_bytes)
    pct = int((100 * used_mb) / limit_mb) if limit_mb > 0 else 0

    # "live" / state — Hologram uses different state fields across plans.
    state = (dig(src, "state") or dig(src, "status") or "").lower()
    paused = state not in ("live",) if state else False

    # Last connect timestamp -> age in seconds (best-effort).
    last_connect_age = -1
    last_connect = dig(src, "lastsession") or dig(src, "last_connect_time")
    if last_connect:
        try:
            # Accept epoch seconds or ISO8601.
            if str(last_connect).isdigit():
                ts = int(last_connect)
            else:
                ts = int(datetime.fromisoformat(
                    str(last_connect).replace("Z", "+00:00")).timestamp())
            last_connect_age = max(0, int(time.time()) - ts)
        except (ValueError, TypeError):
            last_connect_age = -1

    return {
        "used_mb": used_mb,
        "limit_mb": limit_mb,
        "pct": pct,
        "paused": paused,
        "last_connect_age_s": last_connect_age,
    }


def main() -> int:
    if not HOLOGRAM_API_KEY:
        log("ERROR: HOLOGRAM_API_KEY is not set (env). Refusing to start.")
        return 2

    # paho-mqtt 2.x requires the callback API version argument.
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="hologram-poller",
        protocol=mqtt.MQTTv5,
    )
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    log(f"connected to MQTT {MQTT_HOST}:{MQTT_PORT}, publishing to {TOPIC}")

    while True:
        try:
            device = fetch_device()
            summary = parse_usage(device)
            client.publish(TOPIC, json.dumps(summary), qos=1, retain=True)
            log(f"published {summary}")
        except requests.RequestException as e:
            log(f"Hologram API error: {e}")
        except Exception as e:  # keep the loop alive on any transient failure
            log(f"unexpected error: {e}")
        time.sleep(POLL_INTERVAL_S)


if __name__ == "__main__":
    sys.exit(main())
