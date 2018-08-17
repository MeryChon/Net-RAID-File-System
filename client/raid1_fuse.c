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
static int raid1_opendir (const char* path, struct fuse_file_info* fi);
// static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int raid1_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int raid1_mkdir(const char* path, mode_t mode);
static int raid1_rmdir(const char* path);
static int raid1_access(const char *path, int mask);
static int raid1_mknod(const char *path, mode_t mode, dev_t rdev);
static int raid1_rename(const char* from, const char *to);
static int raid1_utime(const char *path, struct utimbuf *ubuf);
static int raid1_unlink(const char *path);
static int raid1_truncate(const char *path, off_t size);
static int raid1_utimens(const char *path, const struct timespec ts[2]);
static int raid1_releasedir(const char *path, struct fuse_file_info *fi);



struct fuse_operations raid1_operations = {
	.getattr	= raid1_getattr,
	.readdir	= raid1_readdir,
	.open		= raid1_open,
	.read		= raid1_read,
	.write 		= raid1_write, 
	.releasedir = raid1_releasedir,
	.opendir	= raid1_opendir, 
	// .create 	= raid1_create,
	.rename		= raid1_rename, 
	.unlink		= raid1_unlink, 
	.mkdir		= raid1_mkdir, 
	.rmdir		= raid1_rmdir,  
	.access = raid1_access,
	.mknod = raid1_mknod,
	.utime = raid1_utime,
	.truncate = raid1_truncate,
	.utimens = raid1_utimens,
};


static void print_stat(struct stat* stbuf) {
    printf("st_dev %lu; st_ino %lu; st_mode %lu; st_uid %lu; st_gid %lu \n; st_rdev %lu; st_size %lu; st_atime %lu; st_mtime %lu; st_ctime %lu; st_blksize %lu; st_blocks %lu;\n", 
            (size_t)stbuf->st_dev, (size_t)stbuf->st_ino, (size_t)stbuf->st_mode, (size_t)stbuf->st_uid, 
            (size_t)stbuf->st_gid, (size_t)stbuf->st_rdev, (size_t)stbuf->st_size, (size_t)stbuf->st_atime, 
            (size_t)stbuf->st_mtime, (size_t)stbuf->st_ctime, (size_t)stbuf->st_blksize, (size_t)stbuf->st_blocks);
}


static char* fill_in_basic_info (int args_length, const char* syscall, const char* path, int* num_bytes_written) {
	char* buf = malloc(sizeof(int) + args_length);
	assert(buf != NULL);

	int bytes_written = 0;
	int n = args_length;

	memcpy(buf, &n, sizeof(int));
	bytes_written += sizeof(int);
	printf("args length %d\n", args_length);
	strcpy(buf + bytes_written, syscall);
	bytes_written += strlen(syscall);
	printf("bytes_written=%d\n", bytes_written);
	strcpy(buf + bytes_written, path);
	bytes_written += strlen(path) + 1;
	*num_bytes_written = bytes_written;
	
	return buf;
}



static int send_to_all_servers(char* msg, int size) {
	int i, res;
	res = -1;
	for(i=0; i<num_servers; i++) {
		write_results[i] = -1;
		time_t start = time(NULL);	
		do {
			write_results[i] = write(server_sfds[i], msg, size);
			printf("Server N %d, write_result = %d\n", i, write_results[i]);
		} while(write_results[i] < 0 && (difftime(time(NULL), start) <= timeout));			

		if(write_results[i] < 0) {
			printf("%s\n", "tavzeit dzala araa");
			log_error("Couldn't send data to server", errno);
		} else {
			res = 0;
		}
	}

	return res;
}


/* Returns fd of the first server socket that was available at that moment
*/
static int send_to_available_server(char* msg, int size) {
	int i = 0;
	int res = -1;
	while(i < num_servers && res < 0) {
		printf("Sending to server %d\n", server_sfds[i]);
		int bytes_sent = write(server_sfds[i], msg, size);
		printf("bytes_sent = %d\n", bytes_sent);
		if(bytes_sent <= 0) {
			i++;
			continue;
		} else {
			res = server_sfds[i];	
			break;		
		}
	}

	return res;
}



int raid1_releasedir(const char *path, struct fuse_file_info *fi)
{
    // printf("----  releasedir : %s\n", path);

    // int retstat = 0;
    
    // closedir((DIR *) (uintptr_t) fi->fh);
    
    return 0;
}



static int raid1_getattr(const char *path, struct stat *stbuf) {
	printf("%s\n", "-------------------------------- GETATTR");
	int args_length = 8 + strlen(path)+1;
	char* msg = malloc(sizeof(int) + 9 + strlen(path)); //
	assert(msg != NULL);

	memcpy(msg, &args_length, sizeof(int));

	sprintf(msg+sizeof(int), "%s%s", "getattr;", path);
	
	int sfd = send_to_available_server(msg, args_length + sizeof(int));
	if(sfd < 0) {
		printf("%s\n", "ver chevitanet");
		return -1;
	}
	// if((n = write(socket_fd, msg, args_length + sizeof(int))) < 0) {
	// 	printf("%s\n", "Gotta retry");
	// }
	free(msg);

	//---------- receiving response -----------------
	char resp_raw [sizeof(int) + sizeof(struct stat)];
	if(read(sfd, resp_raw, sizeof(int) + sizeof(struct stat)) < 0) {
		printf("%s\n", "Couldn't receive reponse");
	}

	int ret_val;
	memcpy(&ret_val, resp_raw, sizeof(int));
	printf("received status ret_val=%d\n", ret_val);
	memcpy(stbuf, (struct stat*)(resp_raw + sizeof(int)), sizeof(struct stat));
	print_stat(stbuf);

	return -ret_val; 
}


static int raid1_mkdir(const char* path, mode_t mode) {
	printf("%s\n", "-------------------------------- MKDIR");
	int args_length = 6 + strlen(path)+ 1 + sizeof(mode_t);
	char* msg = malloc(sizeof(int) + args_length); //
	assert(msg != NULL);

	memcpy(msg, &args_length, sizeof(int));
	printf("args length %d\n", args_length);

	strcpy(msg + sizeof(int), "mkdir;");
	printf("%s\n", msg + sizeof(int));

	strcpy(msg + sizeof(int) + 6, path);
	printf("%s\n", msg + sizeof(int) + 6);

	memcpy(msg + sizeof(int) + 6 + strlen(path) + 1, &mode, sizeof(mode_t));

	if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int response;
	if(read(socket_fd, &response, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("server responded with %d\n", response);
	free(msg);
	return -response;
}


static int raid1_rmdir(const char* path){
	printf("%s\n", "-------------------------------- RMDIR");

	int args_length = 6 + strlen(path)+ 1;
	char* msg = malloc(sizeof(int) + args_length); //
	assert(msg != NULL);

	memcpy(msg, &args_length, sizeof(int));
	printf("args length %d\n", args_length);

	strcpy(msg + sizeof(int), "rmdir;");

	strcpy(msg + sizeof(int) + 6, path);
	printf("part of message %s\n", msg + sizeof(int));

	if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int response;
	if(read(socket_fd, &response, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("Server responded with %d\n", response);
	free(msg);
	return -response;
}



static int raid1_open(const char *path, struct fuse_file_info *fi) {
	printf("%s\n", "-------------------------------- OPEN");

	int args_length = 5 + strlen(path) + 1 + sizeof(int);
	char* msg = malloc(sizeof(int) + args_length); //
	assert(msg != NULL);

	memcpy(msg, &args_length, sizeof(int));
	printf("args length %d\n", args_length);

	strcpy(msg + sizeof(int), "open;");
	strcpy(msg + sizeof(int) + 5, path);
	printf("%s\n", msg + sizeof(int));

	int flags = fi->flags;
	printf("flags are %d\n", flags);
	memcpy(msg + sizeof(int) + 5 + strlen(path) + 1, &flags, sizeof(int));

	if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int status, fd;
	char response[2*sizeof(int)];
	if(read(socket_fd, response, 2*sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	memcpy(&status, response, sizeof(int));
	memcpy(&fd, response + sizeof(int), sizeof(int));
	printf("status is %d, fd is %d\n", status, fd);
	fi->fh = fd;

	free(msg);
	return -status;
}



static int raid1_opendir (const char* path, struct fuse_file_info* fi) {
	printf("%s\n", "-------------------------------- OPENDIR");
	return 0;
}



static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	printf("%s\n", "-------------------------------- READ");

	int args_length = 5 + strlen(path) + 1 + sizeof(size_t) + sizeof(off_t);
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "read;", path, &bytes_written);//malloc(sizeof(int) + args_length); //
	printf("Part of the message yall %s\n", msg + sizeof(int));

	memcpy(msg + bytes_written, &size, sizeof(size_t));
	memcpy(msg + bytes_written + sizeof(size_t), &offset, sizeof(off_t));

	if(write(socket_fd, msg, args_length + sizeof(int)) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}
	
	char* response = malloc(2*sizeof(int) + size);
	assert(response != NULL);
	if(read(socket_fd, response, 2*sizeof(int) + size) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}

	int status, bytes_read;
	memcpy(&status, response, sizeof(int));
	memcpy(&bytes_read, response+sizeof(int), sizeof(int));
	printf("Status is %d, bytes_read is %d\n", status, bytes_read);
	if(bytes_read < 0) {
		printf("%s\n", "Something's not right");
		free(msg);
		return status;
	}

	memcpy(buf, response + 2*sizeof(int), bytes_read);
	printf("%s\n", buf);
	free(msg);
	free(response);
	return bytes_read;
}



static int raid1_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("%s\n", "-------------------------------- WRITE");
	int args_length = 6 + strlen(path) + 1 + sizeof(size_t) + sizeof(off_t) + size;
	int bytes_written;

	char* msg = fill_in_basic_info(args_length, "write;", path, &bytes_written);
	printf("Part of the msg %s\n", msg + sizeof(int));

	memcpy(msg + bytes_written, &size, sizeof(size_t));
	memcpy(msg + bytes_written + sizeof(size_t), &offset, sizeof(off_t));
	memcpy(msg + bytes_written + sizeof(size_t) + sizeof(off_t), buf, size);

	if(write(socket_fd, msg, args_length + sizeof(int) + size) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}


	char* response = malloc(2*sizeof(int) + size);
	assert(response != NULL);
	if(read(socket_fd, response, 2*sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}

	int status, bytes_written_to_fd;
	memcpy(&status, response, sizeof(int));
	memcpy(&bytes_written_to_fd, response+sizeof(int), sizeof(int));
	printf("Status is %d, bytes_written_to_fd is %d\n", status, bytes_written_to_fd);
	if(status != 0 || bytes_written_to_fd < 0) {
		free(msg);
		return status;
	}

	free(msg);
	free(response);
	return bytes_written_to_fd;
}



static int raid1_mknod(const char *path, mode_t mode, dev_t rdev) {
	printf("%s\n", "-------------------------------- MKNOD");

	int args_length = 6 + strlen(path) + 1 + sizeof(mode_t) + sizeof(dev_t);
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "mknod;", path, &bytes_written);//malloc(sizeof(int) + args_length); //
	printf("Part of the message %s\n", msg + sizeof(int));
	printf("mode = %d, dev_t = %lu\n", mode, rdev);

	memcpy(msg + bytes_written, &mode, sizeof(mode_t));
	memcpy(msg + bytes_written + sizeof(mode_t), &rdev, sizeof(dev_t));

	if(write(socket_fd, msg, bytes_written + sizeof(mode_t) + sizeof(dev_t)) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int response;
	if(read(socket_fd, &response, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("response=%d\n", response);
	free(msg);
	return -response;
}


static int raid1_utime(const char *path, struct utimbuf *ubuf) {
	printf("%s\n", "-------------------------------- UTIME");
	int args_length = 6 + strlen(path) + 1 + sizeof(struct utimbuf);
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "utime;", path, &bytes_written);//malloc(sizeof(int) + args_length); //
	printf("Part of the message %s\n", msg + sizeof(int));

	if(write(socket_fd, msg, bytes_written + sizeof(struct utimbuf)) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int response;
	if(read(socket_fd, &response, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("response %d\n", response);
	free(msg);
	return -response;
}


static void print_timespec_array(const struct timespec ts[]) {
	printf("ts[0].tv_sec=%lu; ts[0].tv_nsec / 1000=%lu; ts[1].tv_sec=%lu; ts[1].tv_nsec / 1000=%lu\n", 
		ts[0].tv_sec, ts[0].tv_nsec / 1000, ts[1].tv_sec, ts[1].tv_nsec / 1000);
}


static int raid1_utimens(const char *path, const struct timespec ts[2]){
	printf("%s\n", "-------------------------------- UTIMENS");
	int args_length = 8 + strlen(path) + 1 + 2*sizeof(struct timespec);
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "utimens;", path, &bytes_written);
	printf("args_length=%d; Part of the message %s\n", args_length, msg + sizeof(int));

	memcpy(msg + bytes_written, ts, 2 * sizeof(struct timespec));
	print_timespec_array(ts);

	if(write(socket_fd, msg, sizeof(int) + args_length) <=0 ) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int resp;
	if(read(socket_fd, &resp, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("response is %d\n", resp);
	return resp;
}

static int raid1_unlink(const char *path) {
	printf("%s\n", "-------------------------------- UNLINK");
	int args_length = 7 + strlen(path) + 1;
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "unlink;", path, &bytes_written);


	if(write(socket_fd, msg, sizeof(int) + bytes_written) <= 0){
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int response;
	if(read(socket_fd, &response, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("Response is %d\n", response);
	return -response;
}


static int raid1_truncate(const char *path, off_t size) {
	printf("%s\n", "-------------------------------- TRUNCATE");
	int args_length = 9 + strlen(path) + 1 + sizeof(off_t);
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "truncate;", path, &bytes_written);
	printf("Part of the message %s\n", msg + sizeof(int));

	printf("size=%lu\n", size);
	memcpy(msg + bytes_written, &size, sizeof(off_t));

	if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int resp;
	if(read(socket_fd, &resp, sizeof(int)) <= 0) {
		printf("Couldn't receive response %s\n", strerror(errno));
	}
	printf("resp=%d\n", resp);

	return resp;
}



static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	printf("%s\n", "-------------------- READDIR");
	int args_length = 8 + strlen(path) + 1;
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "readdir;", path, &bytes_written);//malloc(sizeof(int) + args_length); //
	// printf("Part of the message %s\n", msg + sizeof(int));

	if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}

	int status, bytes_to_read;
	char* buffer = malloc(2 * sizeof(int));
	assert(buffer != NULL);
	if(read(socket_fd, buffer, sizeof(int)*2) <= 0){
		printf("Couldn't receive response info %s\n", strerror(errno));
	}
	memcpy(&status, buffer, sizeof(int));
	memcpy(&bytes_to_read, buffer+sizeof(int), sizeof(int));
	printf("status=%d; bytes_to_read=%d\n", status, bytes_to_read);
	sleep(0.5);
	//int bytes_read_on_iteration = (bytes_to_read < 1024) ? bytes_to_read : 1024;
	if(status == 0) {
		int bytes_received = 0;
		buffer = realloc(buffer, bytes_to_read);
		assert(buffer != NULL);
		//Reading with one read might be a bad idea, as data might be pretty big 
		//for big dig directories
		bytes_received = read(socket_fd, buffer, bytes_to_read);
		// while(bytes_received < bytes_to_read) {
		// 	int nbr = read(socket_fd, buffer+bytes_received, bytes_read_on_iteration);
		// 	if(nbr < 0) {
		// 		printf("Couldn't receive response %s\n", strerror(errno));
		// 	}
		// 	bytes_received+=nbr;
		// }
		printf("bytes_received=%d\n", bytes_received);

		int i = 0;
		char* dirname=malloc(MAX_PATH_LENGTH);
		assert(dirname != NULL);
		while(i < bytes_received) {
			struct stat st;
			memcpy(&st, buffer + i, sizeof(struct stat));
			print_stat(&st);			
			strcpy(dirname, buffer+i+sizeof(struct stat));
			printf("%s\n", dirname);
			i+=sizeof(struct stat)+strlen(dirname)+1;
			printf("%d\n", i);
			if((filler(buf, dirname, &st, 0)))
				break;
			printf("next iteration\n");
		}
	}

	free(buffer);
	free(msg);
	return -status;
}




static int raid1_access(const char *path, int mask) {
	printf("%s\n", "--------------------------------SYSCALL ACCESS");
	return 0;
	// log_msg(strcat("Syscall access; Path is ", path));

	// int num_bytes_to_send = 7 + strlen(path)+1 + sizeof(int);
	// if(write(socket_fd, &num_bytes_to_send, sizeof(int)) < 0) {
	// 	printf("%s\n", "Couldn't send num bytes");
	// 	log_error("GETATTR. Couldn't send num bytes", errno);
	// 	return -1;
	// }

	// char msg[8 + MAX_PATH_LENGTH + sizeof(int)]; //access;path;mask\0
	// sprintf(msg, "%s;%s", "access", path);
	// int mask_arg = mask;
	// memcpy(msg + 8 + strlen(path), &mask_arg, sizeof(int));
	// printf("Sending %s\n", msg);

	// if(write(socket_fd, msg, num_bytes_to_send) < 0) {
	// 	printf("%s\n", "ACCESS: Couldn't send message to server!");
	// }

	// char response [12];
	// if(read(socket_fd, response, strlen(response)) < 0) {
	// 	printf("%s\n", "ACCESS: Couldn't read response");
	// 	return -1;
	// }
	// int res = atoi(response);
	// printf("Received result %d\n", res);
	// return -res;
}


static int raid1_rename(const char* from, const char *to) {
	printf("%s\n", "-------------------------------- RENAME");
	int args_length = 7 + strlen(from)+ 1 + strlen(to)+1;
	printf("Args length %d\n", args_length);

	char* msg = malloc(sizeof(int) + args_length);
	assert(msg != NULL);

	memcpy(msg, &args_length, sizeof(int));
	sprintf(msg + sizeof(int), "rename;%s", from);
	printf("Part of message:%s\n", msg + sizeof(int));
	sprintf(msg + sizeof(int) + 7 + strlen(from) + 1, "%s", to);
	printf("Part of message:%s\n", msg + sizeof(int));
	printf("Other part of message:%s\n", msg + sizeof(int) + 7 + strlen(from) + 1);


	if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
		printf("%s\n", "Couldn't send message");
	} 

	int resp;
	if(read(socket_fd, &resp, sizeof(int)) <= 0) {
		printf("%s\n", "Couldn't read response");
	}
	printf("response is %d\n", resp);
	return -resp;
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


static int create_socket_connection(char* address_str, int timeout, int server_index) {
	int ip, port;
	
	server_sfds[server_index] = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sfds[server_index] < 0) {
		log_error("Couldn't acquire endpoint file descriptor", errno);
		return -1;
	}

	if(split_address(address_str, &ip, &port) < 0){
		log_error("Malformed server address", errno);
		return -1; //special error code?
	}

	server_addrs[server_index].sin_family = AF_INET;
	server_addrs[server_index].sin_port = htons(port);
	server_addrs[server_index].sin_addr.s_addr = ip;

	int conn_status = -1;
	time_t start = time(NULL);
	
	while(conn_status < 0 && (difftime(time(NULL), start) <= timeout)) {
		log_msg("Trying to connect to server");
		printf("%s\n", "Trying to connect to server");
		conn_status = connect(server_sfds[server_index], (struct sockaddr*)(&server_addrs[server_index]), sizeof(server_addrs[server_index]));
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
	num_servers = storage_info.num_servers;
	server_sfds = malloc(sizeof(int) * num_servers);
	assert(server_sfds != NULL);
	server_addrs = malloc(sizeof(struct sockaddr_in) * num_servers);
	assert(server_addrs != NULL);
	write_results = malloc(sizeof(int) * num_servers);
	assert(write_results != NULL);
	timeout = client_info.timeout;

	int server_index = 0;
	int i = 0;
	for(;i < storage_info.num_servers; i++) {
		if(create_socket_connection(storage_info.servers[i], client_info.timeout, server_index++) != 0) {
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