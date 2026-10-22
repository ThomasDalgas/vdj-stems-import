#include "vdj_monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture { int calls, cancel, error, scanning, remove_during; struct vdj_status latest; };
static int cancelled(void *u) { return ((struct fixture *)u)->cancel; }
static void publish(const struct vdj_status *s,void *u) {
 struct fixture *f=u; f->latest=*s; if (s->state==VDJ_SCANNING) f->scanning++;
}
static int scan(const char *path,vdj_set_fn emit,vdj_cancel_fn cancel,void *u,struct vdj_scan_result *r) {
 (void)emit;(void)cancel;
 struct fixture *f=u;f->calls++;assert(strstr(path,"/Contents"));
 *r=(struct vdj_scan_result){.sets=3,.complete=2,.incomplete=1};
 if (f->remove_during) f->cancel=1;
 return f->error ? VDJ_SCAN_ERROR : VDJ_SCAN_OK;
}
static void poll(struct vdj_monitor *m,const struct vdj_mounts *v,int enabled,unsigned gen,struct fixture *f) {
 vdj_monitor_poll(m,v,enabled,gen,scan,cancelled,publish,f);
}
int main(void) {
 struct vdj_monitor m={0};struct fixture f={0};struct vdj_mounts v={0};
 poll(&m,&v,1,0,&f);assert(f.latest.state==VDJ_WAITING && f.calls==0);
 v.count=1;v.items[0]=(struct vdj_mount){.id=31,.major=8,.minor=1};strcpy(v.items[0].path,"/media/usb/sda1");
 poll(&m,&v,1,0,&f);assert(f.calls==1 && f.scanning && f.latest.state==VDJ_DONE && f.latest.complete==2 && f.latest.incomplete==1);
 poll(&m,&v,1,0,&f);assert(f.calls==1); /* no repeated full scan */
 poll(&m,&v,1,1,&f);assert(f.calls==2); /* manual rescan */
 v.items[0].id=32;poll(&m,&v,1,1,&f);assert(f.calls==3); /* same-path USB replacement */
 v.count=2;v.items[1]=v.items[0];v.items[1].id=33;strcpy(v.items[1].path,"/media/usb/sdb1");
 poll(&m,&v,1,1,&f);assert(f.calls==5 && f.latest.complete==4 && f.latest.volumes==2);
 poll(&m,&v,0,1,&f);assert(f.latest.state==VDJ_DISABLED && !f.latest.complete);
 poll(&m,&v,1,1,&f);assert(f.calls==7); /* enable rescans */
 f.cancel=1;poll(&m,&v,1,2,&f);assert(f.latest.state==VDJ_CANCELLED && !f.latest.complete && !m.valid);
 f.cancel=0;f.remove_during=1;poll(&m,&v,1,3,&f);assert(f.latest.state==VDJ_CANCELLED && !f.latest.complete && !m.valid);
 f.remove_during=0;f.cancel=0;f.error=1;poll(&m,&v,1,2,&f);assert(f.latest.state==VDJ_PARTIAL);
 poll(&m,NULL,1,2,&f);assert(f.latest.state==VDJ_FAILED && !f.latest.complete && !m.valid);
 v.count=0;poll(&m,&v,1,2,&f);assert(f.latest.state==VDJ_WAITING && !f.latest.complete);
 puts("Monitor insertion/removal, remount, two volumes, manual rescan, disable, cancellation and errors passed");
}
