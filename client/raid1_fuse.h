#ifndef _RAID1_FUSE_
#define _RAID1_FUSE_

#define FUSE_USE_VERSION 26


#include <fuse.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include "structs.h"


// struct stat1 {
// 	char syscall [10];
// 	struct stat;

// 	dev_t     st_dev;     //ID of device containing file
// 	ino_t     st_ino;     //file serial number
// 	mode_t    st_mode;    //mode of file (see below)
// 	nlink_t   st_nlink;   //number of links to the file
// 	uid_t     st_uid;     //user ID of file
// 	gid_t     st_gid;     //group ID of file
// 	dev_t     st_rdev;    //device ID (if file is character or block special)
// 	off_t     st_size;    //file size in bytes (if file is a regular file)
// 	time_t    st_atime;   //time of last access
// 	time_t    st_mtime;   //time of last data modification
// 	time_t    st_ctime;   //time of last status change
// 	blksize_t st_blksize; /*a filesystem-specific preferred I/O block size for
// 	                     this object.  In some filesystem types, this may
// 	                     vary from file to file*/
// 	blkcnt_t  st_blocks; 

// } __attribute__((packed));


const char *raid1_root;

struct sockaddr_in server_addr;
int socket_fd; //TODO: Must be deleted
struct sockaddr_in* server_addrs;
int num_servers;
int* server_sfds;
int* write_results;
int* read_results;
int* read_results;
int* active_servers;
int timeout;

int raid1_fuse_main(const char* process_name, struct meta_info client_info, struct disk_info storage_info);

#endif