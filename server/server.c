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

#define BACKLOG 10
#define MAX_CLIENT_MSG_LENGTH 2048
#define MAX_ARGS_LENGTH 2000


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


static int getattr_handler(int client_sfd, char path[]) {
	printf("%s\n", "--------------------GETATTR HANDLER");
	printf("passed args %s\n", path);
	char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s\n", fpath);

    struct stat* stbuf = malloc(sizeof(struct stat));
    assert(stbuf != NULL);
    int res = stat(path, stbuf);
  	int ret_val = 0;
    if(res == -1) {
    	ret_val = errno;
    	printf("stat error %s\n", strerror(errno));
    }
    print_stat(stbuf);

    char response[sizeof(int) + sizeof(struct stat)];
    memcpy(response, &ret_val, sizeof(int));
    memcpy(response + sizeof(int), stbuf, sizeof(struct stat));

    if(write(client_sfd, response, sizeof(int) + sizeof(struct stat)) < 0) {
    	printf("Couldn't send response %s\n", strerror(errno));
    }

	return -ret_val;
}





static int read_from_client(int client_sfd, char* syscall, char* args) {
	char buf[MAX_CLIENT_MSG_LENGTH];
	int bytes_read = read(client_sfd, buf, MAX_CLIENT_MSG_LENGTH);
	if(bytes_read < 0) {
		return -1;
	}
	
	printf("buffer is %s\n", buf + sizeof(int));

	int info_length;
	memcpy(&info_length, buf, sizeof(int));
	printf("Number of bytes of info %d\n", info_length);
	char* info = malloc(info_length + 1);
	assert(info != NULL);
	memcpy(info, buf+sizeof(int), info_length);
	buf[sizeof(int)+info_length] = '\0';
	printf("the rest of the buffer %s\n", buf + sizeof(int));

	strcpy(syscall, strtok(buf + sizeof(int), ";"));
	assert(syscall != NULL);
	printf("Syscall is %s\n", syscall);
	strcpy(args, strtok(NULL, ";"));
	// memcpy(args, buf + sizeof(int) + strlen(syscall) + 1, info_length - (sizeof(int) + strlen(syscall)));
	printf("args %s\n", args);
	printf("Bytes read %d\n", bytes_read);
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
			printf("%s\n", "calling getattr handler");
			getattr_handler(client_sfd, args);
			
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