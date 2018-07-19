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
// #include "../utils/communication_structs.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <assert.h>

#define BACKLOG 10
#define MAX_CLIENT_MSG_LENGTH 4096
// #define MAX_PATH_LENGTH 2048




static void fullpath(char fpath[MAX_PATH_LENGTH], const char *path)
{
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


static int read_from_buffer(int cfd, char buf[], int length) {
    int data_size;
    int bytes_received = 0;
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
    }
    return bytes_received;
}


static int getattr_handler(int cfd) {
    char* path = strtok(NULL, ";"); //might need to use strtok_r and pass string to tokenize as an argument
    assert(path != NULL);
    
    char fpath[MAX_PATH_LENGTH];
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
    char* path = strtok(NULL, ";"); //might need to use strtok_r and pass string to tokenize as an argument
    assert(path != NULL);
    
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("%s\n", fpath);

    char* flags_str = strtok(NULL, ";");
    assert(flags_str != NULL);
    int flags = atoi(flags_str); //check?

    int fd = open(path, flags);
    int ret_val = 0;
    if (fd == -1) {
       ret_val = errno;
    }

    char resp [21]; //length of two ints
    sprintf(resp, "%d;%d", ret_val, fd);
    printf("OPENDIR: Sending back %s\n", resp);
    if(write(cfd, &resp, strlen(resp) + 1) < 0) {
        printf("%s\n", "OPENDIR: Couldn't send response");
    }

    return ret_val;
}



static int readdir_handler(int cfd) {
    char* path = strtok(NULL, ";"); //might need to use strtok_r and pass string to tokenize as an argument
    assert(path != NULL);    
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("%s\n", fpath);

    char* offset_str = strtok(NULL, ";");
    if(offset_str == NULL) {
        char err_resp[100] = "-1;Malformed message";
        write(cfd, &err_resp, strlen(err_resp) + 1);
        return -1;
    }
    int offset = atoi(offset_str);

    DIR *dp;
    struct dirent *de;

    dp = opendir(fpath);
    int opendir_status = 0;
    if (dp == NULL) {
        // char err_resp[100]; 
        // sprintf(err_resp, "%d;%s", errno, "Error on opendir");
        opendir_status = errno;
        printf("READDIR_HANDLER: error %d\n", err_resp);
        write(cfd, &err_resp, strlen(err_resp));
        return -errno;
    }

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        printf("st.st_ino = %lu;  st.st_mode = %d;  de->d_type = %d; de_.de_name = %s\n", 
                                        (size_t)st.st_ino, st.st_mode, de->d_type, de->d_name);
        
        if(write(cfd, de->d_name, strlen(de->d_name) + 1) < 0) {
            printf("%s\n", "READDIR_HANDLER: Couldn't send de->dname");
            return -errno; //TODO: something else
        }

        if(write(cfd, &st, sizeof(st)) < 0) {
            printf("%s\n", "READDIR_HANDLER: Couldn't send struct stat");
            return -errno; //TODO: something else
        }
    }

    if(write(cfd, "/", 2) < 0) { // sending "/" indicates that reading the directory is over 
        printf("%s\n", "READDIR_HANDLER: Couldn't send end of dir indicator '/'");
        return -errno; //TODO: something else
    }

    closedir(dp);
}


static void client_handler(int cfd) {
    char msg[MAX_CLIENT_MSG_LENGTH];
    while(1) {
        printf("%s\n", "---------Inside client handler. Waiting for message from the client...--------");
        read_from_buffer(cfd, msg, MAX_CLIENT_MSG_LENGTH);
        printf("%s\n",msg);
        char* syscall = strtok(msg, ";");
        assert(syscall != NULL); //When client dies, some message is sent on socket close(?) and assertion fails
        printf("Received syscall %s\n", syscall);
        if(syscall == NULL || strcmp(syscall, "0") == 0) {
            break;
        } else if(strcmp(syscall, "getattr") == 0) {
            getattr_handler(cfd);
        } else if(strcmp(syscall, "opendir") == 0) {
            opendir_handler(cfd);
        } else if(strcmp(syscall, "readdir") == 0) {
            readdir_handler(cfd);
        }
    }
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
	addr.sin_addr.s_addr = ip;//htonl(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
        printf("%s %s : %d\n", "Couldn't bind to ", ip_str, port);
        exit(-1);
    }
	
    listen(sockfd, BACKLOG);

	while (1) 
    {
        int peer_addr_size = sizeof(struct sockaddr_in);
        cfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
        printf("Accepted new socket connection %d\n", peer_addr.sin_addr.s_addr);

        switch(fork()) {
            case -1:
                exit(100);
            case 0:
                // close(sockfd);
                client_handler(cfd); //Child process handles client request and dies
                exit(0);
            default:
                break;
                // close(cfd);
        }
    }
    close(sockfd);

	return 0;
}


