# Method 4 — Node-RED FFT / Waterfall ⭐

**Tier:** Hero (signal visualization) · **Platform:** Web browser (Node-RED Dashboard)

The headline visualization. This method consumes the **raw time-domain waveform**
on `dyn/vib/notify`, reassembles the multipart payload, computes an **FFT**, and
renders a scrolling **waterfall / spectrogram** — the frequency content of the
machine over time. This is the one that actually *shows the vibration*.

## Purpose

- Turn raw accelerometer waveforms into a **frequency-domain** view.
- Render a **waterfall (spectrogram)**: frequency on one axis, time on the other,
  amplitude as color — so emerging fault frequencies (imbalance, bearing defects,
  looseness) become visible as they develop.
- Also show a single-reading **FFT spectrum** (amplitude vs frequency) per axis.
- All fed **directly from MQTT** — no aggregator service.

## Why Node-RED

Node-RED is ideal here because the hard part is **flow logic**, not charting:
group multipart fragments → reassemble JSON → extract `X`/`Y`/`Z` + `Fs`/`Samples`
→ FFT → push a spectrum row into a heatmap. Node-RED's function nodes + dashboard
chart nodes do this with minimal code, and it already runs on the `.150` stack.

## How it works

```
access360/43250372/dyn/vib/notify (multipart fragments)
        │
        ▼  [mqtt in]  QoS 1
   [function] reassemble multipart   ← group by MultiPart_ID, concat Data, JSON.parse
        │
        ▼
   [function] extract waveform        ← X/Y/Z arrays, Fs, Samples, Serial, ID
        │
        ▼
   [function] FFT                     ← windowed real FFT → magnitude spectrum, bins → Hz
        │
        ├──► [chart: line]    FFT spectrum (amplitude vs frequency), per axis
        └──► [ui_heatmap /    waterfall: append spectrum as a new time row
              custom canvas]   (frequency × time, amplitude = color)
```

### Key signal facts (from [`../../docs/mqtt-topics.md`](../../docs/mqtt-topics.md))

- `dyn/vib/notify` carries `X`, `Y`, `Z` (raw acceleration, g) and `Plot` (time).
- `Fs` = sampling frequency (Hz), `Samples` = sample count → frequency resolution
  `df = Fs / Samples`; usable band up to Nyquist `Fs/2`.
- Live example: WS300 `22255728`, `Fs ≈ 12989 Hz`, `Samples = 6400`.
- WS200 = one axis populated; WS300 = all three.
- **Multipart is mandatory** here: reassemble `{MultiPart_ID, Data}` fragments
  before parsing — see
  [`../../docs/backend-context.md`](../../docs/backend-context.md#5-multipart-payloads-critical-for-waveformfft).

### Two ways to get the FFT

- **(A) Compute client-side (default):** run an FFT over `X`/`Y`/`Z` using `Fs`.
  Full control over windowing (Hann), units, and band. No extra round-trip.
- **(B) Ask the gateway:** publish `{ "ID": <id> }` to
  `access360/43250372/dyn/fft/get`; the response `[{ID,X,Y,Z,Plot}]` already has
  RMS magnitude per axis with `Plot` = **frequency**. Lower compute, but an extra
  request per reading.

## Prerequisites

- Node-RED (deploy as a **Docker container on `192.168.68.150`**, or reuse the
  existing `iot-node-red` on the stack — but keep this flow separate from the
  production ingestion flow).
- Palette nodes:
  - `node-red-dashboard` (charts/UI) — or `@flowfuse/node-red-dashboard` (Dashboard 2.0).
  - An FFT helper: `node-red-contrib-fft`, or a function node using a JS FFT lib
    (e.g. `fft.js`), or approach (B) above.
  - A heatmap/waterfall widget: `node-red-contrib-ui-heatmap`, ECharts via
    `node-red-contrib-ui-echarts`, or a `ui_template` canvas.
- Broker details — [`../../docs/backend-context.md`](../../docs/backend-context.md):
  `iot-hivemq:1883` / `192.168.68.150:1883`, TLS off, anonymous.
- Target sensor: WS300 `22255728` (has verified live waveform).

## Setup (outline — flow JSON lands in Phase 2)

1. Run Node-RED on `.150` on the `iot` Docker network.
2. Install the palette nodes above.
3. **`mqtt in`** → topic `access360/43250372/dyn/vib/notify`, QoS 1, broker
   `iot-hivemq:1883`.
4. **Reassembly function** — buffer fragments keyed by `MultiPart_ID`, concatenate
   `Data` in order, attempt `JSON.parse`; emit only when parse succeeds.
5. **Extract** `X`/`Y`/`Z`, `Fs`, `Samples`, `Serial`, `ID`.
6. **FFT function** — Hann window → real FFT → magnitude; build a `{freqs[], mag[]}`
   frame; map bin index → Hz with `df = Fs / Samples`.
7. **Spectrum chart** — line chart, amplitude vs frequency, one series per axis.
8. **Waterfall** — push each new spectrum as a row into a heatmap (frequency on X,
   time scrolling on Y, amplitude as color).
9. Add a sensor/axis selector and an optional `dyn/vib/trigger` button to force a
   fresh reading on demand.

## Notes

- **Data cost.** A full waveform is ~514 KB and rides the 4G link, so triggering
  readings has a real cellular cost (see the cost model in
  [`../../docs/fleet-health-metrics.md`](../../docs/fleet-health-metrics.md)).
  Prefer on-demand triggers over a tight auto-poll for the demo.
- **Scope.** This method is *signal* visualization. The on-device health monitor
  (Method 5) deliberately does **not** draw FFTs.

## Phase 2 (later)

Commit the exported `flow.json`, the FFT/reassembly function code, a
`docker-compose.yml` for a standalone Node-RED, and a screenshot/GIF of the live
waterfall.
