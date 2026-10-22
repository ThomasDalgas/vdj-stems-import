/* SPDX-License-Identifier: GPL-3.0-only */
#include "usb_watch.h"
#include <string.h>
#include <stdlib.h>
static int usb_path(const char *p) {
    const char *prefix="/media/usb/sd";
    size_t n=strlen(prefix);
    if (strncmp(p,prefix,n) || p[n]<'a' || p[n]>'z') return 0;
    p+=n+1;
    while (*p>='0' && *p<='9') p++;
    return !*p;
}
static int cmp(const void *a,const void *b) {
    const struct vdj_mount *x=a,*y=b;
    int c=strcmp(x->path,y->path);
    return c ? c : x->id > y->id ? 1 : x->id < y->id ? -1 : 0;
}
int vdj_mounts_read(FILE *f,struct vdj_mounts *out) {
    memset(out,0,sizeof *out);
    char line[8192];
    while (fgets(line,sizeof line,f)) {
        if (!strchr(line,'\n') && !feof(f)) return -1;
        struct vdj_mount m={0}; unsigned parent;
        char root[256];
        if (sscanf(line,"%u %u %u:%u %255s %255s",&m.id,&parent,&m.major,&m.minor,root,m.path)!=6) return -1;
        if (!usb_path(m.path)) continue;
        if (!m.id || !strstr(line," - ") || out->count==VDJ_MAX_MOUNTS) return -1;
        /* Stacked mounts at one pathname cannot be attributed safely. */
        for (unsigned i=0;i<out->count;i++)
            if (!strcmp(out->items[i].path,m.path)) return -1;
        out->items[out->count++]=m;
    }
    if (ferror(f)) return -1;
    qsort(out->items,out->count,sizeof out->items[0],cmp);
    return 0;
}
int vdj_mounts_equal(const struct vdj_mounts *a,const struct vdj_mounts *b) {
    if (a->count!=b->count) return 0;
    for (unsigned i=0;i<a->count;i++) {
        const struct vdj_mount *x=&a->items[i],*y=&b->items[i];
        if (x->id!=y->id || x->major!=y->major || x->minor!=y->minor || strcmp(x->path,y->path)) return 0;
    }
    return 1;
}
