#include "server.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "../utils/communication_structs.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/xattr.h>
#include <openssl/sha.h>

// #include "hash.h"

#define BACKLOG 10
#define MAX_CLIENT_MSG_LENGTH 2048
#define MAX_ARGS_LENGTH 2000
#define HASH_ATTR_NAME "user.hash"



static void fullpath(char fpath[MAX_PATH_LENGTH], const char *path){
    strcpy(fpath, mountpoint);
    if(mountpoint[strlen(mountpoint) - 1] != '/' && path[0] != '/') {
        strncat(fpath, "/", MAX_PATH_LENGTH);
    } else if(mountpoint[strlen(mountpoint) - 1] == '/' && path[0] == '/') {
        strncat(fpath, path+1, MAX_PATH_LENGTH); // ridiculously long paths will break here
    } else {
        strncat(fpath, path, MAX_PATH_LENGTH); // ridiculously long paths will break here
    }
    
}


static void print_stat(struct stat* stbuf) {
    printf("st_dev %lu; st_ino %lu; st_mode %lu; st_uid %lu; st_gid %lu \n; st_rdev %lu; st_size %lu; st_atime %lu; st_mtime %lu; st_ctime %lu; st_blksize %lu; st_blocks %lu;\n", 
            (size_t)stbuf->st_dev, (size_t)stbuf->st_ino, (size_t)stbuf->st_mode, (size_t)stbuf->st_uid, 
            (size_t)stbuf->st_gid, (size_t)stbuf->st_rdev, (size_t)stbuf->st_size, (size_t)stbuf->st_atime, 
            (size_t)stbuf->st_mtime, (size_t)stbuf->st_ctime, (size_t)stbuf->st_blksize, (size_t)stbuf->st_blocks);
}


static char* get_path_from_args(char args[], int* path_length) {
	char path[MAX_PATH_LENGTH];
	strcpy(path, args);
	printf("path is %s\n", path);

	char* fpath = malloc(MAX_PATH_LENGTH);
	assert(fpath != NULL);
	fullpath(fpath, args);
    printf("Full path is %s length %lu\n", fpath, strlen(fpath));

    *path_length = strlen(path);

    return fpath;
}

static int write_dir_info(char* buf, struct dirent *de, int* buffer_size, int* bytes_filled){
	int dname_size = strlen(de->d_name)+1;
	if(*buffer_size < *bytes_filled + sizeof(struct stat) + dname_size) {
		*buffer_size = 2*(*buffer_size);
		buf = realloc(buf, *buffer_size);
		if(buf == NULL) return -1;
	}

	struct stat st;
	memset(&st, 0, sizeof(st));
	st.st_ino = de->d_ino;
	st.st_mode = de->d_type << 12;
	print_stat(&st);
	memcpy(buf+*bytes_filled, &st, sizeof(st));
	printf("%s\n", de->d_name);
	strcpy(buf+*bytes_filled + sizeof(st), de->d_name);

	*bytes_filled += (sizeof(st) + dname_size);
	printf("bytes_filled=%d\n", *bytes_filled);
	return 0;
}




static int send_buffer(int client_sfd, char* buf, int num_bytes_to_send) {
	int num_bytes_sent = 0;
	while(num_bytes_sent < num_bytes_to_send) {
		int sent;
		if((sent=write(client_sfd, buf, 1024)) < 0)
			break;
		num_bytes_sent += sent;
		printf("Bytes sent %d\n", sent);
	}
	return num_bytes_sent;
}




static int send_dir_info(int cfd, int status, struct stat* st, char* dirname) {
	int buffer_size = sizeof(int) + sizeof(st) + strlen(dirname) + 1;
	printf("buffer_size=%d\n", buffer_size);
	printf("dirname is %s\n", dirname);
	// int diname_length = strlen(dirname);
	char* buf = malloc(buffer_size);
	assert(buf!=NULL);
	memcpy(buf, &status, sizeof(int));
	memcpy(buf+sizeof(int), st, sizeof(st));
	// memcpy(buf + sizeof(st), &dirname_length, sizeof(int));
	strcpy(buf + sizeof(int) + sizeof(st), dirname);

	if(write(cfd, buf, buffer_size) <= 0) {
		printf("%s\n", strerror(errno));
	}
	return buffer_size;
}



//-------------------- HASH STUFF ---------------------------

// static size_t djb_hash(const char* string) {
// 	size_t hash = 5381;
//     while (*string) {
//         hash = 33 * hash ^ (unsigned char) *string++;
//     }
//     return hash;
// }

void print_hash(char* msg, unsigned char hash []) {
	printf("%s:", msg);
	int i;
	for(i = 0; i < SHA_DIGEST_LENGTH; i++)
		printf("%02x%c", hash[i], i < (SHA_DIGEST_LENGTH-1) ? ' ' : '\n');
}



static off_t get_file_size(const char* path) {
	struct stat st;
	off_t file_size;
	if(stat(path, &st) == 0) {
		return st.st_size;
	}
	return (off_t)-errno;
}


static int compute_file_hash(const char* path, unsigned char result []) {
	off_t file_size = get_file_size(path);
	if(file_size < 0) {
		return -file_size;
	}

	FILE* file = fopen(path, "r+");
	if(fseek(file, 0, SEEK_SET) != 0) {
		printf("fseek %s\n", strerror(errno));
		return -errno;
	}

	char* file_content = malloc(file_size * sizeof(char));
	assert(file_content != NULL);

	//using file_Size as number of elements might be wrong file_size/sizof(char) maybe?
	int elems_read = fread(file_content, sizeof(char), file_size, file);
	//add \0 ??
	printf("elems_read = %d\n", elems_read);
	printf("content:%s\n", file_content);
	if(elems_read != file_size) {
		free(file_content);
		return -errno;
	}

	// SHA1(file_content, strlen(file_content), result);

	SHA_CTX c;
	if(!SHA1_Init(&c)) {
		printf("SHA1_Init: %s\n", strerror(errno));
		return -errno;
	}

	if(!SHA1_Update(&c, file_content, file_size)) {
		printf("SHA1_Update: %s\n", strerror(errno));
		return -errno;
	}

	if(!SHA1_Final(result, &c)) {
		printf("SHA1_Final: %s\n", strerror(errno));
		return -errno;
	}


	print_hash("computed sha1", result);


	fclose(file);
	free(file_content);

	
	// unsigned char* result = malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char));
	// assert(result != NULL);
	// SHA1_wrapper(file_content, result);
	return 0;
}


static int hash_cmp(unsigned char h1[], unsigned char h2[]) {
	int i;
	for(i=0; i<SHA_DIGEST_LENGTH; i++) {
		if(h1[i] != h2[i])
			return 1;
	}
	return 0;
}

/*
 * Returns FILE_INTACT if hash attribute did not differ from file content hash;
 * FILE_CORRUPT if these hashes differ and -errno if any of the syscalls failed 
 */
static int check_hash(const char* path, unsigned char* hash) {
	int ret_val = -1;
	off_t file_size = get_file_size(path);
	if(file_size < 0)
		return -file_size;
	printf("file size is %lu\n", file_size);

	unsigned char h1[SHA_DIGEST_LENGTH];
	int res;
	if((res = compute_file_hash(path, h1)) == 0) {
		print_hash("hash1", h1);
		unsigned char h2 [SHA_DIGEST_LENGTH];
		size_t size = sizeof(unsigned char) * SHA_DIGEST_LENGTH;
		if(getxattr(path, HASH_ATTR_NAME, h2, size)  == size) {
			print_hash("hash2", h2);
			if(hash_cmp(h1, h2) == 0) {	
				printf("%s\n", "Yaay Hahses match");
				memcpy(hash, h1, SHA_DIGEST_LENGTH);
				ret_val = FILE_INTACT;
			} else {
				printf("%s\n", "Nooooo Hashes differ");
				ret_val = 0;
			}
		} else {
			ret_val = -errno;
			printf("getxattr: errno %d --> %s\n",errno, strerror(errno));
		}
	} else {
		ret_val = -res;
		printf("compute_file_hash: errno %d --> %s\n", res, strerror(res));
	}

	return ret_val;
}


static void update_hash(const char* path, int attr_exists) {
	// off_t file_size = get_file_size(path);
	// printf("file size is %lu\n", file_size);

	unsigned char hash[SHA_DIGEST_LENGTH];
	compute_file_hash(path, hash);
	print_hash("updating", hash);
	// printf("hash is %lu\n", hash);

	int flags = 0;//attr_exists ? XATTR_REPLACE : XATTR_CREATE;
	int n = setxattr(path, HASH_ATTR_NAME, &hash, sizeof(char) * SHA_DIGEST_LENGTH, flags);
	if(n < 0) printf("setxattr: %s\n", strerror(errno));
}




//========================= SYSCALL HANDLERS =========================

static int getattr_handler(int client_sfd, char path[]) {
	printf("%s\n", "--------------------GETATTR HANDLER");
	printf("passed args %s\n", path);
	char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s. length %lu\n", fpath, strlen(fpath));

    struct stat* stbuf = malloc(sizeof(struct stat));
    assert(stbuf != NULL);
    int res = lstat(fpath, stbuf);
  	int ret_val = 0;
    if(res == -1) {
    	ret_val = errno;
    	printf("stat error! %s\n", strerror(errno));
    }
    print_stat(stbuf);

    char response[sizeof(int) + sizeof(struct stat)];
    memcpy(response, &ret_val, sizeof(int));
    memcpy(response + sizeof(int), stbuf, sizeof(struct stat));

    if(write(client_sfd, response, sizeof(int) + sizeof(struct stat)) < 0) {
    	printf("Couldn't send response %s\n", strerror(errno));
    }
    printf("Response ret_val = %d\n", ret_val);

	return -ret_val;
}

static int rename_handler(int client_sfd, char args[]) {
	printf("%s\n", "--------------------RENAME HANDLER");
	char oldpath[MAX_PATH_LENGTH];
	strcpy(oldpath, args);
	printf("Old path %s\n", oldpath);
	char f_oldpath[MAX_PATH_LENGTH];
	fullpath(f_oldpath, oldpath);
    printf("Full path is %s\n", f_oldpath);

	char newpath[MAX_PATH_LENGTH];
	strcpy(newpath, args + strlen(oldpath) + 1);
	printf("New path %s\n", newpath);
	char f_newpath[MAX_PATH_LENGTH];
	fullpath(f_newpath, newpath);
    printf("Full path is %s\n", f_newpath);

	int res, ret_val;
	ret_val = 0;
	res = rename(f_oldpath, f_newpath);
	if (res == -1) {
		ret_val = errno;
		// return -errno;
	}

	if(write(client_sfd, &ret_val, sizeof(int)) <= 0) {
		printf("%s\n", "Couldn't send response");
	}

	return -ret_val;
}


static int mkdir_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------MKDIR HANDLER");
	char path[MAX_PATH_LENGTH];
	strcpy(path, args);
	printf("Path is %s\n", path);
	char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s length %lu\n", fpath, strlen(fpath));

	mode_t mode;
	memcpy(&mode, args + strlen(path) + 1, sizeof(mode_t));
	printf("mode is %d\n", mode);

	int res;
	int ret_val = 0;
	res = mkdir(fpath, mode);
	if (res == -1) {
		ret_val = errno;
	}
	printf("Response is %d\n", ret_val);

	if(write(client_sfd, &ret_val, sizeof(int)) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}

	return -ret_val;
}

static int rmdir_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------RMDIR HANDLER");

	printf("args are%s\n", args);
	char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, args);
    printf("Full path is %s length %lu\n", fpath, strlen(fpath));

    int res;
    int ret_val = 0;
	res = rmdir(fpath);
	if (res == -1) {
		ret_val = errno;
	}
	printf("Retval is %d\n", ret_val);

	if(write(client_sfd, &ret_val, sizeof(int)) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}
	return -ret_val;
}



static int open_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------OPEN HANDLER");
	char path[MAX_PATH_LENGTH];
	strcpy(path, args);
	printf("path is %s\n", path);

	char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, args);
    printf("Full path is %s length %lu\n", fpath, strlen(fpath));

    int flags;
    memcpy(&flags, args + strlen(path) + 1, sizeof(int));
    printf("flags are %d\n", flags);

    int fd;
    int status = 0;
    unsigned char hash [SHA_DIGEST_LENGTH];
    int file_intact = check_hash(fpath, hash);
    printf("file_intact = %d\n", file_intact);
    print_hash("Sending hash ", hash);   
    fd = open(fpath, flags);
	if (fd == -1) {
		printf("open returned -1 %s\n", strerror(errno));
		status = errno;
	}
	printf("status is %d,fd is %d\n", status, fd);

	int response_size = 3*sizeof(int) + SHA_DIGEST_LENGTH;
	char response[response_size];
	memcpy(response, &status, sizeof(int));
	memcpy(response + sizeof(int), &fd, sizeof(int));
	memcpy(response + 2*sizeof(int), &file_intact, sizeof(int));
	memcpy(response + 3*sizeof(int), hash, SHA_DIGEST_LENGTH);

	if(write(client_sfd, response, response_size) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}
	return -status;

	// close(res); ???????????????????
}


static int read_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------READ HANDLER");
	char path[MAX_PATH_LENGTH];
	strcpy(path, args);
	printf("path is %s\n", path);

	char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, args);
    printf("Full path is %s length %lu\n", fpath, strlen(fpath));

    size_t size;
    off_t offset;

    memcpy(&size, args + strlen(path) + 1, sizeof(size_t));
    memcpy(&offset, args + strlen(path) + 1 + sizeof(size_t), sizeof(off_t));
    printf("size is %lu, offset is %lu\n", size, offset);

    int fd, res;
	// int ret_val = 0;
	int bytes_read = -1;
	char* buf = malloc(size);
	assert(buf != NULL);
	fd = open(fpath, O_RDONLY);
	if (fd == -1){
		res = -errno;
		printf("fd is -1 %s\n", strerror(errno));
	} else {
		res = pread(fd, buf, size, offset);
		if (res == -1) {
			res = errno;
			printf("pread returned -1 %s\n", strerror(errno));
		}
		bytes_read = res;
		close(fd);
	}
	printf("res %d, bytes_read %d\n", res, bytes_read);

	char* response = malloc(2*sizeof(int) + size);
	assert(response != NULL);
	memcpy(response, &res, sizeof(int));
	memcpy(response + sizeof(int), &bytes_read, sizeof(int));
	memcpy(response+2*sizeof(int), buf, size);
	printf("content:%s\n", buf);
	if(write(client_sfd, response, 2*sizeof(int) + size) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}

	free(buf);
	free(response);
	return res;
}



static int write_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------WRITE HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);

    size_t size;
    off_t offset;
    int flags;

    memcpy(&size, args + path_length + 1, sizeof(size_t));
    memcpy(&offset, args + path_length + 1 + sizeof(size_t), sizeof(off_t));
    memcpy(&flags, args + path_length + 1 + sizeof(size_t) + sizeof(off_t), sizeof(int));
    printf("size is %lu, offset is %lu, flags is %d\n", size, offset, flags);

    char* buf = malloc(size);
    assert(buf != NULL);
    memcpy(buf, args+path_length+1+sizeof(size_t)+sizeof(off_t)+sizeof(int), size);

    int fd, res;
    // int ret_val = 0;
    int bytes_written = -1;

    fd = open(fpath, flags);
	if (fd == -1) {
		res = -errno;
		printf("open returned -1 %s\n", strerror(errno));
	} else {
		res = pwrite(fd, buf, size, offset);
		if (res == -1) {
			res = -errno;
			printf("pwrite returned -1 %s\n", strerror(errno));
		}
		bytes_written = res;
		close(fd);
	}
	printf("res %d, bytes_written %d\n", res, bytes_written);

	update_hash(fpath, 1);

	char* response = malloc(2*sizeof(int));
	assert(response != NULL);
	memcpy(response, &res, sizeof(int));
	memcpy(response + sizeof(int), &bytes_written, sizeof(int));
	if(write(client_sfd, response, 2*sizeof(int)) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}

	free(response);
	
	return res;
}


static int utime_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------UTIME HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);
	printf("%s\n", fpath);

	struct utimbuf* ubuf = malloc(sizeof(struct utimbuf));
	memcpy(ubuf, args + path_length + 1, sizeof(struct utimbuf));
	int res;
	int ret_val = 0;
	res = utime(fpath, ubuf);
	if(res == -1) {
		ret_val = errno;
	}
	printf("utime ret_val %d\n", ret_val);

	write(client_sfd, &ret_val, sizeof(int));
	free(fpath);
	free(ubuf);

	return -ret_val;
}


static int utimens_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------UTIMENS HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);
	printf("%s\n", fpath);

	struct timespec ts[2];
	memcpy(ts, args + path_length + 1, 2*sizeof(struct timespec));

	printf("ts[0].tv_sec=%lu; ts[0].tv_nsec / 1000=%lu; ts[1].tv_sec=%lu; ts[1].tv_nsec / 1000=%lu\n", 
		ts[0].tv_sec, ts[0].tv_nsec / 1000, ts[1].tv_sec, ts[1].tv_nsec / 1000);

	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(fpath, tv);
	if (res == -1){
		res = -errno;
		printf("%s\n", strerror(errno));
	}
	printf("result is %d\n", res);
	if(write(client_sfd, &res, sizeof(int))<= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}
	return res;
}


static int mknod_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------MKNOD HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);
	
	mode_t mode;
	dev_t rdev;
	printf("%s\n", fpath);
	memcpy(&mode, args+path_length+1, sizeof(mode_t));
	printf("%s\n", "HERE");
	memcpy(&rdev, args+path_length+1+sizeof(mode_t), sizeof(dev_t));
	printf("mode = %d, dev_t = %lu\n", mode, rdev);

	int res;
	int ret_val = 0;
	if (S_ISREG(mode)) {
		res = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(fpath, mode);
	else
		res = mknod(fpath, mode, rdev);
	if (res == -1) {
		ret_val = errno;
	}
	update_hash(fpath, 0);
	printf("returning %d\n", ret_val);
	if(write(client_sfd, &ret_val, sizeof(int)) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}
	return -ret_val;
}

static int unlink_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------UNLINK HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);

	int res;
	int ret_val = 0;
	res = unlink(fpath);
	if (res == -1)
		ret_val = errno;

	if(write(client_sfd, &ret_val, sizeof(int)) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}
	return -ret_val;
}


static int truncate_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------TRUNCATE HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);

	off_t size;
	memcpy(&size, args + path_length + 1, sizeof(off_t));
	printf("size=%lu\n", size);

	int res;

	res = truncate(fpath, size);
	if (res == -1)
		res = -errno;

	if(write(client_sfd, &res, sizeof(int)) <= 0) {
		printf("Couldn't send response %s\n", strerror(errno));
	}

	return res;
}



static int readdir_handler (int client_sfd, char args[]) {
	printf("%s\n", "--------------------READDIR HANDLER");
	int path_length;
	char* fpath = get_path_from_args(args, &path_length);

	DIR *dp;
	struct dirent *de;
	int status = 0;

	

	dp = opendir(fpath);
	if (dp == NULL) {
		status = errno;
	}
	printf("status=%d\n", status);


	int buf_size = sizeof(struct stat) * 16;
	char* buf = malloc(buf_size);
	assert(buf!=NULL);

	// memcpy(buf, &status, sizeof(int));	
	int bytes_filled = 2 * sizeof(int);
	int num_structs = 0;	//TODO: remove? for debug only?
	printf("Size of struct stat %lu\n", sizeof(struct stat));
	while ( (status == 0) && ((de = readdir(dp)) != NULL) ) {
		if(bytes_filled >= buf_size) {
			buf_size += buf_size;
			printf("Must realloc %d\n", buf_size);
			buf = realloc(buf, buf_size);
			if(buf == NULL) {
				printf("Couldn't realloc %s\n", strerror(errno));
				status = errno;
				break;
			}
		}	

		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		print_stat(&st);

		memcpy(buf + bytes_filled, &st, sizeof(struct stat));
		bytes_filled+=sizeof(struct stat);
		strcpy(buf + bytes_filled, de->d_name);
		printf("%s %lu\n", de->d_name, strlen(de->d_name));
		bytes_filled += strlen(de->d_name) + 1;
		num_structs++; //TODO: delete this variable
		printf("bytes_filled=%d; num_structs=%d\n", bytes_filled, num_structs);			
	}

	printf("status=%d\n", status);
	printf("Total bytes filled in %d\n", bytes_filled);

	// char* info = malloc(2 * sizeof(int));
	// memcpy(info, &status, sizeof(int));
	// memcpy(info + sizeof(int), &bytes_filled, sizeof(int));
	memcpy(buf, &status, sizeof(int));
	memcpy(buf + sizeof(int), &bytes_filled, sizeof(int));

	// if(write(client_sfd, buf, 2*sizeof(int)) <= 0) {
	// 	printf("Couldn't send info %s\n", strerror(errno));
	// }

	// if(status == 0) {
		int bytes_sent = 0;
		int nbs = write(client_sfd, buf, bytes_filled);
		printf("bytes_sent=%d\n", nbs);
	// }
	
	// free(info);
	free(buf);
	return -status;
}

//======================================================================================



static int read_from_client(int client_sfd, char* syscall, char* args) {
	char buf[MAX_CLIENT_MSG_LENGTH];
	int bytes_read = read(client_sfd, buf, MAX_CLIENT_MSG_LENGTH);
	if(bytes_read < 0) {
		return -1;
	}

	int info_length;
	memcpy(&info_length, buf, sizeof(int));
	printf("info length %d\n", info_length);
	char* info = malloc(info_length + 1);
	assert(info != NULL);
	memcpy(info, buf+sizeof(int), info_length);

	strcpy(syscall, strtok(buf + sizeof(int), ";"));
	assert(syscall != NULL);
	memcpy(args, strtok(NULL, ";"), info_length - strlen(syscall) - 1);

	return bytes_read;
}



static int client_handler(int client_sfd) {
	char buf[MAX_CLIENT_MSG_LENGTH];
	char* syscall = malloc(10);
    char* args = malloc(MAX_ARGS_LENGTH);
	while(1) {
		printf("%s\n", "--------------Client handler-------------");
		
		int num_bytes_read = read_from_client(client_sfd, syscall, args);
		printf("returned %d %s %s\n", num_bytes_read, syscall, args);
		if(num_bytes_read <= 0){
			printf("%s\n", "Gonna break while");
			break;
		}

		if(strcmp(syscall, "getattr") == 0) {
			getattr_handler(client_sfd, args);			
		} else if(strcmp(syscall, "rename") == 0) {
			rename_handler(client_sfd, args);
		} else if(strcmp(syscall, "mkdir") == 0) {
			mkdir_handler(client_sfd, args);
		} else if(strcmp(syscall, "rmdir") == 0) {
			rmdir_handler(client_sfd, args);
		} else if(strcmp(syscall, "open") == 0) {
			open_handler(client_sfd, args);
		} else if(strcmp(syscall, "read") == 0) {
			read_handler(client_sfd, args);
		} else if(strcmp(syscall, "write") == 0) {
			write_handler(client_sfd, args);
		} else if(strcmp(syscall, "mknod") == 0) {
			mknod_handler(client_sfd, args);
		} else if(strcmp(syscall, "utime") == 0) {
			utime_handler(client_sfd, args);
		} else if(strcmp(syscall, "readdir") == 0) {
			readdir_handler(client_sfd, args);
		} else if(strcmp(syscall, "unlink") == 0) {
			unlink_handler(client_sfd, args);
		} else if(strcmp(syscall, "truncate") == 0) {
			truncate_handler(client_sfd, args);
		} else if(strcmp(syscall, "utimens") == 0) {
			utimens_handler(client_sfd, args);
		}
	}
	free(syscall);
	free(args);
	close(client_sfd);
	return 0;
}


static int create_and_bind_socket(char* ip_str, char* port_str, struct sockaddr_in* addr) {
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	int ip;
	inet_pton(AF_INET, ip_str, &ip);
    int port;
	port = atoi(port_str);

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = ip;

	if(bind(sfd, (struct sockaddr *) addr, sizeof(struct sockaddr_in)) < 0) {
        printf("%s %s : %d\n", "Couldn't bind to ", ip_str, port);
        exit(-1);
    }
	return sfd;
}


int main(int argc, char const *argv[]){
	if(argc < 4) {
		printf("%s\n", "Usage : ./server *ip* *port* *mountpoint*");
		return 0;
	}

	char* ip_str = strdup(argv[1]);
	char* port_str = strdup(argv[2]);
	mountpoint = strdup(argv[3]);

	struct sockaddr_in addr, peer_addr;
	int sfd = create_and_bind_socket(ip_str, port_str, &addr);
	listen(sfd, BACKLOG);

	while (1) {
        int peer_addr_size = sizeof(struct sockaddr_in);
        int client_sfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
        printf("Accepted new socket connection %d\n", peer_addr.sin_addr.s_addr);

        switch(fork()) {
            case -1:
                exit(100);
            case 0:
                client_handler(client_sfd); //Child process handles client request and dies
                exit(0);
            default:
                break;
        }
    }
    close(sfd);

	return 0;
}