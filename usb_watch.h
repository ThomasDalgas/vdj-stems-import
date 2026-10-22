/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdio.h>
#define VDJ_MAX_MOUNTS 8
struct vdj_mount { unsigned id, major, minor; char path[256]; };
struct vdj_mounts { unsigned count; struct vdj_mount items[VDJ_MAX_MOUNTS]; };
/* Linux /proc/self/mountinfo. Mount ID distinguishes remounts at the same path.
 * Returns -1 on read error, malformed USB entry or overflow: never truncate silently. */
int vdj_mounts_read(FILE *file, struct vdj_mounts *out);
int vdj_mounts_equal(const struct vdj_mounts *a, const struct vdj_mounts *b);
