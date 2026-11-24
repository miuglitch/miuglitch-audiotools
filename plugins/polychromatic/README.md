# Polychromatic

Pulls the energy of every FFT bin onto the nearest target note and re-emits it
as phase-continuous, ringing partials. Built for soundscapes and sparkles, so
transients are not preserved on purpose.

## Controls

| Control | What it does |
| --- | --- |
| Amount | 0 % is a bit-exact null (input delayed by the reported latency). Turning it up narrows a spectral sieve around the targets and crossfades (equal power) into resynthesised partials. 100 % is fully tonal. |
| Sustain | How long partials keep ringing after the input energy drops (about 50 ms up to about 12 s). |
| Root / Scale | Targets when no MIDI is steering: every Ableton Live scale and mode, plus Chromatic and Custom. |
| Custom | Opens a two-octave keyboard. Click or drag keys to include notes. The 24-note pattern is absolute (C-anchored) and repeats every two octaves, so the low and high octave can differ. "Copy scale" fills it from the last scale + root. |
| Targets from | Auto = held MIDI notes when there are any, otherwise the scale. Scale or MIDI force one source. |
| MIDI notes as | Harmonic Series (each note's first N harmonics, see Harmonics) or All Octaves (the note's pitch class across the range). |
| Latch | Keep the last chord after release; a new chord replaces it. Sustain pedal (CC64) also holds notes. |
| MPE | Lower zone: channel 1 is master (Bend range), channels 2-16 carry per-note pitch bend (MPE bend range), pressure (louder partials) and CC74 (brightness tilt). |
| FFT | 4096 (default, about 85 ms at 48 kHz / 93 ms at 44.1 kHz) or 2048 (half the latency, coarser low end). Latency is reported to the host and updates when switched. |

All MIDI notes 0-127 work, including bends past the ends of the range.

## Build

Polychromatic builds with the rest of the repo; see the root README.
To run its offline DSP checks:

```
cmake -S . -B build -DMIUGLITCH_BUILD_TESTS=ON -DMIUGLITCH_BUILD_PLUGINS=OFF
cmake --build build --config Release
ctest --test-dir build -C Release
```

In Ableton Live, MIDI reaches an audio effect by setting a MIDI track's
"MIDI To" to the track holding the plugin, then choosing "Polychromatic".
