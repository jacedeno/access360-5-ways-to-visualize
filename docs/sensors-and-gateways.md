# Sensors & Gateways In Use

The live fleet identifiers, so a visualization can pre-populate device lists,
filters, and per-sensor tiles instead of waiting to discover them.

> **Source of truth.** The authoritative, always-current backend model lives in the
> **`spectra-io`** repo (`~/repos/spectra-io`) — the Spectra ingester encodes the
> real parsing/classification rules and the CTC vendor docs. This file is a
> **self-contained snapshot** of what a visualization needs; when in doubt, or when
> the fleet changes, reconcile against:
> - `spectra-io/services/ingester/src/ingester/topics.py` — `model_from_serial()`
> - `spectra-io/services/ingester/src/ingester/parsers.py` — per-channel model (WS100 override)
> - `spectra-io/docs/vendor/ctc/sensor-specs.md` — WS100/WS200/WS300 specs
> - `spectra-io/docs/vendor/ctc/mqtt-topics.md` — topic/payload reference

---

## Gateways

| Gateway serial (`gateway_sn`) | Status | Notes |
|---|---|---|
| **`43250372`** | **Current / live** | The physical gateway publishing today. Subscribe `access360/43250372/#`. |
| `30250244` | Previous (archived) | Swapped out on 2026-06-19. Retained only as historical data; no longer publishes. |

The gateway connects to the broker over **4G LTE** using a **Hologram SIM**, which
is why 4G data usage is a tracked health metric (see
[`fleet-health-metrics.md`](fleet-health-metrics.md)). Per CTC sizing, one
ACCESS360 ConnectBridge services up to **10 BLE dynamic sensors**.

---

## Sensors (live, gateway `43250372`)

Confirmed by a **live broker capture (2026-06-22)** and cross-checked against the
`spectra-io` classification logic. The desk fleet is **5 sensors: 2× WS100 +
2× WS200 + 1× WS300**.

| Sensor serial (`sensor_id`) | Model | Kind | Axes | Confirmed by |
|---|---|---|---|---|
| **`22255728`** | **WS300** | Triaxial dynamic vibration | X, Y, Z | `dyn/vib/notify` (X/Y/Z each 6400 @ Fs≈12989 Hz) |
| **`11251423`** | **WS200** | Single-axial dynamic vibration | one axis (Z) | `dyn/vib/notify` (only Z populated) |
| **`11251280`** | **WS200** | Single-axial dynamic vibration | one axis | inferred (only `rssi` seen so far — confirm on next `dyn/vib`) |
| **`11251722`** | **WS100** | Process control (overall) | triaxial overall | `proc/reading/notify` — ⚠️ Batt **27%**, `Alert:true` |
| **`11252280`** | **WS100** | Process control (overall) | triaxial overall | `proc/reading/notify` — Batt 58%, `Alert:true` |

Sensor `22255728` (WS300) is the one with a verified live full-waveform capture
— the best target for the **Node-RED FFT/waterfall** method (Method 4).

> **⚠️ Serial-prefix collision.** Both WS100s above have a `1125…` serial — the same
> prefix as the WS200s. So **the prefix does NOT distinguish WS100 from WS200**;
> only the **channel** does (see the rule below). The old "1125 ⇒ WS200" shortcut
> misclassifies these two WS100s.

> **⏱ Clock skew caveat.** Live payloads carried a `Time`/`Timestamp` of
> `2026-01-28` while the real capture date was `2026-06-22` — the sensor/gateway
> clock is ~5 months behind. Health views (Method 5 "last-seen age") should derive
> freshness from **broker arrival time**, not the payload `Time` field.

---

## Model conventions (how to label a sensor)

Model derivation is **channel-first, prefix-second** — matching
`spectra-io`'s parser (`parsers.py` overrides the model to `WS100` whenever the
channel is `proc/reading/notify`, regardless of serial):

| Signal | Model | Kind |
|---|---|---|
| Channel `proc/reading/notify` | **WS100** | Process control — **wins over serial prefix** |
| Serial `2225…` (on a dynamic channel) | **WS300** | Triaxial dynamic (vibration) |
| Serial `1125…` (on a dynamic channel) | **WS200** | Single-axial dynamic (vibration) |

Pseudocode (channel-aware — the correct version):

```text
function modelFor(serial, channel):
    if channel contains "proc/reading":   return "WS100"   # process control wins
    s = string(serial)
    if s startswith "2225":                return "WS300"   # triaxial dynamic
    if s startswith "1125":                return "WS200"   # single-axial dynamic
    return "WS_DYNAMIC"                                     # unknown dynamic sensor
```

> Why channel-first: WS100 process-control sensors share the `1125…` prefix with
> WS200 in this fleet, so the serial alone is ambiguous. `proc/reading/notify`
> (overall RMS/Peak/Pk-Pk + Temp + Batt, no waveform) is the reliable WS100 tell.

### Model behavior at a glance (from `spectra-io/docs/vendor/ctc/sensor-specs.md`)

| | WS100 | WS200 | WS300 |
|---|---|---|---|
| Type | Process control (overall) | Dynamic vibration | Dynamic vibration |
| Axes | Triaxial (overall values) | **Single-axis** | **Triaxial** |
| Raw waveform / FFT | ❌ overall only | ✅ | ✅ |
| Primary channel | `proc/reading/notify` | `dyn/vib/notify` | `dyn/vib/notify` |
| Serial prefix (this fleet) | `1125…` (shared!) | `1125…` | `2225…` |

---

## Suggested device list for UIs (copy/paste)

```json
{
  "gateway_sn": "43250372",
  "site": "Cement_Plant",
  "sensors": [
    { "sensor_id": "22255728", "model": "WS300", "axes": ["X", "Y", "Z"], "channel": "dyn/vib/notify" },
    { "sensor_id": "11251423", "model": "WS200", "axes": ["Z"],           "channel": "dyn/vib/notify" },
    { "sensor_id": "11251280", "model": "WS200", "axes": ["X"],           "channel": "dyn/vib/notify" },
    { "sensor_id": "11251722", "model": "WS100", "axes": ["X", "Y", "Z"], "channel": "proc/reading/notify" },
    { "sensor_id": "11252280", "model": "WS100", "axes": ["X", "Y", "Z"], "channel": "proc/reading/notify" }
  ]
}
```

> The `site` tag for this fleet is `Cement_Plant`. `11251280`'s axis is a placeholder
> until a live `dyn/vib` reading confirms which single axis it populates.
</content>
</invoke>
