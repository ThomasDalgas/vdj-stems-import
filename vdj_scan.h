/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef VDJ_SCAN_H
#define VDJ_SCAN_H
#include <stddef.h>
/* Inventory only: complete means files exist, NOT audio/playback compatibility.
 * Call only from a worker. Does no writes, decoding or large allocations.
 * Callback data is valid only during callback. Paths are relative to Contents.
 * The caller must not use this snapshot as authorization to write to a USB. */
enum { VDJ_ACAPELLA, VDJ_INSTRUMENTAL, VDJ_MELODY, VDJ_BEAT, VDJ_GROUPS };
enum { VDJ_SCAN_OK, VDJ_SCAN_ERROR, VDJ_SCAN_LIMIT, VDJ_SCAN_CANCELLED };
#define VDJ_PATH_CAP 2048
#define VDJ_SCAN_MAX_ENTRIES 50000u
#define VDJ_SCAN_MAX_DEPTH 8u
struct vdj_set {
    char original[VDJ_PATH_CAP];
    unsigned present; /* nonempty regular source files; bit per VDJ_GROUPS */
    unsigned invalid; /* empty/nonregular/inaccessible expected file */
    unsigned duplicate; /* case-insensitive group name collision */
    int original_ok;
    int complete;
};
struct vdj_scan_result { unsigned sets, complete, incomplete, errors, entries; };
typedef int (*vdj_cancel_fn)(void *user);
typedef void (*vdj_set_fn)(const struct vdj_set *set, void *user);
int vdj_scan_contents(const char *contents, vdj_set_fn on_set,
                      vdj_cancel_fn cancelled, void *user,
                      struct vdj_scan_result *result);
#endif
