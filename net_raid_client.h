#ifndef _NET_RAID_CLIENT_
#define _NET_RAID_CLIENT_

#include <stdlib.h>
#include <stdio.h>


struct meta_info {
	char* errorlog_path;
	size_t cache_size; // in MB
	int cache_replacment_algorithm; //only implementing rlu
	size_t timeout; // in ms?
};

struct disk_info {
	char* diskname;
	char* mountpoint;
	size_t raid;
	char** servers;
	char* hotswap; 
};

struct disk_info* disks;

#endif