/* SPDX-License-Identifier: GPL-3.0-only */
#include "vdj_scan.h"
#include <stdio.h>
static void json_string(const char *p) {
    putchar('"');
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 32) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}
static void report(const struct vdj_set *s, void *user) {
    (void)user;
    printf("{\"type\":\"set\",\"original\":"); json_string(s->original);
    printf(",\"original_exists\":%s,\"present_mask\":%u,\"invalid_mask\":%u,\"duplicate_mask\":%u,\"files_complete\":%s,\"playback_ready\":false}\n",
        s->original_ok ? "true":"false", s->present, s->invalid, s->duplicate,
        s->complete ? "true":"false");
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: vdj-scan /path/to/USB/Contents\n"); return 64; }
    puts("{\"type\":\"schema\",\"version\":2,\"groups\":[\"Vocals\",\"Instruments\",\"Bass\",\"Drums\"],\"azmod_slots\":[\"VOCAL\",\"MELODY\",\"BASS\",\"DRUMS\"],\"codec_verified\":false}");
    struct vdj_scan_result r;
    int status=vdj_scan_contents(argv[1], report, NULL, NULL, &r);
    printf("{\"type\":\"summary\",\"scan_status\":%d,\"sets\":%u,\"complete\":%u,\"incomplete\":%u,\"errors\":%u}\n",status,r.sets,r.complete,r.incomplete,r.errors);
    if (status) return status;
    return r.incomplete ? 4 : 0;
}
