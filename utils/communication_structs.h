#ifndef _COMMUNICATION_STRUCTS_
#define _COMMUNICATION_STRUCTS_

#include <sys/stat.h>

#define MAX_PATH_LENGTH 2048
#define MAX_DIR_NAME_LENGTH 64

struct syscall_params {
	char syscall[10];
	char path [MAX_PATH_LENGTH];
	size_t offset;
};


struct getattr_resp {
	int ret_val;
	struct stat stat_buf;
};

struct opendir_resp{
	int ret_val;
	int file_desc;
};

struct readdir_resp{
	char dir_name [MAX_DIR_NAME_LENGTH];
	struct stat st;
};


#endif
