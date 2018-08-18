#ifndef _STRUCTS_
#define _STRUCTS_


struct meta_info {
	char* errorlog_path;
	int cache_size; // in MB
	char* cache_replacement_algorithm; //only implementing lru
	int timeout; // in s?
};

struct disk_info {
	char* diskname;
	char* mountpoint;
	int raid;
	char** servers;
	int num_servers;
	char* hotswap; 
};

struct disk_info* raids;


#endif