#include "raid1_fuse.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <openssl/sha.h>
// #include <sys/types.h>
// #include <sys/stat.h>

#include "log.h"
#include "../utils/communication_structs.h"

// #define MAX_PATH_LENGTH 200

#define READ_FAILED -1
#define WRITE_FAILED -2
#define HASH_UPDATE_SUCCESS 1
#define NUM_SERVERS 2

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

static int write_to_server(int index, const char* msg, int size) {
	write_results[index] = -1;
	read_results[index] = -1;
	//Might move to function read_from_server()
	// time_t start = time(NULL);	
	// do {
		write_results[index] = write(server_sfds[index], msg, size);
		printf("Server N %d, write_result = %d\n", index, write_results[index]);
		// if(write_results[i] <= 0) sleep(1);
	// } while(write_results[i] < 0 && (difftime(time(NULL), start) <= timeout));			

	if(write_results[index] <= 0) {
		printf("%s\n", "tavzeit dzala araa");
		char log_text [1024];
		sprintf(log_text, "Couldn't send data to storage %s.", raids[index].diskname);
		// log_error(log_text, errno);
		log_server_error(0, index, errno, "");
		return -1;
	}
	return 0;
}


static int read_from_server(int i, char* resp, int expected_size, int* actual_size) {
	// start = time(NULL);
	// do {
		read_results[i] = read(server_sfds[i], resp, expected_size);
		*actual_size = read_results[i];
		printf("Server N %d, read_result = %d\n", i, read_results[i]);
	// 	if(read_results[i] <= 0) sleep(0.5);
	// } while(read_results[i] <= 0 && (difftime(time(NULL), start) <= timeout));	

	if(read_results[i] <= 0) {
		printf("%s\n", "tavzeit dzala araa, ver chamevitanet");
		char log_text [1024];
		sprintf(log_text, "Operation not completed. Couldn't receive data from storage %s.", raids[i].diskname);
		// log_error(log_text, errno);
		log_server_error(0, i, errno, "");
		return -1;
	}
	return 0;
}



/*
 * Returns 0 on success and -1 if writing to server or
 * reading from it was not successful.
 */
static int communicate_with_server(int server_index, char* msg, const int size, char* resp, 
													const int expected_size, int* actual_size) {
	if(write_to_server(server_index, msg, size) < 0) {
		return -1;
	}

	if(read_from_server(server_index, resp, expected_size, actual_size) < 0) {
		return -1;
	}

	return 0;
}


static int communicate_with_all_servers(char* msg, int size, char* resp, int expected_size, int* actual_size) {
	int i;
	if(specific_server_index >= 0) {
		communicate_with_server(specific_server_index, msg, size, resp, expected_size, actual_size);
	} else {
		for(i=0; i<num_servers; i++) {
			if(communicate_with_server(i, msg, size, resp, expected_size, actual_size) < 0) {
				return -1;
			}
			// if(write_to_server(i, msg, size) < 0) {
			// 	return -1;
			// }

			// if(read_from_server(i, resp, expected_size, actual_size) < 0) {
			// 	return -1;
			// }
		}
	}
	
	return 0;
}

/*
 * Returns number of bytes read from server in response on success,
 * and -1 in case of connection failure
 */
static int communicate_with_available_server(char* msg, const int size, char* resp, 
																	const int expected_size) {
	int i = 0;
	int actual_size = -1;
	if(specific_server_index >= 0) {
		communicate_with_server(specific_server_index, msg, size, resp, expected_size, &actual_size);
	} else {
		for(i=0; i<num_servers; i++) {
			if(actual_size > 0) break;
			communicate_with_server(i, msg, size, resp, expected_size, &actual_size);
			
		}
	}
	
	printf("actual_size=%d\n", actual_size);
	return actual_size;
}



int raid1_releasedir(const char *path, struct fuse_file_info *fi){
    printf("%s\n", "--------------------------------RELEASEDIR");
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
	
	int resp_size = sizeof(int) + sizeof(struct stat);
	char resp_raw [resp_size];
	int bytes_read = communicate_with_available_server(msg, args_length + sizeof(int), resp_raw, resp_size);
	if(bytes_read < 0) {
		printf("%s\n", "ver chevitanet");
		return -1;
	}
	free(msg);

	//---------- parse response -----------------
	int ret_val;
	memcpy(&ret_val, resp_raw, sizeof(int));
	printf("received status ret_val=%d\n", ret_val);
	memcpy(stbuf, (struct stat*)(resp_raw + sizeof(int)), sizeof(struct stat));
	print_stat(stbuf);

	return -ret_val; 
}


void print_hash(char* msg, unsigned char hash []) {
	printf("%s:", msg);
	int i;
	for(i = 0; i < SHA_DIGEST_LENGTH; i++)
		printf("%02x%c", hash[i], i < (SHA_DIGEST_LENGTH-1) ? ' ' : '\n');
}

static int hash_cmp(unsigned char h1[], unsigned char h2[]) {
	int i;
	print_hash("cmp hash1", h1);
	print_hash("cmp hash2", h2);
	for(i=0; i<SHA_DIGEST_LENGTH; i++) {
		if(h1[i] != h2[i])
			return 1;
	}
	return 0;
}




/*
 * Returns negative value if reading/writing to servers fails and positive on success
 */
static int update_file(const char* path, int dest_server_index, off_t buffer_size) {
	
	char* buf = malloc(buffer_size * sizeof(char));
	assert(buf != NULL);
	struct fuse_file_info fi;
	if(raid1_read(path, buf, buffer_size, 0, &fi) < 0) {
		//TODO: log
		return READ_FAILED;
	}
	printf("Read from server:%s\n", buf);

	struct fuse_file_info;
	specific_server_index = dest_server_index;
	if(raid1_write(path, buf, buffer_size, 0, &fi) < 0) {
		//TODO: log
		return WRITE_FAILED;
	}

	char text [MAX_PATH_LENGTH + 64];
	sprintf(text, "%s %s", path, "File updated successfully");
	log_server_error(0, dest_server_index, 0, text);
	return HASH_UPDATE_SUCCESS;
}


static int timespec_cmp(const struct timespec t1, const struct timespec t2) {
	if (t1.tv_sec == t2.tv_sec)
        return t1.tv_nsec > t2.tv_nsec;
    else
        return t1.tv_sec > t2.tv_sec;
}


static int update_old_file(const char* path) {
	int i;
	struct stat stats[NUM_SERVERS];
	for(i = 0; i<NUM_SERVERS; i++) {
		memset(&stats[i], 0, sizeof(struct stat));
		specific_server_index = i;
		if(raid1_getattr(path, &stats[i]) < 0) {
			return -1;
		}
	}
	specific_server_index = (timespec_cmp(stats[0].st_mtim, stats[1].st_mtim) > 0) ? 0 : 1; 
	int dest_server_index = (specific_server_index == 0) ? 1 : 0;
	return update_file(path, dest_server_index, stats[specific_server_index].st_size);
}


/*
 * Returns 0 if no servers needed to be updated due to file corruption
 * a positive number if server was updated successfully and -1 if 
 * server couldn't be updated.
 */
static int check_file_hashes(const char* path, const char* msg, int msg_size, char* resp,
												int expected_size, int* actual_size) {
	unsigned char* hashes[NUM_SERVERS];
	int hash_matching_statuses [NUM_SERVERS];
	int statuses [NUM_SERVERS];
	int fds [NUM_SERVERS];
	int i;

	for(i=0; i<NUM_SERVERS; i++) {
		hashes[i] = malloc(SHA_DIGEST_LENGTH);
		assert(hashes[i] != NULL);
		//read_from_server will only be executed if write_to_server returns 0;
		if((write_to_server(i, msg, msg_size) != 0) || 
						(read_from_server(i, resp, expected_size, actual_size) != 0)) {
			free(hashes[i]); //hashes[0] might not be freed and that's bad!!!
			return -1;
		}
		memcpy(&statuses[i], resp, sizeof(int));
		memcpy(&fds[i], resp + sizeof(int), sizeof(int));
		memcpy(&hash_matching_statuses[i], resp + 2*sizeof(int), sizeof(int));
		memcpy(hashes[i], resp + 3*sizeof(int), SHA_DIGEST_LENGTH);
		printf("hash_matching_statuses[%d] = %d\n", i, hash_matching_statuses[i]);
		print_hash("check_file_hashes", hashes[i]);
	}

	for(i=0; i<NUM_SERVERS; i++) {
		if(hash_matching_statuses[i] != FILE_INTACT) {
			printf("File corrupted on server %d\n", i);
			char err_str[MAX_PATH_LENGTH + 64];
			sprintf(err_str, "%s - %s", path, "File corrupt");
			log_server_error(0, i, 0, err_str);
			specific_server_index = (i == 0) ? 1 : 0;//(num_servers + (i-1)%num_servers)%num_servers; 
			printf("current server index %d, source server index %d\n", i, specific_server_index);
			if(hash_matching_statuses[specific_server_index] != FILE_INTACT) {
				printf("hash_matching_statuses[%d]=%d\n", specific_server_index, hash_matching_statuses[specific_server_index]);
				printf("server %d was corrupt too\n", i);
				sprintf(err_str, "%s - %s", path, "File corrupt. Unable to repaire file");
				log_server_error(0, specific_server_index, 0, err_str);
				return -1;
			} else {
				struct stat stbuf;
				raid1_getattr(path, &stbuf);
				printf("file_size %lu\n", stbuf.st_size);
				int update_res = update_file(path, i, stbuf.st_size);
				//Return back to normal (we no longer have one specific server we want to communicate to)
				specific_server_index = -1;
				return update_res;
			}
		}
	}

	if(hash_cmp(hashes[0], hashes[1]) != 0) {
		printf("Files are intact, but different\n");
		//TODO: get the latest version to the older one 
		update_old_file(path);
		specific_server_index = -1;
	}

	return 0;
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
	int response_size = 3*sizeof(int) + SHA_DIGEST_LENGTH;
	char response[response_size];
	int actual_size;
	int check_hashes_res = check_file_hashes(path, msg, args_length + sizeof(int), response, 
							response_size, &actual_size);
	printf("check_hashes_res=%d; actual_size=%d\n", check_hashes_res, actual_size);
	if(check_hashes_res < 0) {
		//TODO: log
		return -1;
	}
	int status, fd, file_intact;
	// int response_size = 3*sizeof(int);
	// char response[response_size];
	// if((available_sfd = communicate_with_available_server(msg, args_length + sizeof(int),
	// 																 response, response_size)) <= 0) {
	// 	printf("Couldn't send message %s\n", strerror(errno));
	// }

	memcpy(&status, response, sizeof(int));
	memcpy(&fd, response + sizeof(int), sizeof(int));
	memcpy(&file_intact, response + 2*sizeof(int), sizeof(int));
	printf("status is %d, fd is %d, file_intact is %d\n", status, fd, file_intact);
	fi->fh = fd;

	free(msg);
	return -status;
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

	int response, bytes_received;
	if(communicate_with_all_servers(msg, sizeof(int) + args_length, (char*)&response, sizeof(int), &bytes_received) < 0) {
		printf("%s\n", "Oops");
		return -1;
	}
	printf("num_bytes = %d; server responded with %d\n", bytes_received, response);
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

	int response, bytes_received;

	if(communicate_with_all_servers(msg, sizeof(int) + args_length, (char*)&response, sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno; //TODO: ?????????? what should i return in this case
	}
	printf("Server responded with %d\n", response);
	free(msg);
	return -response;
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

	memcpy(msg + bytes_written, &size, sizeof(size_t));
	memcpy(msg + bytes_written + sizeof(size_t), &offset, sizeof(off_t));

	int resp_size = 2*sizeof(int) + size;
	char* response = malloc(resp_size);
	assert(response != NULL);
	int bytes_received;

	if((bytes_received = communicate_with_available_server(msg, args_length + sizeof(int), 
																		response, resp_size)) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
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
	int args_length = 6 + strlen(path) + 1 + sizeof(size_t) + sizeof(off_t) + sizeof(int) + size;
	int bytes_written, flags;

	char* msg = fill_in_basic_info(args_length, "write;", path, &bytes_written);
	printf("Part of the msg %s\n", msg + sizeof(int));

	flags = (specific_server_index >= 0) ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY);
	printf("flags=%d\n", flags);

	memcpy(msg + bytes_written, &size, sizeof(size_t));
	memcpy(msg + bytes_written + sizeof(size_t), &offset, sizeof(off_t));
	memcpy(msg + bytes_written + sizeof(size_t) + sizeof(off_t), &flags, sizeof(int));
	memcpy(msg + bytes_written + sizeof(size_t) + sizeof(off_t) + sizeof(int), buf, size);

	char* response = malloc(2*sizeof(int));
	assert(response != NULL);

	int bytes_received;
	if(communicate_with_all_servers(msg, args_length+sizeof(int)+size, response, 
															2*sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
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

	int response, bytes_received;
	if(communicate_with_all_servers(msg, bytes_written + sizeof(mode_t) + sizeof(dev_t), (char*)&response, sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
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

	int response, bytes_received;
	if(communicate_with_all_servers(msg, bytes_written + sizeof(struct utimbuf), (char*)(&response), sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
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

	int response, bytes_received;
	if(communicate_with_all_servers(msg, bytes_written + sizeof(struct utimbuf), (char*)(&response), sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
	}

	printf("response is %d\n", response);
	return response;
}

static int raid1_unlink(const char *path) {
	printf("%s\n", "-------------------------------- UNLINK");
	int args_length = 7 + strlen(path) + 1;
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "unlink;", path, &bytes_written);

	int response, bytes_received;
	if(communicate_with_all_servers(msg, sizeof(int) + bytes_written, (char*)(&response), sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
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

	int response, bytes_received;
	if(communicate_with_all_servers(msg, sizeof(int) + args_length, (char*)(&response), sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
	}
	printf("resp=%d\n", response);

	return response;
}



static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	printf("%s\n", "-------------------- READDIR");
	int args_length = 8 + strlen(path) + 1;
	int bytes_written;
	char* msg = fill_in_basic_info(args_length, "readdir;", path, &bytes_written);//malloc(sizeof(int) + args_length); //

	int bytes_received, status, bytes_to_read;
	int buffer_size = 2 * sizeof(int) + 8000;
	char* buffer = malloc(buffer_size);
	assert(buffer != NULL);
	if((bytes_received = communicate_with_available_server(msg, args_length + sizeof(int), 
																	buffer, buffer_size)) <= 0) {
		printf("Couldn't send message %s\n", strerror(errno));
	}
	// printf("available_sfd = %d\n", available_sfd);
	memcpy(&status, buffer, sizeof(int));
	memcpy(&bytes_to_read, buffer+sizeof(int), sizeof(int));
	printf("status=%d; bytes_to_read=%d\n", status, bytes_to_read);
	sleep(0.5);
	if(status == 0) {
		//Reading with one read might be a bad idea, as data might be pretty big 
		//for big dig directories
		int i = 2 * sizeof(int);
		char* dirname=malloc(MAX_PATH_LENGTH);
		assert(dirname != NULL);
		while(i < bytes_to_read) {
			struct stat st;
			memcpy(&st, buffer + i, sizeof(struct stat));
			print_stat(&st);			
			strcpy(dirname, buffer + i+sizeof(struct stat));
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

	int response, bytes_received;
	if(communicate_with_all_servers(msg, sizeof(int) + args_length, (char*)(&response), sizeof(int), &bytes_received) < 0) {
		printf("Oops! bytes_received = %d\n", bytes_received);
		return -errno;
	}

	// if(write(socket_fd, msg, sizeof(int) + args_length) <= 0) {
	// 	printf("%s\n", "Couldn't send message");
	// } 

	// int resp;
	// if(read(socket_fd, &resp, sizeof(int)) <= 0) {
	// 	printf("%s\n", "Couldn't read response");
	// }
	printf("response is %d\n", response);
	return -response;
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
	read_results = malloc(sizeof(int) * num_servers);
	assert(read_results != NULL);
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
	//initialise global variable
	specific_server_index = -1;

	return fuse_main(3, fuse_args, &raid1_operations, NULL);
}