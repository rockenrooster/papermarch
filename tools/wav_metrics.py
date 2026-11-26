"""wav_metrics.py: the numbers of a 16-bit WAV render (TECH_SPEC 6, P4 exit checks).

  python -I -B tools/wav_metrics.py a.wav [b.wav ...] [--ladder ladder.wav] [--table out.md]

Per file: sample peak, true peak and integrated loudness (ffmpeg ebur128), RMS, DC offset, clipped samples, the share
of silence (10 ms windows under -60 dBFS), the largest sample-to-sample jump, and the share of energy above 300 Hz
(a phone speaker has no bass). --ladder checks the eleven ladder notes' fundamentals against C major pentatonic.
"""
import re
import subprocess
import sys
import wave

import numpy as np

LADDER_MIDI = [60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84]


def read(path):
    with wave.open(path, "rb") as w:
        n, ch, rate = w.getnframes(), w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64).reshape(-1, ch)
    return x / 32768.0, rate, x


def ebur128(path):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-af", "ebur128=peak=true", "-f", "null", "-"],
                       capture_output=True, text=True)
    text = r.stderr
    summary = text[text.rfind("Summary:"):] if "Summary:" in text else text
    lufs = re.search(r"I:\s+(-?[\d.]+|-inf) LUFS", summary)
    tp = re.search(r"Peak:\s+(-?[\d.]+|-inf) dBFS", summary)
    return (float(lufs.group(1)) if lufs else float("nan"), float(tp.group(1)) if tp else float("nan"))


def db(v):
    return 20.0 * np.log10(max(v, 1e-12))


def metrics(path):
    x, rate, raw = read(path)
    mono = x.mean(axis=1)
    win = rate // 100
    nwin = len(mono) // win
    rms_w = np.sqrt((mono[: nwin * win].reshape(nwin, win) ** 2).mean(axis=1)) if nwin else np.zeros(1)
    spec = np.abs(np.fft.rfft(mono)) ** 2
    freqs = np.fft.rfftfreq(len(mono), 1.0 / rate)
    lufs, tp = ebur128(path)
    return {
        "file": path.replace("\\", "/").split("/")[-1],
        "seconds": len(mono) / rate,
        "peak": db(np.abs(x).max()),
        "true_peak": tp,
        "lufs": lufs,
        "rms": db(np.sqrt((x ** 2).mean())),
        "dc": float(abs(x.mean(axis=0)).max()),
        "clipped": int((np.abs(raw) >= 32767).sum()),
        "silence": float((rms_w < 10 ** (-60 / 20)).mean()),
        "jump": float(np.abs(np.diff(x, axis=0)).max()),
        "above300": float(spec[freqs >= 300].sum() / max(spec.sum(), 1e-30)),
    }


def ladder(path):
    """Each note sits in its own half-second window from 0.1 s; the fundamental is the spectral peak near the note."""
    x, rate, _ = read(path)
    mono = x.mean(axis=1)
    rows = []
    for k, midi in enumerate(LADDER_MIDI):
        a = int((0.1 + 0.5 * k) * rate)
        seg = mono[a: a + int(0.45 * rate)]
        seg = seg * np.hanning(len(seg))
        n = 1 << 20  # zero-padded: 0.05 Hz bins
        spec = np.abs(np.fft.rfft(seg, n))
        freqs = np.fft.rfftfreq(n, 1.0 / rate)
        want = 440.0 * 2 ** ((midi - 69) / 12.0)
        band = (freqs > want * 0.94) & (freqs < want * 1.06)
        i = np.flatnonzero(band)[np.argmax(spec[band])]
        # Parabolic interpolation on the log magnitude.
        y0, y1, y2 = np.log(spec[i - 1: i + 2] + 1e-30)
        d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)
        got = (i + d) * rate / n
        cents = 1200 * np.log2(got / want)
        rows.append((midi, want, got, cents))
    return rows


def main(argv):
    files, ladder_file, table = [], None, None
    i = 0
    while i < len(argv):
        if argv[i] == "--ladder":
            ladder_file = argv[i + 1]
            i += 2
        elif argv[i] == "--table":
            table = argv[i + 1]
            i += 2
        else:
            files.append(argv[i])
            i += 1
    lines = ["| File | Seconds | Sample peak | True peak | Loudness | RMS | DC | Clipped | Silence | Largest jump | Energy > 300 Hz |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    for f in files:
        m = metrics(f)
        lines.append(f"| {m['file']} | {m['seconds']:.1f} | {m['peak']:.2f} dBFS | {m['true_peak']:.2f} dBTP | {m['lufs']:.1f} LUFS | "
                     f"{m['rms']:.1f} dBFS | {m['dc']:.5f} | {m['clipped']} | {100 * m['silence']:.1f}% | {m['jump']:.3f} | "
                     f"{100 * m['above300']:.0f}% |")
    ok = True
    if ladder_file:
        lines += ["", "| Note | Wanted Hz | Measured Hz | Cents |", "|---|---|---|---|"]
        for midi, want, got, cents in ladder(ladder_file):
            names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
            lines.append(f"| {names[midi % 12]}{midi // 12 - 1} | {want:.2f} | {got:.2f} | {cents:+.2f} |")
            ok = ok and abs(cents) <= 5.0
        lines.append("")
        lines.append(f"Ladder: every fundamental within 5 cents: {'yes' if ok else 'NO'}")
    text = "\n".join(lines)
    print(text)
    if table:
        with open(table, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
