#include <arpa/inet.h>
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
#include "server.h"
#include "list.h"

#define MAXCONNS 100
#define BACKLOG 200
#define STD_RETRIES 10

int16_t current_port = -1;
linked_list_t *registered_users;

int verify_new_id(char *id) {
	ll_node_t *head = registered_users->head;

	while (head) {
		struct client *curr_client = (struct client*)head->data;
		if (!strcmp(curr_client->id, id)) {
			return 1;
		}
		head = head->next;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	//setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int rc;
	int tcp_sockfd, udp_sockfd, epoll_fd;
	registered_users = ll_create(sizeof(client));
    struct sockaddr_in servaddr;
	struct epoll_event ev;
	struct epoll_event *events_list;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
		return 1;
	}

    current_port = atoi(argv[1]);
	if (!current_port) {
		perror("Invalid server port. Aborting...\n");
		exit(2);
	}

	printf("%d\n", current_port);

    epoll_fd = epoll_create(MAXCONNS);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(current_port);

    /* TCP Socket INIT */

    if ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        int retries = STD_RETRIES;
        while ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            perror("socket(SOCK_STREAM) failed. Aborting...\n");
            exit(2);
        }
    }

	/* Setting TCP Socket as non-blocking */
	int current_tcp_flags = fcntl(tcp_sockfd, F_GETFL, 0);
	if (current_tcp_flags == -1) {
		perror("fcntl(F_GETFL) failed.");
		exit(2);
	} else if (fcntl(tcp_sockfd, F_SETFL, current_tcp_flags | O_NONBLOCK) == -1) {
		perror("fcntl(F_SETFL) failed.");
		exit(2);
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
			perror("bind() failed. Aborting...\n");
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
			perror("listen() failed. Aborting...\n");
            exit(2);
        }
	}

	ev.events = EPOLLIN;
	ev.data.fd = tcp_sockfd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0) {
		int retries = STD_RETRIES;
        while (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("epoll_ctl(EPOLL_CTL_ADD) failed. Aborting...\n");
            exit(2);
        }
	}

	/* UDP Socket INIT */

    if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        int retries = STD_RETRIES;
        while ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            perror("socket(SOCK_DGRAM) failed. Aborting...\n");
            exit(2);
        }
    }

	if (setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("SO_REUSEADDR");
    }

    if (bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("bind() failed. Aborting...\n");
            exit(2);
        }
    }

	ev.events = EPOLLIN;
	ev.data.fd = udp_sockfd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sockfd, &ev) < 0) {
		int retries = STD_RETRIES;
        while (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sockfd, &ev) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("epoll_ctl(EPOLL_CTL_ADD) failed. Aborting...\n");
            exit(2);
        }
	}

	ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0) {
		int retries = STD_RETRIES;
        while (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("epoll_ctl(EPOLL_CTL_ADD) failed. Aborting...\n");
            exit(2);
        }
	}

	events_list = calloc(MAXCONNS, sizeof(ev));
	while (1) {
		int num_of_events = epoll_wait(epoll_fd, events_list, MAXCONNS, 2000);
		if(num_of_events == -1) {
			perror("epoll_wait() failed. Aborting...");
			break;
		} else if (!num_of_events) {
			
		}

		for (int i = 0; i < num_of_events; i++) {
			if(events_list[i].data.fd == tcp_sockfd){
				struct sockaddr_in new_client_addr;
				socklen_t addr_size = sizeof(struct sockaddr_in);
				int new_client_fd = accept(events_list[i].data.fd,
										   (struct sockaddr *) &new_client_addr,
										   &addr_size);

				if (new_client_fd == -1) {
					if(errno == EAGAIN || errno == EWOULDBLOCK) {
						break;
					} else {
						perror("accept(new_client) fails. Aborting...");
						break;
					}
				}

				struct command_hdr check_client;
				recv(new_client_fd, &check_client, sizeof(struct command_hdr), 0);

				if (check_client.opcode == ERR) {
					fprintf(stderr, "Error opcode.\n");

					close(new_client_fd);
					continue;
				} else if (check_client.opcode != CHECK_ID) {
					fprintf(stderr, "Invalid opcode.\n");

					check_client.opcode = ERR;
					check_client.option_sf = 0;
					check_client.buf_len = 0;
					send(new_client_fd, &check_client, sizeof(struct command_hdr), 0);
					close(new_client_fd);
					continue;
				}

				char *buf = malloc(check_client.buf_len);
				recv(new_client_fd, buf, check_client.buf_len, 0);

				if (verify_new_id(buf)) {
					printf("Client %s already connected.\n", buf);
					check_client.opcode = ERR;
					check_client.option_sf = 0;
					check_client.buf_len = 0;

					send(new_client_fd, &check_client, sizeof(struct command_hdr), 0);
					close(new_client_fd);
					continue;
				}

				check_client.opcode = OK;
				check_client.option_sf = 0;
				check_client.buf_len = 0;
				send(new_client_fd, &check_client, sizeof(struct command_hdr), 0);

				struct client *new_client = calloc(1, sizeof(struct client));
				new_client->serv_conned = 1;
				strcpy(new_client->id, buf);
				new_client->fd = new_client_fd;

				ll_add_nth_node(registered_users, ll_get_size(registered_users), new_client);
				ev.events = EPOLLIN;
				ev.data.ptr = new_client;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client->fd, &ev) < 0) {
					printf("Failed to insert socket into epoll.\n");
				}

				char ip_addr[30];
				if (!inet_ntop(AF_INET, &new_client_addr.sin_addr, ip_addr, addr_size)) {
					perror("inet_ntop() failed. Aborting...\n");
					exit(2);
				}

				printf("New client %s connected from %s:%d.\n", new_client->id, ip_addr, new_client_addr.sin_port);
			} else if (events_list[i].data.fd == udp_sockfd) {
				struct sockaddr_in udp_client;
				socklen_t addr_size = sizeof(struct sockaddr_in);

				struct udp_packet new_content;
				int rc = recvfrom(udp_sockfd, &new_content,
								  sizeof(new_content), 0,
								  (struct sockaddr *) &udp_client, &addr_size);
				
				if (rc < 0) {
					perror("recvfrom() failed.");
				}
			} else if (events_list[i].data.fd == STDIN_FILENO) {
				char command[BUFSIZ];
				fgets(command, BUFSIZ, stdin);

				if (strlen(command) != 5 || strcmp(command, "exit\n")) {
					printf("Invalid command. Please try again.\n");
					continue;
				}

				free(events_list);
				close(tcp_sockfd);
				close(udp_sockfd);
				close(epoll_fd);

				return 0;
			} else {
				struct client *curr_client = (struct client*)events_list[i].data.ptr;
				int test;
				rc = recv(curr_client->fd, &test, sizeof(test), 0);

				if (!rc) {
					printf("Client %s disconnected.\n", curr_client->id);
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events_list[i].data.fd, NULL);
					close(curr_client->fd);
				}
			}
		}
	}

	free(events_list);
	close(tcp_sockfd);
	close(udp_sockfd);
	close(epoll_fd);
	
	return 0;
}