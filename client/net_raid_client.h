#ifndef _NET_RAID_CLIENT_
#define _NET_RAID_CLIENT_

#include <stdlib.h>
#include <stdio.h>

#define RAID_1 1
#define RAID_5 5

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



#include "structs.h"

struct disk_info* raids;
int num_storages = 0;
struct meta_info client_info;
char* client_info_keys[] = {"errorlog", "cache_size", "cache_replacement", "timeout"};


void client_init();
void client_destroy();

void log_to_file(const char* log_text);


#endif