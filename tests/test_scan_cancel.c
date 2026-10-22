/* SPDX-License-Identifier: GPL-3.0-only */
#include "vdj_scan.h"
#include <assert.h>
#include <stdio.h>
static int cancel(void *user) { (*(int *)user)++; return 1; }
static void unexpected(const struct vdj_set *s, void *u) { (void)s; (void)u; assert(0); }
int main(int argc, char **argv) {
    assert(argc==2); int calls=0; struct vdj_scan_result r;
    assert(vdj_scan_contents(argv[1],unexpected,cancel,&calls,&r)==VDJ_SCAN_CANCELLED);
    assert(calls==1 && r.sets==0 && r.entries==0);
    assert(vdj_scan_contents(NULL,NULL,NULL,NULL,&r)==VDJ_SCAN_ERROR);
    assert(r.errors==1);
    puts("Cancellation and invalid argument tests passed");
}
