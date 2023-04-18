#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

#define MAXCONNS 100
#define BACKLOG 200
#define STD_RETRIES 10

int current_port = -1;

int main(int argc, char *argv[])
{
	int rc;
	int tcp_sockfd, udp_sockfd, epoll_fd, new_fd, kdpfd, nfds, n, curfds;
    struct sockaddr_in servaddr;
	struct sockaddr_in client_addr;
	struct epoll_event ev;
	struct epoll_event *events;
	socklen_t addr_size;

	if (argc != 2) {
		fprintf(stderr, "usage: ./server <PORT>\n");
		return 1;
	}
    current_port = atoi(argv[1]);

    epoll_fd = epoll_create(MAXCONNS);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htonl(current_port);

    /* TCP Socket INIT */

    if ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        int retries = STD_RETRIES;
        while ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            fprintf(stderr, "Socket couldn't be created... Aborting!\n");
            exit(2);
        }
    }

    int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("SO_REUSEADDR");
    }

    if (bind(tcp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (bind(tcp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            exit(2);
        }
    }

    // listen for incoming connection
	if (listen(tcp_sockfd, BACKLOG) == -1) {
		int retries = STD_RETRIES;
        while (listen(tcp_sockfd, BACKLOG) == -1 && retries) {
            retries--;
        }
        if (!retries) {
            exit(2);
        }
	}

	ev.data.fd = tcp_sockfd;
	if(epoll_ctl(kdpfd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0)
	{
		fprintf(stderr, "epoll set insert error.");
		return -1;
	} else {
		printf("success insert listening socket into epoll.\n");
	}

	printf("server: waiting for connections...\n");

	ev.events = EPOLLIN|EPOLLET;
	ev.data.fd = tcp_sockfd;
	if(epoll_ctl(kdpfd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0) {
		fprintf(stderr, "epoll set insert error.");
		return -1;
	} else {
		printf("success insert listening socket into epoll.\n");
	}
	events = calloc (MAXEPOLLSIZE, sizeof ev);
	curfds = 1;
	while(1) 
	{ //loop for accept incoming connection

		nfds = epoll_wait(kdpfd, events, curfds, -1);
		if(nfds == -1)
		{
			perror("epoll_wait");
			break;
		}		
		for (n = 0; n < nfds; ++n)
		{
			if(events[n].data.fd == sockfd){
				addr_size = sizeof client_addr;
				new_fd = accept(events[n].data.fd, (struct sockaddr *)&client_addr, &addr_size);
				if (new_fd == -1)
				{
					if((errno == EAGAIN) ||
						 (errno == EWOULDBLOCK))
					{
						break;
					}
					else
					{
						perror("accept");
						break;
					}
				}
				printf("server: connection established...\n");
				set_non_blocking(new_fd);
				ev.events = EPOLLIN|EPOLLET;
				ev.data.fd = new_fd;
				if(epoll_ctl(kdpfd,EPOLL_CTL_ADD, new_fd, &ev)<0)
				{
					printf("Failed to insert socket into epoll.\n");
				}
				curfds++;
			} else {
				if(send(events[n].data.fd, "Hello, world!", 13, 0) == -1)
				{
					perror("send");
					break;
				}
				epoll_ctl(kdpfd, EPOLL_CTL_DEL, events[n].data.fd, &ev);
				curfds--;
				close(events[n].data.fd);
			}
		}
	}
	free(events);
	close(sockfd);			
	return 0;
}