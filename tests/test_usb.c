#include "usb_watch.h"
#include <assert.h>
#include <stdio.h>
static int parse(const char *s,struct vdj_mounts *out) { FILE *f=tmpfile();assert(f);fputs(s,f);rewind(f);int r=vdj_mounts_read(f,out);fclose(f);return r; }
int main(void) {
 struct vdj_mounts a,b;
 assert(parse("",&a)==0 && a.count==0);
 const char *one="31 20 8:1 / /media/usb/sda1 rw - vfat /dev/sda1 rw\n";
 assert(parse(one,&a)==0 && a.count==1 && a.items[0].id==31);
 assert(parse("32 20 8:1 / /media/usb/sda1 rw - vfat /dev/sda1 rw\n",&b)==0);
 assert(!vdj_mounts_equal(&a,&b)); /* same device/path, new mount */
 assert(parse("32 20 8:2 / /media/usb/sda2 rw - vfat /dev/sda2 rw\n31 20 8:1 / /media/usb/sda1 rw - vfat /dev/sda1 rw\n",&a)==0);
 assert(parse("31 20 8:1 / /media/usb/sda1 rw - vfat /dev/sda1 rw\n32 20 8:2 / /media/usb/sda2 rw - vfat /dev/sda2 rw\n",&b)==0);
 assert(a.count==2 && vdj_mounts_equal(&a,&b)); /* order-independent, partitions distinct */
 assert(parse("31 20 8:1 / /media/usb/sda1/sub rw - vfat /dev/sda1 rw\n",&a)==0 && !a.count);
 assert(parse("broken\n",&a)==-1);
 assert(parse("31 20 8:1 / /media/usb/sda1 rw\n",&a)==-1);
 assert(parse("31 20 8:1 / /media/usb/sda1 rw - vfat /dev/sda1 rw\n32 20 8:2 / /media/usb/sda1 rw - vfat /dev/sda2 rw\n",&a)==-1);
 FILE *f=tmpfile();assert(f);
 for (unsigned i=0;i<9;i++) fprintf(f,"%u 1 8:%u / /media/usb/sd%c1 rw - vfat /dev/sd%c1 rw\n",i+20,i,'a'+i,'a'+i);
 rewind(f);assert(vdj_mounts_read(f,&a)==-1);fclose(f);
 puts("Mount identity, ordering, partitions, malformed input and overflow passed");
}
