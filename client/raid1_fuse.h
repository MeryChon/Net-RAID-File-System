#ifndef _RAID1_FUSE_
#define _RAID1_FUSE_

#define FUSE_USE_VERSION 26
#define PATH_MAX 200

#include <fuse.h>



const char *raid1_root;
int raid1_fuse_main(const char* process_name, const char* mountpoint);

#endif