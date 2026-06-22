# Sensors & Gateways In Use

The live fleet identifiers, so a visualization can pre-populate device lists,
filters, and per-sensor tiles instead of waiting to discover them.

---

## Gateways

| Gateway serial (`gateway_sn`) | Status | Notes |
|---|---|---|
| **`43250372`** | **Current / live** | The physical gateway publishing today. Subscribe `access360/43250372/#`. |
| `30250244` | Previous (archived) | Swapped out on 2026-06-19. Retained only as historical data; no longer publishes. |

The gateway connects to the broker over **4G LTE** using a **Hologram SIM**, which
is why 4G data usage is a tracked health metric (see
[`fleet-health-metrics.md`](fleet-health-metrics.md)).

---

## Sensors (live, gateway `43250372`)

These three dynamic sensors are the confirmed-active fleet (pre-seeded in the
platform for battery polling):

| Sensor serial (`sensor_id`) | Model | Kind | Axes |
|---|---|---|---|
| **`22255728`** | WS300 | Triaxial dynamic vibration | X, Y, Z |
| **`11251280`** | WS200 | Single-axial dynamic vibration | one axis |
| **`11251423`** | WS200 | Single-axial dynamic vibration | one axis |

Sensor `22255728` (WS300) is the one with a verified live full-waveform capture
(2026-06-20) — the best target for the **Node-RED FFT/waterfall** method.

### Additional serials seen in test fixtures

Useful as extra examples / mock data when building a UI before live data is
available:

| Sensor serial | Model | Seen in |
|---|---|---|
| `22255721` | WS300 | `dyn/vib/notify/lite` example |
| `11252280` | WS200 | checkin / rssi / proc reading examples |
| `30001234` | WS100 | `proc/reading/notify` example |

---

## Model conventions (how to label a sensor from its serial)

The platform derives the sensor model from the **serial prefix**:

| Serial prefix | Model | Kind |
|---|---|---|
| `2225…` | **WS300** | Triaxial dynamic (vibration) |
| `1125…` | **WS200** | Single-axial dynamic (vibration) |
| (per-channel, not by prefix) | **WS100** | Process control — identified by the `proc/reading/notify` channel |

Pseudocode:

```text
function modelFromSerial(serial):
    s = string(serial)
    if s startswith "2225": return "WS300"
    if s startswith "1125": return "WS200"
    return "WS_DYNAMIC"      # fall back; WS100 is set from the proc/ channel
```

---

## Suggested device list for UIs (copy/paste)

```json
{
  "gateway_sn": "43250372",
  "site": "Cement_Plant",
  "sensors": [
    { "sensor_id": "22255728", "model": "WS300", "axes": ["X", "Y", "Z"] },
    { "sensor_id": "11251280", "model": "WS200", "axes": ["X"] },
    { "sensor_id": "11251423", "model": "WS200", "axes": ["X"] }
  ]
}
```

> The `site` tag for this fleet is `Cement_Plant`.
