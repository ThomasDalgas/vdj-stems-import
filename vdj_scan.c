/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "vdj_scan.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

struct scan {
    vdj_set_fn emit; vdj_cancel_fn cancel; void *user;
    struct vdj_scan_result *result; int status;
};
static int stopped(struct scan *s) {
    if (s->status) return 1;
    if (s->cancel && s->cancel(s->user)) s->status = VDJ_SCAN_CANCELLED;
    return s->status != 0;
}
static void error(struct scan *s) { s->result->errors++; }
static int budget(struct scan *s) {
    if (stopped(s)) return 0;
    if (s->result->entries >= VDJ_SCAN_MAX_ENTRIES) {
        s->status = VDJ_SCAN_LIMIT; return 0;
    }
    s->result->entries++; return 1;
}
static int nonempty_file(int fd, const char *name) {
    struct stat st;
    return fstatat(fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0 &&
           S_ISREG(st.st_mode) && st.st_size > 0;
}
static void inspect_set(struct scan *s, int parent, const char *name, const char *rel) {
    struct vdj_set set = {0};
    size_t n = strlen(rel);
    if (n < 6 || n >= sizeof set.original) { error(s); return; }
    memcpy(set.original, rel, n - 6); /* remove .stems */
    char original[VDJ_PATH_CAP];
    size_t len = strlen(name);
    memcpy(original, name, len - 6); original[len - 6] = 0;
    set.original_ok = nonempty_file(parent, original);
    int fd = openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { error(s); return; }
    DIR *dir = fdopendir(fd);
    if (!dir) { close(fd); error(s); return; }
    const char *names[] = {"Vocals.m4a", "Instruments.m4a", "Bass.m4a", "Drums.m4a"};
    unsigned seen = 0;
    unsigned errors_before = s->result->errors;
    for (;;) {
        if (stopped(s)) break;
        errno = 0; struct dirent *e = readdir(dir);
        if (!e) { if (errno) error(s); break; }
        if (!budget(s)) break;
        if (e->d_name[0] == '.') continue;
        for (unsigned i = 0; i < VDJ_GROUPS; i++) {
            if (strcasecmp(e->d_name, names[i])) continue;
            unsigned bit = 1u << i;
            if (seen & bit) set.duplicate |= bit;
            seen |= bit;
            if (nonempty_file(fd, e->d_name)) set.present |= bit;
            else set.invalid |= bit;
        }
    }
    closedir(dir);
    if (stopped(s)) return;
    set.complete = s->result->errors == errors_before && set.original_ok && set.present == 15 && !set.invalid && !set.duplicate;
    s->result->sets++;
    if (set.complete) s->result->complete++; else s->result->incomplete++;
    if (s->emit) s->emit(&set, s->user);
}
/* fd is owned and closed here. Never follow symlinks or cross filesystem boundaries. */
static void walk(struct scan *s, int fd, const char *rel, unsigned depth, dev_t device) {
    DIR *dir = fdopendir(fd);
    if (!dir) { close(fd); error(s); return; }
    for (;;) {
        if (stopped(s)) break;
        errno = 0; struct dirent *e = readdir(dir);
        if (!e) { if (errno) error(s); break; }
        if (!budget(s)) break;
        if (e->d_name[0] == '.') continue;
        struct stat st;
        if (fstatat(fd, e->d_name, &st, AT_SYMLINK_NOFOLLOW)) { error(s); continue; }
        if (!S_ISDIR(st.st_mode)) continue;
        if (st.st_dev != device) { error(s); continue; }
        char path[VDJ_PATH_CAP];
        int size = snprintf(path, sizeof path, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
        if (size < 0 || (size_t)size >= sizeof path) { s->status=VDJ_SCAN_LIMIT; break; }
        size_t len = strlen(e->d_name);
        if (len > 6 && !strcasecmp(e->d_name + len - 6, ".stems")) {
            inspect_set(s, fd, e->d_name, path); continue;
        }
        if (depth >= VDJ_SCAN_MAX_DEPTH) { s->status=VDJ_SCAN_LIMIT; break; }
        int child = openat(fd, e->d_name, O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if (child < 0) { error(s); continue; }
        struct stat opened;
        if (fstat(child, &opened) || opened.st_dev != device) { close(child); error(s); continue; }
        walk(s, child, path, depth + 1, device);
    }
    closedir(dir);
}
int vdj_scan_contents(const char *contents, vdj_set_fn emit, vdj_cancel_fn cancel,
                      void *user, struct vdj_scan_result *result) {
    if (!result) return VDJ_SCAN_ERROR;
    memset(result, 0, sizeof *result);
    if (!contents) { result->errors=1; return VDJ_SCAN_ERROR; }
    struct scan s = {emit, cancel, user, result, 0};
    int fd = open(contents, O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (fd < 0) { result->errors=1; return VDJ_SCAN_ERROR; }
    struct stat st;
    if (fstat(fd, &st)) { close(fd); result->errors=1; return VDJ_SCAN_ERROR; }
    walk(&s, fd, "", 0, st.st_dev);
    return s.status ? s.status : result->errors ? VDJ_SCAN_ERROR : VDJ_SCAN_OK;
}
