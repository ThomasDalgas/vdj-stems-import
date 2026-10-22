/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "usb_watch.h"
#include "vdj_scan.h"
enum { VDJ_WAITING, VDJ_DISABLED, VDJ_SCANNING, VDJ_DONE, VDJ_PARTIAL, VDJ_FAILED, VDJ_CANCELLED };
struct vdj_status { unsigned state, volumes, complete, incomplete; };
struct vdj_monitor { int valid; unsigned generation; struct vdj_mounts previous; };
typedef void (*vdj_status_fn)(const struct vdj_status *,void *);
typedef int (*vdj_scan_fn)(const char *,vdj_set_fn,vdj_cancel_fn,void *,struct vdj_scan_result *);
/* Worker-only state. scan injection allows lifecycle testing without a deck.
 * cancelled must check enabled/generation AND USB identity. */
void vdj_monitor_poll(struct vdj_monitor *,const struct vdj_mounts *,int enabled,
    unsigned generation,vdj_scan_fn,vdj_cancel_fn,vdj_status_fn,void *);
