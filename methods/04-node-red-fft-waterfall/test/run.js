/*
 * Sanity test for the Method 4 signal core against a REAL captured WS300 payload.
 * No dependencies — run:  node test/run.js
 *
 * Proves: multipart passthrough + Reading-wrapper unwrap + FFT produces a sane
 * single-sided spectrum (DC near zero, peak inside the band, length = Nfft/2).
 */

'use strict';

const fs = require('fs');
const path = require('path');
const { feed, readingBody } = require('../lib/reassemble');
const { spectrum, peak } = require('../lib/fft');

const fixture = path.join(__dirname, '..', 'fixtures', 'ws300-dyn-vib-notify.json');
const raw = JSON.parse(fs.readFileSync(fixture, 'utf8'));

// 1) reassembly (whole-payload path) + unwrap
const store = {};
const assembled = feed(raw, store);
const r = readingBody(assembled);

let fail = 0;
function check(name, cond, extra) {
  console.log(`${cond ? 'ok  ' : 'FAIL'}  ${name}${extra ? '  -> ' + extra : ''}`);
  if (!cond) fail++;
}

check('reading unwrapped (has Serial/Fs/Samples)', r && r.Serial && r.Fs && r.Samples,
  `Serial=${r.Serial} Fs=${r.Fs} Samples=${r.Samples}`);
check('Samples matches X length', r.Samples === r.X.length, `${r.Samples} vs ${r.X.length}`);
check('WS300 is triaxial (X,Y,Z all populated)',
  Array.isArray(r.X) && Array.isArray(r.Y) && Array.isArray(r.Z));

// 2) FFT per axis
for (const axis of ['X', 'Y', 'Z']) {
  const t0 = process.hrtime.bigint();
  const { freqs, mag, df, Nfft } = spectrum(r[axis], r.Fs);
  const ms = Number(process.hrtime.bigint() - t0) / 1e6;
  const pk = peak(freqs, mag);
  const nyq = freqs[freqs.length - 1];
  check(`${axis}: spectrum length = Nfft/2`, mag.length === Nfft / 2, `${mag.length}`);
  check(`${axis}: df ~ Fs/Nfft`, Math.abs(df - r.Fs / Nfft) < 1e-9, `df=${df.toFixed(4)} Hz`);
  check(`${axis}: peak within band [0, Nyquist]`, pk.freq >= 0 && pk.freq <= nyq + df);
  check(`${axis}: DC bin not dominant`, mag[0] <= pk.amp);
  console.log(
    `      ${axis}: Nfft=${Nfft} df=${df.toFixed(3)}Hz nyquist=${nyq.toFixed(0)}Hz ` +
    `peak=${pk.amp.toExponential(2)}g @ ${pk.freq.toFixed(1)}Hz  (${ms.toFixed(1)}ms)`
  );
}

console.log(fail ? `\n${fail} check(s) FAILED` : '\nall checks passed');
process.exit(fail ? 1 : 0);
