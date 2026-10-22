/* SPDX-License-Identifier: GPL-3.0-only */
#include "azmod-mod.h"
#include "vdj_monitor.h"
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#define ID "vdj-stems-import"
static const az_core_v1 *C;
static atomic_int enabled_flag=1;
static atomic_uint generation;
/* One atomic publication prevents mixed counts/status across threads.
 * 20 bits per count is larger than 8 volumes * 50,000 bounded entries. */
static atomic_uint_fast64_t display;
static int selected;
static az_rect toggle_hit, rescan_hit;
static long now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (long)t.tv_sec*1000+t.tv_nsec/1000000;
}
static void publish(const struct vdj_status *s,void *user) {
    (void)user;
    uint64_t word=s->state | ((uint64_t)s->volumes<<4) |
        ((uint64_t)s->complete<<8) | ((uint64_t)s->incomplete<<28);
    atomic_store(&display,word);
}
static int read_mounts(struct vdj_mounts *mounts) {
    FILE *f=fopen("/proc/self/mountinfo","re");
    if (!f) return -1;
    int r=vdj_mounts_read(f,mounts); fclose(f); return r;
}
struct scan_context { struct vdj_mounts expected; unsigned generation; long checked; int stale; };
static int cancel_scan(void *user) {
    struct scan_context *s=user;
    if (!atomic_load(&enabled_flag) || atomic_load(&generation)!=s->generation) return 1;
    if (s->stale) return 1;
    long now=now_ms();
    if (!s->checked || now-s->checked>=250) {
        struct vdj_mounts actual;
        s->stale=read_mounts(&actual)!=0 || !vdj_mounts_equal(&actual,&s->expected);
        s->checked=now;
    }
    return s->stale;
}
static int scan_volume(const char *contents,vdj_set_fn emit,vdj_cancel_fn cancel,void *user,
                       struct vdj_scan_result *result) {
    struct stat st;
    memset(result,0,sizeof *result);
    /* A mounted USB with no Contents folder is a legitimate empty inventory. */
    int absent=lstat(contents,&st)!=0 && errno==ENOENT;
    int rc=absent ? VDJ_SCAN_OK : vdj_scan_contents(contents,emit,cancel,user,result);
    /* Force identity validation after each volume, even for very short scans. */
    ((struct scan_context *)user)->checked=0;
    if (cancel(user)) return VDJ_SCAN_CANCELLED;
    return rc;
}
static void *worker(void *arg) {
    (void)arg;
    struct vdj_monitor monitor={0};
    long retry_at=0;
    for (;;) {
        struct scan_context context={0};
        context.generation=atomic_load(&generation);
        int on=atomic_load(&enabled_flag);
        int ok=on ? read_mounts(&context.expected)==0 : 1;
        if (retry_at && now_ms()>=retry_at) { monitor.valid=0; retry_at=0; }
        vdj_monitor_poll(&monitor,ok ? &context.expected : NULL,on,context.generation,
                         scan_volume,cancel_scan,publish,&context);
        unsigned state=(unsigned)atomic_load(&display)&15u;
        if (state==VDJ_PARTIAL && !retry_at) retry_at=now_ms()+10000;
        if (state!=VDJ_PARTIAL) retry_at=0;
        struct timespec delay={1,0}; nanosleep(&delay,NULL);
    }
    return NULL;
}
static void line(az_draw *d,az_rect area,int row,int index,const char *text) {
    if (row<=0 || (index+1)*row>area.h || area.w<=16) return;
    int px=row/2; if (px<8) px=8; if (px>96) px=96;
    char out[128]; snprintf(out,sizeof out,"%s",text);
    size_t n=strlen(out);
    while (n && C->text_width(out,px)>area.w-16) out[--n]=0;
    if (n) C->draw_text(d,area.x+8,area.y+index*row,out,px,C->color(AZ_COL_TEXT));
}
static void draw(az_draw *d,az_rect area,void *user) {
    (void)user;
    static const char *labels[]={"Waiting for USB","Scan disabled","Scanning VDJ files...",
        "Scan complete","Partial scan: retrying","Cannot read USB mounts","USB changed / scan cancelled"};
    C->draw_header(d,"VirtualDJ Stem Scan");
    int row=C->row_height(d);
    toggle_hit=(az_rect){0}; rescan_hit=(az_rect){0};
    uint64_t word=atomic_load(&display);
    unsigned state=word&15u,volumes=(word>>4)&15u;
    unsigned complete=(word>>8)&0xfffffu,incomplete=(word>>28)&0xfffffu;
    if (state>VDJ_CANCELLED) state=VDJ_FAILED;
    char text[128];
    snprintf(text,sizeof text,"%s | %u complete, %u incomplete",labels[state],complete,incomplete);
    line(d,area,row,1,text);
    snprintf(text,sizeof text,"%s Auto scan: %s",selected==0 ? ">":" ",atomic_load(&enabled_flag) ? "ON":"OFF");
    line(d,area,row,2,text);
    line(d,area,row,3,selected==1 ? "> Scan again":"  Scan again");
    snprintf(text,sizeof text,"%u USB volumes | %u complete | %u incomplete",volumes,complete,incomplete);
    line(d,area,row,4,text);
    line(d,area,row,5,"Files only: M4A playback support pending");
    if (row>0 && area.h>=3*row) toggle_hit=(az_rect){area.x,area.y+2*row,area.w,row};
    if (row>0 && area.h>=4*row) rescan_hit=(az_rect){area.x,area.y+3*row,area.w,row};
}
static int contains(az_rect r,int x,int y) { return r.w>0 && r.h>0 && x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h; }
static int input(const az_event *ev,void *user) {
    (void)user;
    if (ev->kind==AZ_EV_KNOB) { selected=ev->dir>0 ? 1:0; return 1; }
    if (ev->kind==AZ_EV_TAP) {
        if (contains(toggle_hit,ev->x,ev->y)) selected=0;
        else if (contains(rescan_hit,ev->x,ev->y)) selected=1;
        else return 0;
    } else if (ev->kind!=AZ_EV_CLICK) return 0;
    if (!selected) {
        int value=!atomic_load(&enabled_flag);
        atomic_store(&enabled_flag,value); C->cfg_set(ID,"auto-import",value);
    }
    atomic_fetch_add(&generation,1); /* cancels old scan, then schedules a new one */
    return 1;
}
static const az_screen_v1 screen={.size=sizeof(az_screen_v1),.on_draw=draw,.on_input=input};
static void tick(void) {
    static uint64_t seen=UINT64_MAX;
    static int seen_selected=-1,seen_enabled=-1;
    static long last_ms=-250;
    uint64_t word=atomic_load(&display); int on=atomic_load(&enabled_flag);
    long ms=now_ms();
    if ((word!=seen || selected!=seen_selected || on!=seen_enabled) && ms-last_ms>=250) {
        seen=word; seen_selected=selected; seen_enabled=on; last_ms=ms; C->request_redraw();
    }
}
static int init(const az_core_v1 *core) {
    if (!core || core->abi!=AZ_MOD_ABI) return 1;
    C=core;
    if (!AZ_HAS(C,thread_start) || !AZ_HAS(C,register_screen) ||
        !AZ_HAS(C,request_redraw) || !AZ_HAS(C,text_width)) return 1;
    atomic_store(&enabled_flag,C->cfg_get(ID,"auto-import",1)!=0);
    if (C->register_screen(ID,&screen)) return 1;
    return C->thread_start(ID,"vdj-usb-scan",worker,NULL)!=0;
}
static const char *const firmware[]={"any",NULL};
static const az_mod_v1 mod={.abi=AZ_MOD_ABI,.size=sizeof(az_mod_v1),.id=ID,.fw_tags=firmware,.init=init,.tick=tick};
const az_mod_v1 *az_mod_v1_entry(void) { return &mod; }
