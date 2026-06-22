/*
 * Dependency-free real-FFT + single-sided amplitude spectrum.
 *
 * Used by Method 4 (Node-RED FFT/waterfall). The Node-RED "FFT" function node
 * embeds a copy of this logic; this file is the canonical, unit-testable source
 * (run `node test/run.js`).
 *
 * The CTC waveform is 6400 samples (not a power of two), so we Hann-window the
 * real samples and zero-pad to the next power of two (8192) for an in-place
 * radix-2 Cooley-Tukey FFT. Frequency resolution becomes df = Fs / Nfft.
 */

'use strict';

function nextPow2(n) {
  let p = 1;
  while (p < n) p <<= 1;
  return p;
}

// Hann window of length N (periodic-ish, matches typical analyzers).
function hann(N) {
  const w = new Float64Array(N);
  for (let i = 0; i < N; i++) w[i] = 0.5 * (1 - Math.cos((2 * Math.PI * i) / (N - 1)));
  return w;
}

// In-place iterative radix-2 FFT. re/im are Float64Array of equal pow-2 length.
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      const tr = re[i]; re[i] = re[j]; re[j] = tr;
      const ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (-2 * Math.PI) / len;
    const wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cwr = 1, cwi = 0;
      for (let k = 0; k < len / 2; k++) {
        const a = i + k, b = i + k + len / 2;
        const vr = re[b] * cwr - im[b] * cwi;
        const vi = re[b] * cwi + im[b] * cwr;
        re[b] = re[a] - vr; im[b] = im[a] - vi;
        re[a] += vr;        im[a] += vi;
        const ncwr = cwr * wr - cwi * wi;
        cwi = cwr * wi + cwi * wr;
        cwr = ncwr;
      }
    }
  }
}

/*
 * Single-sided amplitude spectrum of a real time-domain signal.
 *   samples : array of acceleration samples (g)
 *   Fs      : sampling frequency (Hz)
 * Returns { freqs[Hz], mag[g], df, Nfft } over 0 .. Nyquist.
 * Amplitude is normalized by the window's coherent gain so a pure tone reads
 * back close to its true peak amplitude.
 */
function spectrum(samples, Fs) {
  const N0 = samples.length;
  const w = hann(N0);
  const Nfft = nextPow2(N0);
  const re = new Float64Array(Nfft);
  const im = new Float64Array(Nfft);
  let winSum = 0;
  for (let i = 0; i < N0; i++) {
    re[i] = samples[i] * w[i];
    winSum += w[i];
  }
  fft(re, im);
  const half = Nfft >> 1;
  const mag = new Float64Array(half);
  const freqs = new Float64Array(half);
  const df = Fs / Nfft;
  for (let k = 0; k < half; k++) {
    const m = Math.sqrt(re[k] * re[k] + im[k] * im[k]);
    mag[k] = ((k === 0 ? 1 : 2) * m) / winSum; // single-sided, coherent-gain scaled
    freqs[k] = k * df;
  }
  return { freqs, mag, df, Nfft };
}

// Index/frequency/amplitude of the dominant bin (ignores DC bin 0).
function peak(freqs, mag) {
  let bi = 1;
  for (let k = 2; k < mag.length; k++) if (mag[k] > mag[bi]) bi = k;
  return { bin: bi, freq: freqs[bi], amp: mag[bi] };
}

module.exports = { nextPow2, hann, fft, spectrum, peak };
