#include "server.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define BACKLOG 10


static void client_handler(int cfd) {
    char buf[1024];
    int data_size;
    while (1) {
        data_size = read (cfd, &buf, 1024);
        if (data_size <= 0)
            break;
        printf("Received from client: %s\n", buf);
        if(strcmp(buf, get))
        write (cfd, &buf, data_size);
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
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
	listen(sockfd, BACKLOG);

	while (1) 
    {
        int peer_addr_size = sizeof(struct sockaddr_in);
        cfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);

        switch(fork()) {
            case -1:
                exit(100);
            case 0:
                close(sockfd);
                client_handler(cfd);
                exit(0);
            default:
                close(cfd);
        }
    }
    close(sockfd);

	return 0;
}


