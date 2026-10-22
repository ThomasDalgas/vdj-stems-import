# Read-only VirtualDJ inventory

Build and inspect an export (run from project root):

```sh
make scan-tool
out-host/vdj-scan /Volumes/BOOT/Contents > out-host/usb-inventory.jsonl
make scan-test
```

The executable shares `vdj_scan.c` with future on-deck integration. It has no codec
or external library dependency. It does not modify files, follow child symlinks,
or traverse mounted child filesystems. `Contents` must be a trusted absolute path
chosen by the caller: parent components of this input path are resolved by the OS.
Depth is limited to 8 nested directories and work to 50,000 directory entries.
The callback interface permits cooperative cancellation between directory entries;
it cannot interrupt an OS filesystem call that is already blocked.

JSON Lines output: schema, one record per discovered `.stems` directory, summary.
Paths are relative to Contents. Group bits are 1 Acapella, 2 Instrumental,
4 Melody, 8 Beat. Groups are matched case-insensitively. Apple `._` sidecars and
hidden directories are ignored. Duplicate names, missing originals, empty files
and non-regular expected files prevent `files_complete`. No guess based on title
is used: the original is the sibling obtained by removing the `.stems` suffix.

`files_complete` means only that all five nonempty regular files exist. The tool
DOES NOT decode or verify codecs, audio integrity, timing, or group semantics.
`playback_ready` is always false until an actual supported playback path exists.
Exit status: 0 scan complete/all sets complete (also empty), 1 filesystem error,
2 bounded work limit, 3 cancelled (C API), 4 scan finished with incomplete sets.
Always check the summary: errors/limits can produce partial inventory.

A scan is a point-in-time snapshot. Before any future import/write, reopen files
and recheck USB identity, original identity, codec, durations and group overlap.
The SDK mod now invokes this inventory engine from its background USB monitor.
It checks Linux mount IDs, supports manual rescan and cancels on disable or USB
identity changes. Counts are aggregated on the mod's own on-deck page. Host tests
cover the integration; hardware testing remains necessary.

No audio fixture is committed. Tests use small synthetic files because they test
inventory semantics, not playback. Actual USB codec evidence is documented in the
workspace findings from 2026-10-22.
