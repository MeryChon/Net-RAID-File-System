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
#define MAX_CLIENT_MSG_LENGTH 4096
// #define MAX_PATH_LENGTH 2048




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

//TODO: Should be moved to utils????
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
        if(data_size < 0) {
            printf("read_from_buffer : %s\n", strerror(errno));
            return -errno;
        }
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



static int getattr_handler(int cfd, char* parameters) {
    printf("%s\n", "=======================GETATTR_HANDLER");
    char* path = strtok_r(parameters, ";", &parameters); //might need to use strtok_r and pass string to tokenize as an argument
    assert(path != NULL);
    
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s\n", fpath);

    struct stat* stbuf = malloc(sizeof(struct stat));
    assert(stbuf != NULL);
    int res = stat(path, stbuf);

    int ret_val = 0;
    if(res  == -1) {
        ret_val = ENOENT; 
        printf("getattr_handler: %s: %s\n", fpath, strerror(ret_val));       
    }

    //Send status code
    // uint32_t resp = htonl(ret_val);
    char resp [2*sizeof(int)];
    memcpy(resp, &ret_val, sizeof(int));
    int error_code = errno;
    memcpy(resp + sizeof(int), &error_code, sizeof(int));
    printf("sending %d and error_code %d \n", ret_val, error_code);
    write(cfd, resp, 2* sizeof(int));

    //Send struct stat
    print_stat(stbuf);
    if(write(cfd, stbuf, sizeof(struct stat)) < 0) {
        printf("%s\n", "Couldn't send struct stat stbuf ");
    }
    return -ret_val;
}


static int opendir_handler(int cfd, char* parameters) {
    printf("%s\n", "=======================OPENDIR_HANDLER");
    char* path = strtok_r(parameters, ";", &parameters); 
    assert(path != NULL);
    
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s\n", fpath);

    // char* flags_str = strtok_r(parameters, ";", &parameters);
    // assert(flags_str != NULL);
    // int flags = atoi(flags_str); //check?
    // printf("Flags are %d\n", flags);

    // int fd = open(path, flags);
    // int ret_val = 0;
    // if (fd == -1) {
    //    ret_val = errno;
    // }

    // char resp [21]; //length of two ints
    // sprintf(resp, "%d;%d", ret_val, fd);
    // printf("OPENDIR: Sending back %s\n", resp);
    // if(write(cfd, &resp, strlen(resp) + 1) < 0) {
    //     printf("%s\n", "OPENDIR: Couldn't send response");
    // }
    int ret_val = 0;
    DIR *dp;

    dp = opendir(fpath);

    char status_str[12];
    // sprintf(status_str, "%d", 0);
    if(dp == NULL) {
        ret_val = errno;
    }

    sprintf(status_str, "%d", ret_val);
    printf("Sending back status string %s\n", status_str);


    if(write(cfd, status_str, strlen(status_str) + 1) < 0) {
        printf("Couldn't send status %s\n", strerror(errno));
    }

    if(ret_val == 0) {
        intptr_t res = (intptr_t)dp;
        // printf("sending back %p\n", res);
        if(write(cfd, &res, sizeof(intptr_t)) < 0) {
            printf("Couldn't send DIR %s\n", strerror(errno));
        }
    }
    

    return -ret_val;
}



static int open_handler(int cfd, char* args) {
    printf("%s\n", "=======================OPEN_HANDLER");
    printf("Length of path %lu\n", strlen(args));
    char* path = malloc(strlen(args) + 1);
    strcpy(path, args);
    printf("%s\n", path);
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s\n", fpath);


    int flags;
    memcpy(&flags, args + strlen(path) + 1, sizeof(int));
    printf("Flags : %d\n", flags);

    int res;
    int ret_val = 0;
    res = open(fpath, flags);
    if (res == -1) {
        ret_val = errno;
        // return -errno;
    }

    if(write(cfd, &ret_val, sizeof(int)) < 0) {
        printf("%s\n", "Couldn't send response");
    }

    close(res);
    return -ret_val;
}


static int send_readdir_reply(int cfd, struct dirent* de, int has_next_dir) {
    int has_next = has_next_dir;
    char buf[1024];
    memcpy(buf, &has_next, sizeof(int));
    // sprintf(has_next_str, "%d", has_next);

    printf("has_next = %d\n", has_next);
    if(write(cfd, buf, sizeof(int)) < 0) {
        printf("Couldn't send has_next: %s\n", strerror(errno));
    }
    // free(has_next_str);
    if(has_next) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        print_stat(&st);
        if(write(cfd, &st, sizeof(struct stat)) < 0) {
            printf("Couldn't send struct stat %s\n", strerror(errno));
            return -errno;
        }

        printf("Dirent->d_name is %s\n", de->d_name);
        if(write(cfd, de->d_name, strlen(de->d_name) + 1) < 0) {
            printf("Couldn't send dirname %s\n", strerror(errno));
            return -errno;
        }
    }
    return 0;
}



static int readdir_handler(int cfd, char* parameters) {
    printf("%s\n", "=======================READDIR_HANDLER");
    char* path = strtok_r(parameters, ";", &parameters);
    assert(path != NULL);    
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s\n", fpath);

    /*char* offset_str = strtok(NULL, ";");
    char err_resp[100];
    if(offset_str == NULL) {
        strcpy(err_resp, "-1;Malformed message");
        write(cfd, &err_resp, strlen(err_resp) + 1);
        return -1;
    }
    int offset = atoi(offset_str); */

    DIR *dp;
    struct dirent *de;

    dp = opendir(fpath);
    int opendir_status = 0;
    if (dp == NULL) {
        opendir_status = errno;
        printf("READDIR_HANDLER: error %s\n", strerror(errno));
    }

    char status_resp [12];
    sprintf(status_resp, "%d",  opendir_status);
    printf("READDIR_HANDLER: Sending status %s \n", status_resp);
    if(write(cfd, status_resp, strlen(status_resp)) < 0) {
        printf("READDIR_HANDLER: after opendir %s\n", strerror(errno));
    }
    if(opendir_status != 0)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        send_readdir_reply(cfd, de, 1);        
    }
    send_readdir_reply(cfd, NULL, 0);

    closedir(dp);
    return 0;
}


static int access_handler(int cfd, char* parameters) {
    printf("%s\n", "=======================ACCESS_HANDLER");
    char* path = strtok_r(parameters, ";", &parameters); //might need to use strtok_r and pass string to tokenize as an argument
    assert(path != NULL);    
    char fpath[MAX_PATH_LENGTH];
    fullpath(fpath, path);
    printf("Full path is %s\n", fpath);

    char* mask_str = strtok_r(parameters, ";", &parameters);
    int mask = atoi(mask_str);
    printf("Mask is %d\n", mask);

    int res = access(path, mask);
    int ret_val = 0;
    if(res == -1) {
        ret_val = errno;
        printf("ACCESS HANDLER: %s\n", strerror(errno));
    }
    // ret_val = 0;

    char retval_str[12];
    sprintf(retval_str, "%d", ret_val);
    if(write(cfd, retval_str, strlen(retval_str) + 1) < 0) {
        printf("%s\n", "Couldn't send response to client");
    }
    return ret_val;
}



static int mkdir_handler(int cfd) {
    printf("%s\n", "=======================MKDIR_HANDLER");
    char path[MAX_PATH_LENGTH + 1];
    if(read(cfd, path, MAX_PATH_LENGTH) < 0) {
        printf("Couldn't read path %s\n", strerror(errno));
    }


    mode_t mode;
    if(read(cfd, &mode, sizeof(mode_t)) < 0) {
        printf("Couldn't read mode %s\n", strerror(errno));
    }

    int res;
    res = mkdir(path, mode);
    if (res == -1) {
        printf("on mkdir %s\n", strerror(errno));
        return -errno;
    }

}

static int mknod_handler(int cfd, char* args) {
    printf("%s\n", "=======================MKNOD_HANDLER");
    printf("Length of path %lu\n", strlen(args));
    char* path = malloc( strlen(args) + 1);
    strcpy(path, args);
    printf("%s\n", path);

}



static void client_handler(int cfd) {
    char msg[MAX_CLIENT_MSG_LENGTH];
    while(1) {
        printf("%s\n", "---------Inside client handler. Waiting for message from the client...--------");
        int bytes_to_read;
        if(read(cfd, &bytes_to_read, sizeof(int)) < 0) {
            printf("%s\n", "Couldn't read bytes to read");
        }
        
        printf("Bytes to read %d\n", bytes_to_read);
        int bytes_read = read_from_buffer(cfd, msg, bytes_to_read);
        if(bytes_read < 0) {
            // char err_resp [12];
            // sprintf(err_resp, "%d;", errno);
            // write(cfd, &err_resp, sizeof(err_resp));
            break;
        }
        printf("Received message %s bytes %d\n",msg, bytes_read);
        char* rest = msg;
        char* syscall = strtok_r(msg, ";", &rest);
        assert(syscall != NULL); //When client dies, some message is sent on socket close(?) and assertion fails
        printf("Received syscall %s\n", syscall);
        if(syscall == NULL || strcmp(syscall, "0") == 0) {
            break;
        } else if(strcmp(syscall, "getattr") == 0) {
            getattr_handler(cfd, rest);
        } else if(strcmp(syscall, "opendir") == 0) {
            opendir_handler(cfd, rest);
        } else if(strcmp(syscall, "readdir") == 0) {
            readdir_handler(cfd, rest);
        } else if(strcmp(syscall, "access") == 0) {
            access_handler(cfd, rest);
        } else if(strcmp(syscall, "mkdir") == 0) {
            mkdir_handler(cfd);
        } else if(strcmp(syscall, "mknod") == 0) {
            mknod_handler(cfd, rest);
        } else if(strcmp(syscall, "open") == 0) {
            open_handler(cfd, rest);
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


