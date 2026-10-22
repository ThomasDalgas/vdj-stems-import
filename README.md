# VirtualDJ Stems Import — 0.3.0 development preview

SDK-based XDJ-AZ community mod. Automatically scans VirtualDJ exports on mounted
USB media and displays the results on its own page. No conversion or audio/library
file writes: only the user's on/off preference is persisted through SDK settings.

## On-deck behaviour

The background worker reads `/proc/self/mountinfo` once per second and identifies
actual `/media/usb/sd[a-z][partition]` mounts. It scans each volume's `Contents`
folder on initial startup, USB insertion or mount-identity change, re-enable, or
manual rescan. Stable mounts are not repeatedly scanned. Partial scans retry
at ten-second intervals. Up to eight volumes are supported; overflow is an error.

Open VirtualDJ Stems Import in Community Mods. The page shows scanning/waiting/
completed/partial/error status and complete/incomplete file-set counts. Use the
browse knob and click, or touch the rows, to toggle Auto scan or select Scan again.
A USB without Contents yields an empty inventory. Counts aggregate all detected
USB volumes and are cleared on disable, loss of mount information or cancellation.

The worker checks cancellation between entries and mount identity every 250 ms
while scanning, plus after each volume. USB changes invalidate results. Cancellation
cannot interrupt a filesystem call already blocked in the OS. There is one worker,
no UI-thread filesystem access and no unbounded per-track cache. One atomic word
publishes status/counts consistently. The mount layout still needs hardware testing.

## What complete means

A nonempty regular original file and four nonempty regular M4A files were found.
It does NOT establish valid codecs, correct timing, independent stem groups or
AZ-MOD playback readiness. The UI explicitly says M4A playback support is pending.
Scans never copy, modify, rename or delete USB files. No stems are added to AZ-MOD.

The observed sample export is AAC-LC stereo 44.1 kHz. Instrumental overlaps Melody
and Beat; the new export configuration below resolves that overlap in principle; the new
audio files still require verification. M4A playback support awaits upstream guidance. Direct M4A
reuse is preferred. If conversion becomes necessary, use a separate SDK-launched
process with bounded resources, not codec work inside EP147's UI/audio callbacks.

## Development

SDK: `../az-mods-sdk` (revision 3dcaf8f7331ea94135d7161f653edfaf906d5ea6).
Keep generated files here under Projects, outside iCloud.

```sh
make monitor-test    # scanner fixtures, cancellation, mount parsing and lifecycle
make test            # SDK harness: plain, ASan/UBSan, TSan
make scan-tool
out-host/vdj-scan /Volumes/BOOT/Contents
../az-mods-sdk/tools/azmod-pack build .
../az-mods-sdk/tools/azmod-pack package .
```

ARM64 package build requires the SDK Docker toolchain. Host checks do not prove
hardware compatibility. The SDK template icon remains a placeholder for this test package. No package has been installed on the deck.
See [tools/README.md](tools/README.md) for inventory schema and limits.

GPL-3.0-only. Based on Kyle-Hosman/az-mods-sdk template.

## Required next feature: hide VirtualDJ stem entries

When a VirtualDJ export is detected, native playlists should show the original
track only. Acapella, Instrumental, Melody and Beat entries belong in the background
for future stem playback. This is NOT implemented in 0.3.0: this version is a
read-only scanner test package and playlists still show all five entries.

The current public SDK can enumerate playlists and submit INSERT/UPDATE library
spools but has no native track-visibility filter or documented hidden-track flag.
A supported core/SDK visibility hook is preferable to rewriting playlist membership.
Ask upstream for reversible filtering by exact track identity/path, retaining audio
files, library metadata and ordering, and restoring visibility when disabled.
Match only validated `<original>.stems/{Vocals,Instruments,Bass,Drums}.m4a`
relationships; never hide arbitrary files merely because their titles contain
these words. All Tracks/search behaviour must be specified separately from playlists.

## Test installation

See the SDK developer guide for Community Mods USB installation. Package contents
belong under `MODS/community-mods/vdj-stems-import/`; allow the new mod on the deck
and reload when no tracks are loaded. AZ-MODS core 1.2.3 or newer is required.
This is a host-tested beta; on-device validation is still outstanding.

## Four-group export configuration (0.3.0)

Set VirtualDJ `cdjExportStemsConfig` to:

```
Vocals=vocals, Instruments=instru, Bass=bass, Drums=kick+hihat
```

Select all four groups for the new stem test export. Scanner mapping:

| File | AZ-MOD slot | Mask bit |
| --- | --- | --- |
| Vocals.m4a | VOCAL (0) | 1 |
| Instruments.m4a | MELODY (1) | 2 |
| Bass.m4a | BASS (2) | 4 |
| Drums.m4a | DRUMS (3) | 8 |

Matching is case-insensitive but otherwise exact. Legacy Acapella/Instrumental/
Melody/Beat sets remain incomplete; Instrumental is deliberately not an alias for
Instruments because it contains overlapping audio. No audio is decoded or injected
into AZ-MOD: this is inventory mapping, and actual M4A playback remains pending.
CLI JSON schema is now version 2 and includes parallel groups/azmod_slots arrays.

## Readability update (0.3.1)

Larger result counts, separate status and controls, theme-aware selected buttons,
and touch targets matching the visible buttons. Text fits the available panel.
The v0.3.0 scanner was confirmed on an XDJ-AZ with seven complete sets; the new
layout still needs on-device visual confirmation.

When creating `MODS/azmods.cfg`, include the required settings header:

```ini
version = 1
community-mods = on
community-mods.vdj-stems-import = on
```

Preserve any existing settings when updating this file.

## Icon update (0.3.2)

Adds the approved four-waveform import icon. Scanner and layout are unchanged from
0.3.1. This replaces the SDK placeholder so automatic release packaging can pass.
