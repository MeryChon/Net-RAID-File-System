#include "raid1_fuse.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

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


static void print_stat(struct stat* stbuf) {
    printf("st_dev %lu; st_ino %lu; st_mode %lu; st_uid %lu; st_gid %lu \n; st_rdev %lu; st_size %lu; st_atime %lu; st_mtime %lu; st_ctime %lu; st_blksize %lu; st_blocks %lu;\n", 
            (size_t)stbuf->st_dev, (size_t)stbuf->st_ino, (size_t)stbuf->st_mode, (size_t)stbuf->st_uid, 
            (size_t)stbuf->st_gid, (size_t)stbuf->st_rdev, (size_t)stbuf->st_size, (size_t)stbuf->st_atime, 
            (size_t)stbuf->st_mtime, (size_t)stbuf->st_ctime, (size_t)stbuf->st_blksize, (size_t)stbuf->st_blocks);
}



static int raid1_getattr(const char *path, struct stat *stbuf) {
	char* msg = malloc(9 + strlen(path)); //getattr  + ; + path + \0
	assert(msg != NULL);

	sprintf(msg, "%s%s", "getattr;", path);
	if(write(socket_fd, msg, strlen(msg)+1) < 0) {
		printf("%s\n", "Gotta retry");
	}

	//Get int indicating success or failure of getattr (stat syscall)
	int resp_raw;
	if(read(socket_fd, &resp_raw, sizeof(int)) < 0) {
		printf("%s\n", "Couldn't receive status code");
	}
	int resp = ntohl(resp_raw);
	printf("returned %d converted to %d.\n", resp_raw, resp);

	//Going to receive stat structure now
	int bytes_read = read(socket_fd, stbuf, sizeof(struct stat));
	if(bytes_read < 0) {
		printf("%s\n", "Couldn't receive struct stat");
	}

	print_stat(stbuf);

	return -resp;
}


static int raid1_opendir (const char* path, struct fuse_file_info* fi) {
	// char* msg = malloc(7 + strlen(path) + 2); //opendir  + ; + path + \0
	// assert(msg != NULL);
	// sprintf(msg, "%s%s", "opendir;", path);
	// printf("%s\n", msg);
	// if(write(socket_fd, msg, strlen(msg)) < 0) {
	// 	printf("%s\n", "Gotta retry");
	// }
	// char buffer[1024];
	// if(read(socket_fd, &buffer, 30) < 0) {
	// 	printf("%s\n", "no reply from server yet");
	// }
	// printf("Received from server %s\n", buffer);
	return 0;
}


static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
		off_t offset, struct fuse_file_info *fi) 
{
	// if(write(socket_fd, "readdir", 7) < 0) {
	// 	printf("%s\n", "Gotta retry");
	// }
	// char buffer[1024];
	// if(read(socket_fd, &buffer, 30) < 0) {
	// 	printf("%s\n", "no reply from server yet");
	// }
	// printf("Received from server %s\n", buffer);
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
	// uint64_t fd = (uint64_t) open(path, fi->flags);
	// fi->fh = fd;
	// if (fd < 0)
	// 	return -1;
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


static int create_socket_connection(char* address_str, int timeout, int* sfd_ptr, 
																struct sockaddr_in* server_address) {
	int ip, port;
	(void) sfd_ptr;
	
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0) {
		log_error("Couldn't acquire endpoint file descriptor");
		return -1;
	}

	if(split_address(address_str, &ip, &port) < 0){
		log_error("Malformed server address");
		return -1; //special error code?
	}

	server_address->sin_family = AF_INET;
	server_address->sin_port = htons(port);
	server_address->sin_addr.s_addr = ip;

	int conn_status = -1;
	time_t start = time(NULL);
	
	printf("%d\n", timeout);
	
	while(conn_status < 0 && (difftime(time(NULL), start) <= timeout)) {
		log_msg("Trying to connect to server");
		printf("%s\n", "Trying to connect to server");
		conn_status = connect(socket_fd, (struct sockaddr*)server_address, sizeof(*server_address));
		if(conn_status < 0) {
			sleep(2);
		}
	}

	if(conn_status < 0) {
		log_error("Couldn't connect to server");
		printf("%s %s\n", "Couldn't connect to server ", address_str);
		//do hot swap
		return -1;
	}
	printf("%s %s\n", "Connected to", address_str);
	log_connection(address_str);

	return 0;
}


int raid1_fuse_main(const char* process_name, struct meta_info client_info,
																	 struct disk_info storage_info) {
	raid1_root = strdup(storage_info.mountpoint);
	int i = 0;
	for(;i < storage_info.num_servers; i++) {
		if(create_socket_connection(storage_info.servers[i], client_info.timeout, &socket_fd, &server_addr) != 0) {
			printf("%s\n", "Need hot swap");
		}
	}
	printf("%d\n", socket_fd);
	char* fuse_args[3];
	fuse_args[0] = strdup(process_name);
	fuse_args[1] = strdup("-f");
	fuse_args[2] = strdup(raid1_root);

	return fuse_main(3, fuse_args, &raid1_operations, NULL);
}