# MQTT Topics & Payload Schemas

Every MQTT channel the CTC Connect ACCESS360 gateway publishes, with exact JSON
keys, types, units, and example messages. This is what each visualization client
subscribes to.

- **Topic shape:** `access360/<gateway_sn>/<channel>` (no custom root in this
  deployment).
- **Payloads:** JSON. The **sensor serial** is the payload `Serial` field — it is
  not in the topic.
- **Time:** the `Time` string is `"yyyy-mm-dd hh:MM"` (some channels add seconds,
  `"yyyy-mm-dd hh:MM:ss"`). Treat as **UTC**.
- **Datetime example:** `"2026-06-20 14:18"`.

> Source of truth: transcribed from CTC's *"MQTT Topics for ConnectBridge Wireless
> Gateway"* manual and verified against a live captured payload (gateway
> `43250372`, sensor `22255728`, 2026-06-20). Where the manual and live data
> disagree it is noted inline.

---

## Channels the fleet actually publishes (subscribe side)

| Channel (`access360/<gw>/…`) | Meaning | Carries waveform? | Used by methods |
|---|---|---|---|
| `dyn/vib/notify` | Full dynamic vibration reading ⭐ | **Yes** (raw arrays, multipart) | 4 (FFT/waterfall) |
| `dyn/vib/notify/lite` | Overall-only dynamic reading | No | 1, 2, 3 |
| `proc/reading/notify` | WS100 process-control reading | No | 1, 2, 3 |
| `proc/checkin/notify` | Sensor heartbeat (online) | No | all (health) |
| `dyn/batt/notify` | Battery reading | No | all (health) |
| `dyn/temp/notify` | Temperature reading | No | 1, 2 |
| `rssi/notify` | BLE signal strength | No | all (health) |
| `error/notify` | Gateway error event | No | 1, 3, 5 |
| `status/notify` | Gateway status event | No | 1, 3, 5 |
| `ap/notify` | Gateway connect/disconnect | No | 5 (broker/gw status) |
| `dyn/notify` | Dynamic sensor connect/disconnect + full record | No | — |
| `will` | Last Will (gateway connection lost) | No | 5 (broker status) |

---

## 1. `dyn/vib/notify` — full dynamic vibration reading ⭐

The hero topic for **Method 4**. Fires when a dynamic sensor (WS200/WS300) takes a
new reading. Carries overall scalars **and** the raw time-domain waveform arrays.
Large → delivered **multipart** (see [`backend-context.md`](backend-context.md#5-multipart-payloads-critical-for-waveformfft)).

```jsonc
{
  "ID": 10,                 // int   — reading id (use to request FFT via dyn/fft/get)
  "Serial": 22255728,       // int   — sensor serial -> sensor_id
  "Time": "2026-06-20 14:18", // datetime (UTC)
  "Xpk":  0.162,  "Xpp": 0.294, "Xrms": 0.037,  // float, acceleration (g)
  "Ypk":  0.203,  "Ypp": 0.394, "Yrms": 0.056,  // float, acceleration (g)
  "Zpk": -0.160,  "Zpp": 0.299, "Zrms": 0.042,  // float, acceleration (g)
  "X":    [/* float, ... */], // raw acceleration samples, X axis (time domain)
  "Y":    [/* float, ... */], // raw acceleration samples, Y axis
  "Z":    [/* float, ... */], // raw acceleration samples, Z axis
  "Plot": [/* float, ... */], // correlating TIME values for the X/Y/Z arrays
  // Velocity scalars (in/s) and arrays appear on full/historical records:
  "VelXpk": -0.031, "VelXpp": 0.061, "VelXrms": 0.010,
  "VelYpk":  0.041, "VelYpp": 0.077, "VelYrms": 0.015,
  "VelZpk":  0.046, "VelZpp": 0.081, "VelZrms": 0.012,
  "VelX": [/* float, ... */], "VelY": [/* float, ... */],
  "VelZ": [/* float, ... */], "VelPlot": [/* float, ... */],
  "ReadPeriod": 500.0,      // reading length (ms)  [manual prints "ReadPoint" — typo]
  "Samples": 6400,          // int   — samples in the waveform
  "Fs": 12989               // int   — actual sampling frequency (Hz)
}
```

- **Axes:** WS200 (single-axial) populates one axis; WS300 (triaxial) all three.
- **`Plot`** here is **TIME** (the x-axis for the waveform). On `dyn/fft/get` the
  `Plot` field is **FREQUENCY** instead — do not confuse them.
- **FFT for waterfall:** either (A) compute the FFT client-side from `X`/`Y`/`Z`
  using `Fs`/`Samples`, or (B) request the gateway's FFT on demand via the publish
  topic `dyn/fft/get` with `{ "ID": <id> }`. Method 4 uses approach (A).
- **Units note:** acceleration scalars are in **g**; velocity scalars in **in/s**.

---

## 2. `dyn/vib/notify/lite` — overall-only dynamic reading

The lightweight summary: overall scalars, **no arrays**. Cheap to render — used by
the quick-look and dashboard methods.

```jsonc
{
  "ID": 5,                   // int
  "Serial": 22255721,        // int -> sensor_id
  "Time": "2026-06-19 21:00",// datetime (UTC)
  "Xrms": 1.5, "Xpk": 3.0, "Xpp": 3.0,  // float, acceleration (g)
  "Yrms": 0.5, "Ypk": 0.6, "Ypp": 0.6,  // float, acceleration (g)
  "Zrms": 0.2, "Zpk": 0.3, "Zpp": 0.3   // float, acceleration (g)
}
```

> The CTC manual lists this as `proc/vib/notify/lite`; the live deployment routes
> it as `dyn/vib/notify/lite`. Subscribe with `access360/<gw>/dyn/vib/notify/lite`.

---

## 3. `proc/reading/notify` — WS100 process-control reading

Process-control sensor (WS100). Same scalar shape as lite, plus temperature and
battery in the same message.

```jsonc
{
  "Serial": 30001234,        // int -> sensor_id
  "Time": "2026-06-19 21:00",// datetime (UTC)
  "Xrms": 1.0, "Xpk": 3.0, "Xpp": 3.0,  // float, acceleration (g)
  "Yrms": 0.5, "Ypk": 0.6, "Ypp": 0.6,  // float, acceleration (g)
  "Zrms": 0.2, "Zpk": 0.3, "Zpp": 0.3,  // float, acceleration (g)
  "Temp": 25,                // int — temperature (°C)
  "Batt": 90                 // int — battery (%, 0-100)
}
```

---

## 4. `proc/checkin/notify` — sensor heartbeat

Sent periodically with no new reading; proves the sensor is alive. Drives the
**"sensors online"** and **"last-seen age"** health metrics.

```jsonc
{
  "Serial": 11252280,         // int -> sensor_id
  "Time": "2026-06-19 20:08"  // datetime (UTC) — used as last-seen timestamp
}
```

Interpretation: presence of this message ⇒ `online = 1` for that sensor.

---

## 5. `dyn/batt/notify` — battery reading

```jsonc
{
  "ID": 10,                   // int
  "Serial": 22255728,         // int -> sensor_id
  "Time": "2026-06-19 20:10", // datetime (UTC)
  "Batt": 87                  // int — battery (%, 0-100)
}
```

On-demand: publish `dyn/batt/trigger` with `{ "Serial": <serial> }` to ask a
dynamic sensor for a fresh battery reading (~10 s later it replies on this topic).

---

## 6. `dyn/temp/notify` — temperature reading

```jsonc
{
  "ID": 10,                   // int
  "Serial": 22255728,         // int -> sensor_id
  "Time": "2026-06-19 20:10", // datetime (UTC)
  "Temp": 24                  // int — temperature (°C)
}
```

---

## 7. `rssi/notify` — BLE signal strength

Custom topic added by this deployment (not in the CTC manual). Note it uses
`Timestamp` (with seconds), not `Time`.

```jsonc
{
  "Serial": 11252280,             // int -> sensor_id
  "Rssi": -57,                    // int — BLE RSSI (dBm); higher (closer to 0) = stronger
  "Timestamp": "2026-06-19 20:03:17" // datetime with seconds (UTC)
}
```

RSSI guidance: weaker than **-75 dBm** is the practical drop threshold for CTC BLE.

---

## 8. `error/notify` — gateway error event

```jsonc
{
  "Attempt": "x",   // str — what was attempted
  "Error": "boom"   // str — error description
}
```

---

## 9. `status/notify` — gateway status event

```jsonc
{ "Status": "ok" }  // str
```

---

## 10. `ap/notify` — gateway connect/disconnect

Gateway-level presence. Useful for a hardware "broker / gateway up" indicator.

```jsonc
{
  "Serial": 43250372,   // int — gateway serial
  "Connected": true,    // bool
  "Firmware": "…",      // str
  "Hardware": "…",      // str
  "Software": "…",      // str
  "Nickname": "…"       // str
}
```

---

## 11. `dyn/notify` — dynamic sensor connect/disconnect (full record)

Fires on sensor connect/disconnect; `Connected` distinguishes the event. Carries
the full sensor configuration record (also returned by `dyn/config/notify` and the
`dyn/get` request topics).

```jsonc
{
  "Serial": 22255728, "Connected": true,
  "AccessPoint": "…", "PartNum": "…",
  "ReadRate": 6, "GMode": "±16g", "FreqMode": 6400, "Coupling": true,
  "ReadPeriod": 500, "Samples": 6400, "Fs": 12989, "Fmax": 2500.0,
  "HwVer": "…", "FmVer": "…", "Machine": "…",
  "Early": 0.10, "Crit": 0.25, "EarlyUnit": "RMS", "CritUnit": "RMS",
  "Nickname": "…", "Favorite": false, "VelocityMode": false
}
```

---

## 12. `will` — Last Will (gateway connection lost)

MQTT Last-Will message; the broker publishes it if the gateway drops. A hardware
or dashboard "broker/gateway online" tile can watch this.

```jsonc
{ "Message": "…" }  // str or JSON
```

---

## Publish (request) topics — reference

These go **broker → gateway** (the gateway is the subscriber). The visualization
methods are read-only and mostly do **not** publish, but two are useful:

| Topic | Body | Purpose |
|---|---|---|
| `dyn/batt/trigger` | `{ "Serial": int }` | Ask a sensor for a fresh battery reading (used by a "Refresh battery" button) |
| `dyn/vib/trigger` | `{ "Serial": int }` | Trigger an on-demand vibration reading → replies on `dyn/vib/notify` |
| `dyn/fft/get` | `{ "ID": int }` | Get the gateway-computed FFT for a reading id; response `[{ID,X,Y,Z,Plot}]` where `Plot` = FREQUENCY |
| `dyn/vib/get` | `{ Serials:[int], Start, End, Max }` | Historical full waveforms (same fields as `dyn/vib/notify`) |

---

## Field semantics & enums (reference)

| Field | Meaning | Values |
|---|---|---|
| `GMode` | Accelerometer range | ±8g, ±16g, ±32g, ±64g |
| `FreqMode` | Sampling-rate mode (Hz) | 400, 800, 1600, 3200, 6400, 12800, 25600 |
| `Samples` | Samples per reading | 1600, 3200, 6400, 12800, 25600 |
| `Fs` | Actual sampling frequency (Hz) | device-reported |
| `Fmax` | Max measured frequency (Hz) | 156.25, 312.5, 625, 1250, 2500, 5000, 10000 |
| `ReadPeriod` | Reading length (ms) | device-reported |
| `Plot` | X-axis for arrays | **TIME** on notify/get; **FREQUENCY** on `dyn/fft/get` |
| `Early`/`Crit` | Alert thresholds (+`*Unit`) | unit ∈ {RMS, Peak, Peak to Peak} |
