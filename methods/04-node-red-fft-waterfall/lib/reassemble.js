/*
 * Multipart reassembly for CTC `dyn/vib/notify`.
 *
 * The gateway MAY split a large waveform payload into up to 10 sub-packets:
 *     { "MultiPart_ID": 12345, "Data": "<fragment of the original JSON string>" }
 * A client groups by MultiPart_ID, concatenates Data in arrival order, and parses
 * once the concatenation is valid JSON.
 *
 * NOTE (observed 2026-06-22 on the .150 HiveMQ broker): the ~514 KB waveform
 * arrived as a SINGLE whole JSON message — no MultiPart_ID fragments — because the
 * broker's MaximumPacketSize is large enough. So in practice the passthrough path
 * is the common one here; the reassembly path is kept for portability to brokers
 * with a smaller MaximumPacketSize (or different QoS).
 *
 * `feed(obj, store)` is stateful across calls via `store` (in Node-RED use the
 * function node's `context`). Returns the parsed reading object when complete,
 * else null (still buffering).
 */

'use strict';

function feed(obj, store) {
  // Whole-payload fast path: not a fragment -> already the full reading.
  if (obj == null || typeof obj !== 'object' || obj.MultiPart_ID === undefined) {
    return obj;
  }

  const id = obj.MultiPart_ID;
  const buf = store[id] || (store[id] = []);
  buf.push(String(obj.Data == null ? '' : obj.Data));

  // Try to parse the concatenation; succeed only once all fragments are in.
  const joined = buf.join('');
  try {
    const parsed = JSON.parse(joined);
    delete store[id];
    return parsed;
  } catch (e) {
    return null; // incomplete — keep buffering this MultiPart_ID
  }
}

// Pull the reading body out from the `{ "Reading": {...} }` wrapper if present.
// Live dyn/vib/notify nests fields under `Reading`; the vendor schema shows them
// at top level. Handle both.
function readingBody(parsed) {
  if (parsed && typeof parsed === 'object' && parsed.Reading && typeof parsed.Reading === 'object') {
    return parsed.Reading;
  }
  return parsed;
}

module.exports = { feed, readingBody };
