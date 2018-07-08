#ifndef _NET_RAID_CLIENT_
#define _NET_RAID_CLIENT_

#include <stdlib.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 128
#define MAX_NUM_SERVERS 32
#define MAX_NUM_RAIDS 6
#define MAX_DIR_NAME_LENGTH 32

#define ERROR_LOG 0
#define CACHE_SIZE 1
#define CACHE_REPLACEMENT 2
#define TIMEOUT 3

#define TRUE 1
#define FALSE 0



struct meta_info {
	char* errorlog_path;
	int cache_size; // in MB
	char* cache_replacement_algorithm; //only implementing rlu
	int timeout; // in ms?
};

struct disk_info {
	char* diskname;
	char* mountpoint;
	int raid;
	char** servers;
	char* hotswap; 
};

struct disk_info* raids;
struct meta_info client_info;
char* client_info_keys[] = {"errorlog", "cache_size", "cache_replacement", "timeout"};


void client_init();

void client_destroy();


#endif