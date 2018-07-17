#include "server.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <assert.h>

#define BACKLOG 10
#define MAX_CLIENT_MSG_LENGTH 1024
#define PATH_MAX 200




static void fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, mountpoint);
    if(mountpoint[strlen(mountpoint) - 1] != '/' && path[0] != '/') {
        strncat(fpath, "/", PATH_MAX);
    } else if(mountpoint[strlen(mountpoint) - 1] == '/' && path[0] == '/') {
        strncat(fpath, path+1, PATH_MAX); // ridiculously long paths will break here
    } else {
        strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
    }
    
}

static void print_stat(struct stat* stbuf) {
    printf("st_dev %lu; st_ino %lu; st_mode %lu; st_uid %lu; st_gid %lu \n; st_rdev %lu; st_size %lu; st_atime %lu; st_mtime %lu; st_ctime %lu; st_blksize %lu; st_blocks %lu;\n", 
            (size_t)stbuf->st_dev, (size_t)stbuf->st_ino, (size_t)stbuf->st_mode, (size_t)stbuf->st_uid, 
            (size_t)stbuf->st_gid, (size_t)stbuf->st_rdev, (size_t)stbuf->st_size, (size_t)stbuf->st_atime, 
            (size_t)stbuf->st_mtime, (size_t)stbuf->st_ctime, (size_t)stbuf->st_blksize, (size_t)stbuf->st_blocks);
}


static int read_from_buffer(int cfd, char buf[], int length) {
    int data_size;
    int bytes_received = 0;
    // printf("%s\n", "Entered read_from_buffer");
    while (1) {
        data_size = read (cfd, buf, length);
        printf("%s %d\n", buf, data_size);
        int exit = 0;
        int i=0;
        for(; i<length; i++) {
            printf("%c\n", buf[i]);
            if(buf[i] == '\0'){
                exit = 1;
                break;
            }
        }

        if (data_size <= 0 || exit > 0)
            break;
        bytes_received += data_size;
        //write (cfd, &buf, data_size);
    }
    return bytes_received;
}


static int getattr_handler(int cfd) {
    char* path = strtok(NULL, ";"); //might need to use strtok_r and pass string to tokenize as an argument
    assert(path != NULL);
    
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    struct stat* stbuf = malloc(sizeof(struct stat));
    assert(stbuf != NULL);
    int res = stat(path, stbuf);

    int ret_val = 0;
    if(res < 0) {
        ret_val = errno; 
        printf("getattr_handler: %s: %s\n", fpath, strerror(ret_val));       
    }

    //Send status code
    uint32_t resp = htonl(ret_val);
    printf("sending %d converted to  %d\n", ret_val, resp);
    write(cfd, &resp, sizeof(resp));

    //Send struct stat
    print_stat(stbuf);
    if(write(cfd, stbuf, sizeof(struct stat)) < 0) {
        printf("%s\n", "Couldn't send struct stat stbuf ");
    }
    return -resp;
}


static int opendir_handler(int cfd) {

    return 0;
}


static void client_handler(int cfd) {
    char msg[10];
    printf("%s\n", "Entered client handler");
    read_from_buffer(cfd, msg, MAX_CLIENT_MSG_LENGTH);
    printf("%s\n",msg);
    char* syscall = strtok(msg, ";");
    assert(syscall != NULL);
    if(strcmp(syscall, "getattr") == 0) {
        getattr_handler(cfd);
    } else if(strcmp(syscall, "opendir") == 0) {
        opendir_handler(cfd);
    }

    // if(strcmp(command, "getattr") == 0) {
    //     printf("%s\n", "Must receive path");
    //     char path[1024];
    //     if(read_from_buffer(cfd, path, 1024)) {
    //         printf("%s\n", path);
    //     }
    // } 
    close(cfd);
}



int main(int argc, char const *argv[])
{
	if(argc < 4) {
		printf("%s\n", "Usage : ./server *ip* *port* *mountpoint*");
		return 0;
	}

	char* ip_str = strdup(argv[1]);
	char* port_str = strdup(argv[2]);
	mountpoint = strdup(argv[3]);

	int sockfd, cfd;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int ip;
	inet_pton(AF_INET, ip_str, &ip);
    int port;
	port = atoi(port_str);
    
	//specify address and port
	struct sockaddr_in addr, peer_addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
        printf("%s %s : %d\n", "Couldn't bind to ", ip_str, port);
        exit(-1);
    }
	
    listen(sockfd, BACKLOG);

	while (1) 
    {
        int peer_addr_size = sizeof(struct sockaddr_in);
        cfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);

        switch(fork()) {
            case -1:
                exit(100);
            case 0:
                // close(sockfd);
                client_handler(cfd);
                exit(0);
            default:
                break;
                // close(cfd);
        }
    }
    close(sockfd);

	return 0;
}


