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
#include "../utils/communication_structs.h"

// #define MAX_PATH_LENGTH 200

static int raid1_getattr(const char *path, struct stat *stbuf);
static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int raid1_open(const char *path, struct fuse_file_info *fi);
static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// static int raid1_opendir (const char* path, struct fuse_file_info* fi);
// static int raid1_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
// static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi);
// static int raid1_mkdir(const char* path, mode_t mode);
// static int raid1_rmdir(const char* path);
static int raid1_access(const char *path, int mask);
static int raid1_mknod(const char *path, mode_t mode, dev_t rdev);



struct fuse_operations raid1_operations = {
	.getattr	= raid1_getattr,
	.readdir	= raid1_readdir,
	.open		= raid1_open,
	.read		= raid1_read,
	// .write 		= raid1_write, 
	// .releasedir = raid1_releasedir,
	// .rename		= raid1_rename, 
	// .unlink		= raid1_unlink, 
	// .rmdir		= raid1_rmdir, 
	// .mkdir		= raid1_mkdir, 
	// .opendir	= raid1_opendir, 
	// .releasedir = raid1_releasedir, 
	// .create 	= raid1_create, 
	// .access = raid1_access,
	.mknod = raid1_mknod,
};


static void print_stat(struct stat* stbuf) {
    printf("st_dev %lu; st_ino %lu; st_mode %lu; st_uid %lu; st_gid %lu \n; st_rdev %lu; st_size %lu; st_atime %lu; st_mtime %lu; st_ctime %lu; st_blksize %lu; st_blocks %lu;\n", 
            (size_t)stbuf->st_dev, (size_t)stbuf->st_ino, (size_t)stbuf->st_mode, (size_t)stbuf->st_uid, 
            (size_t)stbuf->st_gid, (size_t)stbuf->st_rdev, (size_t)stbuf->st_size, (size_t)stbuf->st_atime, 
            (size_t)stbuf->st_mtime, (size_t)stbuf->st_ctime, (size_t)stbuf->st_blksize, (size_t)stbuf->st_blocks);
}



static int raid1_getattr(const char *path, struct stat *stbuf) {
	printf("%s\n", "--------------------SYSCALL GETATTR");
	int args_length = 8 + strlen(path)+1;
	char* msg = malloc(sizeof(int) + 9 + strlen(path)); //
	assert(msg != NULL);

	memcpy(msg, &args_length, sizeof(int));

	sprintf(msg+sizeof(int), "%s%s", "getattr;", path);
	printf("%s\n", msg);
	
	int n;
	if((n = write(socket_fd, msg, args_length + sizeof(int))) < 0) {
		printf("%s\n", "Gotta retry");
	}
	printf("Written bytes %d\n", n);
	free(msg);

	char resp_raw [sizeof(int) + sizeof(struct stat)];
	if(read(socket_fd, resp_raw, sizeof(int) + sizeof(struct stat)) < 0) {
		printf("%s\n", "Couldn't receive reponse");
	}

	int ret_val;
	memcpy(&ret_val, resp_raw, sizeof(int));
	memcpy(stbuf, resp_raw + sizeof(int), sizeof(struct stat));
	printf("returned status %d .\n", ret_val);
	print_stat(stbuf);

	return -ret_val;
}


static int raid1_opendir (const char* path, struct fuse_file_info* fi) {
	printf("%s\n", "--------------------SYSCALL OPENDIR");
}

static int raid_opendir (const char* path, struct fuse_file_info* fi) {
	printf("%s\n", "--------------------SYSCALL OPENDIR");
	char* msg = malloc(10 + strlen(path)); //opendir  + ; + path + \0
	assert(msg != NULL);
	
	sprintf(msg, "opendir;%s", path);
	printf("Sending to server %s\n", msg);
	int bytes_sent;
	if((bytes_sent = write(socket_fd, msg, strlen(msg) + 1)) < 0) {
		printf("%s\n", "Gotta retry");
	}
	printf("OPENDIR: Sent to server %s; total %d bytes\n", msg, bytes_sent);
	free(msg);

	char status_str [12];
	if(read(socket_fd, status_str, 12) < 0) {
		printf("Couldn't read status %s\n", strerror(errno));
	}

	int status = atoi(status_str);
	printf("Status is %d\n", status);

	intptr_t res;
	if(status == 0) {
		
		if(read(socket_fd, &res, sizeof(intptr_t)) < 0) {
			printf("Couldn't read (intptr_t)DIR %s\n", strerror(errno));
		}
	}
	fi->fh = res;

	return status;
	
	
	

	// char resp [21];
	// if(read(socket_fd, &resp, 21) < 0) {
	// 	printf("%s\n", "Couldn't read response to opendir");
	// }
	// printf("OPENDIR: Received response %s\n", resp);

	// char* retval_str = strtok(resp, ";");
	// assert(retval_str != NULL);
	// int ret_val = atoi(retval_str);
	// printf("OPENDIR: Received ret_val %d\n", ret_val);

	// char* fd_str = strtok(NULL, ";");
	// assert(fd_str != NULL);
	// fi->fh = atoi(fd_str); //check?
	// printf("OPENDIR: return vale %d, file handler %lu.\n", ret_val, (size_t)fi->fh);
	// return -ret_val;
}


static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
		off_t offset, struct fuse_file_info *fi) 
{
	printf("%s\n", "--------------------SYSCALL READDIR");
	char* msg = malloc(21 + strlen(path)); //readdir  + ; + path + ; + offset + \0
	assert(msg != NULL);
	sprintf(msg, "%s;%s;%lu", "readdir", path, (size_t)offset);
	
	printf("READDIR: Going to send: %s\n", msg);
	if(write(socket_fd, msg, strlen(msg) + 1) < 0) {
		printf("%s\n", "READDIR: Couldn't send readdir message!");
	}
	free(msg);

	char status_str[12];
	if(read(socket_fd, status_str, 12) < 0) {
		printf("READDIR: %s\n", strerror(errno));
		return -errno;
	}
	int status = atoi(status_str);
	printf("Status is %s or %d\n", status_str, status);
	if(status != 0) {
		printf("READDIR: After opendir server sent %s\n", strerror(status));
		return -status;
	}

	int has_next;
	char* dirname = malloc(MAX_DIR_NAME_LENGTH + 1);
	assert(dirname != NULL);
	char buffer[1024];
	while(1) {
		if(read(socket_fd, buffer, sizeof(int)) < 0) {
			printf("Couldn't read has_next %s\n", strerror(errno));
		}
		memcpy(&has_next, buffer, sizeof(int));
		printf("has_next = %d\n", has_next);

		if(has_next == 0)
			break;
		
		struct stat st;
		if(read(socket_fd, &st, sizeof(struct stat)) < 0) {
			printf("Couldn't read struct stat %s\n", strerror(errno));
			free(dirname);
			//TODO closedir or hotswap
			return -errno;
		}
		print_stat(&st);

		
		if(read(socket_fd, dirname, MAX_DIR_NAME_LENGTH) < 0) {
			printf("Couldn't read dirname %s\n", strerror(errno));
			free(dirname);
			//TODO closedir or hotswap
			return -errno;
		}
		printf("%s\n", dirname);
		//TODO: this check should send something to server, or server might block trying to send 
		//some info, while client can no longer receive it, since its buffer is full
		if (filler(buf, dirname, &st, 0)) //should last argument be nonzero?
			break;
	}
	free(dirname);
	return 0;
}


static int raid1_access(const char *path, int mask) {
	printf("%s\n", "--------------------SYSCALL ACCESS");
	log_msg(strcat("Syscall access; Path is ", path));

	int num_bytes_to_send = 7 + strlen(path)+1 + sizeof(int);
	if(write(socket_fd, &num_bytes_to_send, sizeof(int)) < 0) {
		printf("%s\n", "Couldn't send num bytes");
		log_error("GETATTR. Couldn't send num bytes", errno);
		return -1;
	}

	char msg[8 + MAX_PATH_LENGTH + sizeof(int)]; //access;path;mask\0
	sprintf(msg, "%s;%s", "access", path);
	int mask_arg = mask;
	memcpy(msg + 8 + strlen(path), &mask_arg, sizeof(int));
	printf("Sending %s\n", msg);

	if(write(socket_fd, msg, num_bytes_to_send) < 0) {
		printf("%s\n", "ACCESS: Couldn't send message to server!");
	}

	char response [12];
	if(read(socket_fd, response, strlen(response)) < 0) {
		printf("%s\n", "ACCESS: Couldn't read response");
		return -1;
	}
	int res = atoi(response);
	printf("Received result %d\n", res);
	return -res;
}


// static int raid1_mkdir(const char* path, mode_t mode) {
// 	printf("%s\n", "--------------------SYSCALL MKDIR");
// 	// int res;

	

// 	char syscall[6] = "mkdir";
// 	if(write(socket_fd, syscall, strlen(syscall) + 1) < 0){
// 		printf("Couldn't send syscall %s\n", strerror(errno));
// 		return -1;
// 	}

// 	if(write(socket_fd, path, strlen(path) + 1) < 0) {
// 		printf("Couldn't send path %s\n", strerror(errno));
// 		return -1;
// 	}

// 	if(write(socket_fd, &mode, sizeof(mode_t)) < 0){
// 		printf("Couldn't send mode %s\n", strerror(errno));
// 		return -1;
// 	}

// 	return 0;
// }


// static int raid1_rmdir(const char* path){
// 	printf("%s\n", "--------------------SYSCALL RMDIR");
// 	int res;

// 	res = rmdir(path);
// 	if (res == -1)
// 		return -errno;

// 	return 0;
// }


// static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
// 	printf("%s\n", "--------------------SYSCALL CREATE");
// 	int res;

// 	res = open(path, fi->flags, mode);
// 	if (res == -1)
// 		return -errno;
// 	fi->fh = res;

// 	return 0;

// }

static int raid1_mknod(const char *path, mode_t mode, dev_t rdev) {
	// char buf[2048];
	char* buf = malloc(8 + MAX_PATH_LENGTH + sizeof(mode_t) + sizeof(dev_t));
	assert(buf != NULL);

	strcpy(buf, "mknod;");
	strcpy(buf + 7, path);
	mode_t mode_arg = mode;
	memcpy(buf+7+strlen(path)+1, &mode_arg, sizeof(mode_t));
	dev_t rdev_arg = rdev;
	memcpy(buf+7+strlen(path)+1+sizeof(mode_t), &rdev_arg, sizeof(dev_t));

	// char syscall[6] = "mknod";
	// memcpy(buf, syscall, strlen(syscall));
	int num_bytes_to_send = 8 + strlen(path) + sizeof(mode_t) + sizeof(dev_t);
	if(write(socket_fd, buf, num_bytes_to_send) < 0) {
		printf("Couldn't send info %s\n", strerror(errno));
	}

	free(buf);
	return 0;
}



static int raid1_open(const char *path, struct fuse_file_info *fi) {
	printf("%s\n", "SYSCALL OPEN");
	log_msg(strcat("Syscall open; Path is ", path));

	int num_bytes_to_send = 5 + strlen(path)+1 + sizeof(int);
	if(write(socket_fd, &num_bytes_to_send, sizeof(int)) < 0) {
		printf("%s\n", "Couldn't send num bytes");
		log_error("Couldn't send num bytes", errno);
		return -1;
	}

	char buf [6 + MAX_PATH_LENGTH + sizeof(int)]; //open; + strlen + path + flags
	sprintf(buf, "open;%s", path);
	int flags = fi->flags; 
	memcpy(buf + 5 + strlen(path) + 1, &flags, sizeof(int));
	printf("flags are %d some part of buf is %s\n", flags, buf);

	if(write(socket_fd, buf, 6 + strlen(path) + sizeof(int)) < 0) {
		printf("%s\n", "Couldn't send path and flags");
		log_error("OPEN sending path and flags.", errno);
	}
	sleep(0.2);

	int resp;
	if(read(socket_fd, &resp, sizeof(int)) < 0) {
		printf("%s\n", "Couldn't read response");
		log_error("OPEN readin servers response ", errno);
	}

	printf("Server response %d\n", resp);
	// uint64_t fd = (uint64_t) open(path, fi->flags);
	// fi->fh = fd;
	// if (fd < 0)
	// 	return -1;
	return resp;
}


static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	printf("%s\n", "SYSCALL READ");
	// int fd;
	// int res;

	// if(fi == NULL)
	// 	fd = open(path, O_RDONLY);
	// else
	// 	fd = fi->fh;
	
	// if (fd == -1)
	// 	return -errno;

	// res = pread(fd, buf, size, offset);
	// if (res == -1)
	// 	res = -errno;

	// if(fi == NULL)
	// 	close(fd);
	return 0;
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
		log_error("Couldn't acquire endpoint file descriptor", errno);
		return -1;
	}

	if(split_address(address_str, &ip, &port) < 0){
		log_error("Malformed server address", errno);
		return -1; //special error code?
	}

	server_address->sin_family = AF_INET;
	server_address->sin_port = htons(port);
	server_address->sin_addr.s_addr = ip;

	int conn_status = -1;
	time_t start = time(NULL);
	
	while(conn_status < 0 && (difftime(time(NULL), start) <= timeout)) {
		log_msg("Trying to connect to server");
		printf("%s\n", "Trying to connect to server");
		conn_status = connect(socket_fd, (struct sockaddr*)server_address, sizeof(*server_address));
		if(conn_status < 0) {
			sleep(2);
		}
	}

	if(conn_status < 0) {
		log_error("Couldn't connect to server", errno);
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