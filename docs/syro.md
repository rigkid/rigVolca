# Syro transfer

The volca sample does not take SysEx dumps. Sample data is encoded as an audio stream (Korg **Syro**) and played into the unit.

## Wiring

**volca sample (original) and sample2 over a cable**

1. Computer headphone / line out to a 3.5 mm cable to volca **SYNC IN**.
2. Set the computer output volume to **full**. Syro is a data stream; a low level fails the decode.
3. On the volca, wait for the [SYRO] receive screen (FUNC + record, or just play the stream — the unit detects it).
4. Do not run the stream through mixers, FX, or the RigKit playback mixer. Use `SyroPlayer`, which opens the device at 16-bit / 44.1 kHz / stereo with no gain.

**volca sample2 over USB**

The sample2 enumerates as a USB audio output. Pick that device in SampleManager's Send window (or `SyroPlayer::deviceNames()`). The same Syro stream is played into it.

## What we send

1. Downmix to mono, resample to **31.25 kHz**, convert to 16-bit (`prepareSample`).
2. Compress-encode with Syro quality 16 into a **44.1 kHz stereo** stream (`encodeSend`).
3. Play the stream (`SyroPlayer::play`). Next send is blocked until `progress()` reaches 1.

Erase a slot with `encodeErase(slot)` and the same player.

## Capacity

Both models have **4 MiB** of sample FLASH (~65 seconds of 16-bit mono at 31.25 kHz).

| Model | Slots | Memory |
|--|--|--|
| volca sample | 100 | 4 MiB |
| volca sample2 | 200 | 4 MiB |

`Capacity::canFit(sample)` is the gate before advancing to the next slot. If a clip is longer than remaining memory, the overflow byte count is returned so the UI can refuse or offer a trim.

## Troubleshooting

- **volca does not enter receive** — volume too low, cable in SYNC OUT, or the stream went through a mixer with FX/gain.
- **Partial load / error on the unit** — another app stole the device; don't play other audio during a send.
- **USB device missing** — sample2 only; original volca sample is cable-only. Rescan devices after plugging in.
- **Slot ignored on original** — slots 100–199 are sample2 only.
