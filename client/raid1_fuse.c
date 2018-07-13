#include "raid1_fuse.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include "log.h"



static int raid1_getattr(const char *path, struct stat *stbuf);
static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int raid1_open(const char *path, struct fuse_file_info *fi);
static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int raid1_opendir (const char* path, struct fuse_file_info* fi);
// static int raid1_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int raid1_mkdir(const char* path, mode_t mode);
static int raid1_rmdir(const char* path);


// static const char *hello_str = "Hello World!\n";
// static const char *hello_path = "/hello";


struct fuse_operations raid1_operations = {
	.getattr	= raid1_getattr,
	.readdir	= raid1_readdir,
	.open		= raid1_open,
	.read		= raid1_read,
	// .write 		= raid1_write, 
	// .releasedir = raid1_releasedir,
	// .rename		= raid1_rename, 
	// .unlink		= raid1_unlink, 
	.rmdir		= raid1_rmdir, 
	.mkdir		= raid1_mkdir, 
	.opendir	= raid1_opendir, 
	// .releasedir = raid1_releasedir, 
	.create 	= raid1_create, 
};



static void fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, raid1_root);
    if(raid1_root[strlen(raid1_root) - 1] != '/' && path[0] != '/')
    	strncat(fpath, "/", PATH_MAX);
    if(raid1_root[strlen(raid1_root) - 1] == '/' && path[0] == '/') {
        strncat(fpath, path+1, PATH_MAX); // ridiculously long paths will break here
    } else {
    	strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
    }
    log_msg(fpath);
}


static int raid1_getattr(const char *path, struct stat *stbuf) {
	// (void) fi;
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}


static int raid1_opendir (const char* path, struct fuse_file_info* fi) {
	int res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}


static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
		off_t offset, struct fuse_file_info *fi) 
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	char fpath[PATH_MAX];
	fullpath(fpath, path);

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		log_msg(de->d_name);
		if (filler(buf, de->d_name, NULL, 0))
			break;
	}

	closedir(dp);
	return 0;
}



static int raid1_mkdir(const char* path, mode_t mode) {
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}


static int raid1_rmdir(const char* path){
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}


static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	int res;

	res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;
	fi->fh = res;

	return 0;
}



static int raid1_open(const char *path, struct fuse_file_info *fi) {
	uint64_t fd = (uint64_t) open(path, fi->flags);
	fi->fh = fd;
	if (fd < 0)
		return -1;
	return 0;
}


static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	int fd;
	int res;

	if(fi == NULL)
		fd = open(path, O_RDONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}


static int split_address(const char* address, int* ip, int* port) {
	char* addr_copy = strdup(address);
	char* ip_str = strtok(addr_copy, ":");
	if(ip_str == NULL) return -1;
	char* port_str = strtok(NULL, ":");
	if(port_str == NULL) return -1;


	inet_pton(AF_INET, ip_str, ip);
	*port = atoi(port_str);

	return 0;
}


int raid1_fuse_main(const char* process_name, struct disk_info storage_info) {
	raid1_root = strdup(storage_info.mountpoint);
	int i = 0;
	for(;i < storage_info.num_servers; i++) {
		//create sockets (for each remote server)
		int network_socket = socket(AF_INET, SOCK_STREAM, 0);
		int ip;
		int port;
		split_address(storage_info.servers[i], &ip, &port);

		//specify address and port
		struct sockaddr_in server_address;
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(port); //need to extract port from passed server address
		server_address.sin_addr.s_addr = ip;

		int conn_status = connect(network_socket, (struct sockaddr*)&server_address, sizeof(server_address));
		if(conn_status == -1) {
			printf("%s\n", "Couldn't connect");
			log_error("Cound not connect");		
			//try again	
		} else {
			printf("%s\n", "Connected");
			log_connection(storage_info.servers[i]);
			write(network_socket, "hello!", 6);
			char buf[1024];
			read(network_socket, &buf, 6);
			printf("Received from server %s\n", buf);
    		sleep(600);
    		close(network_socket);
		}	


	}
	char* fuse_args[3];
	fuse_args[0] = strdup(process_name);
	fuse_args[1] = strdup("-f");
	fuse_args[2] = strdup(raid1_root);

	return 0;
	
	// return fuse_main(3, fuse_args, &raid1_operations, NULL);
}