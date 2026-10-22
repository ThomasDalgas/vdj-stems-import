/* SPDX-License-Identifier: GPL-3.0-only */
#include "vdj_monitor.h"
#include <stdio.h>
void vdj_monitor_poll(struct vdj_monitor *m,const struct vdj_mounts *mounts,int enabled,
    unsigned generation,vdj_scan_fn scan,vdj_cancel_fn cancel,vdj_status_fn publish,void *user) {
    struct vdj_status s={0};
    if (!enabled) { m->valid=0; s.state=VDJ_DISABLED; publish(&s,user); return; }
    if (!mounts) { m->valid=0; s.state=VDJ_FAILED; publish(&s,user); return; }
    if (m->valid && generation==m->generation && vdj_mounts_equal(mounts,&m->previous)) return;
    m->previous=*mounts; m->generation=generation; m->valid=1;
    s.volumes=mounts->count;
    if (!mounts->count) { publish(&s,user); return; }
    s.state=VDJ_SCANNING; publish(&s,user);
    int partial=0;
    for (unsigned i=0;i<mounts->count;i++) {
        if (cancel && cancel(user)) goto cancelled;
        char path[320];
        snprintf(path,sizeof path,"%s/Contents",mounts->items[i].path);
        struct vdj_scan_result result={0};
        int status=scan(path,NULL,cancel,user,&result);
        if (status==VDJ_SCAN_CANCELLED || (cancel && cancel(user))) goto cancelled;
        s.complete+=result.complete; s.incomplete+=result.incomplete;
        if (status!=VDJ_SCAN_OK || result.errors) partial=1;
        publish(&s,user); /* counts only from finished volume snapshots */
    }
    s.state=partial ? VDJ_PARTIAL : VDJ_DONE;
    publish(&s,user);
    return;
cancelled:
    m->valid=0;
    s=(struct vdj_status){.state=VDJ_CANCELLED};
    publish(&s,user);
}
