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
#include "client.h"

#define STD_RETRIES 10

int16_t current_port = -1;

int verify_id(char *id, int tcp_sockfd)
{
    struct command_hdr check_client;
    check_client.opcode = CHECK_ID;
    check_client.option_sf = 0;
    check_client.buf_len = strlen(id) + 1;

    send(tcp_sockfd, &check_client, sizeof(struct command_hdr), 0);
    send(tcp_sockfd, id, check_client.buf_len, 0);

    recv(tcp_sockfd, &check_client, sizeof(struct command_hdr), 0);

    if (check_client.opcode == (uint8_t) ERR) {
        return 1;
    }

    printf("Client should be registered\n");
    return 0;
}

void set_errno(int errorcode) {
    errno = errorcode;
}

comm_type parse_command(char *comm, char **tokens)
{
    int nr;
	char *token, *nl = strchr(comm, '\n');
	token = strtok(comm, " ");
	short i = 0;

	if (nl) {
		*nl = '\0';
    }

	while (token && i != 3) {
		tokens[i++] = token;
		token = strtok(NULL, " ");
	}
	nr = i;

	if (strtok(NULL, " ")) {
		return ERR;
	}

    if (!strcmp(tokens[0], "exit")) {
        if (nr == 1) {
            return EXIT;
        }
        return ERR;
    } else if (!strcmp(tokens[0], "unsubscribe")) {
        if (nr == 2) {
            return UNSUBSCRIBE;
        }
        return ERR;
    } else if (!strcmp(tokens[0], "subscribe")) {
        if (nr == 3) {
            return SUBSCRIBE;
        }
        return ERR;
    }

    return ERR;
}

int main(int argc, char *argv[])
{
    //setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int rc;
	int tcp_sockfd, udp_sockfd, epoll_fd;
    struct sockaddr_in servaddr;
	struct epoll_event ev;
	struct epoll_event *events_list;

    if (argc != 4) {
        printf("Usage: %s <ID> <SERVER_IP> <PORT>\n", argv[0]);
        return 1;
    }

    char check_id[10];
    sprintf(check_id, "%hd", atoi(argv[3]));
    if (strlen(argv[3]) != strlen(check_id)) {
        set_errno(ECONNABORTED);
        perror("Invalid server port ");
        return 1;
    }
    current_port = atoi(argv[3]);

    if (strlen(argv[1]) > 10) {
        set_errno(ECONNABORTED);
        perror("Subscriber id too long ");
        return 1;
    }

    epoll_fd = epoll_create(2);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(current_port);

    if (inet_pton(AF_INET, argv[2], &servaddr.sin_addr.s_addr) <= 0) {
        set_errno(ECONNABORTED);
        perror("inet_pton(SERVER_IP) failed ");
        return 1;
    }

    /* TCP Socket INIT */

    if ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        int retries = STD_RETRIES;
        while ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            set_errno(ECONNABORTED);
            perror("socket(SOCK_STREAM) failed ");
            return 1;
        }
    }

	// /* Setting TCP Socket as non-blocking */
	// int current_tcp_flags = fcntl(tcp_sockfd, F_GETFL, 0);
	// if (current_tcp_flags == -1) {
	// 	perror("fcntl(F_GETFL) failed.");
	// 	return 1;;
	// } else if (fcntl(tcp_sockfd, F_SETFL, current_tcp_flags | O_NONBLOCK) == -1) {
	// 	perror("fcntl(F_SETFL) failed.");
	// 	return 1;;
	// }

    int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        set_errno(ECONNABORTED);
        perror("SO_REUSEADDR failed ");
        return 1;
    }

    if (connect(tcp_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (connect(tcp_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            set_errno(ECONNABORTED);
            perror("connect() failed ");
            return 1;
        }
    }

    if ((rc = verify_id(strdup(argv[1]), tcp_sockfd)) == 1) {
        fprintf(stderr, "Client %s already connected.\n", argv[1]);

        close(tcp_sockfd);
        close(epoll_fd);
        free(events_list);

        return 1;
    }

    ev.events = EPOLLIN;
	ev.data.fd = tcp_sockfd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0) {
		int retries = STD_RETRIES;
        while (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            set_errno(ECONNABORTED);
			perror("epoll_ctl(EPOLL_CTL_ADD) failed ");
            return 1;
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
            set_errno(ECONNABORTED);
			perror("epoll_ctl(EPOLL_CTL_ADD) failed ");
            return 1;
        }
	}

	events_list = calloc(2, sizeof(ev));
    while (1) {
		int num_of_events = epoll_wait(epoll_fd, events_list, 2, 2000);
		if(num_of_events == -1) {
            set_errno(ECONNABORTED);
			perror("epoll_wait() failed ");
			break;
		}

		for (int i = 0; i < num_of_events; i++) {
			if(events_list[i].data.fd == tcp_sockfd){
				
			} else if (events_list[i].data.fd == STDIN_FILENO) {
                comm_type req;
                char **tokens = calloc(4, sizeof(char *));
                char command[BUFSIZ];
				fgets(command, BUFSIZ, stdin);

                if ((req = parse_command(command, tokens)) == -1) {
                    printf("Invalid command. Please try again.\n");
                    continue;
                }

                switch (req) {
                    case EXIT: {
                        close(tcp_sockfd);
                        close(epoll_fd);
                        free(events_list);
                        free(tokens);

                        exit(0);
                    }
                    break;
                    case UNSUBSCRIBE: {
                        printf("unsub topic: %s\n", tokens[1]);
                    }
                    break;
                    case SUBSCRIBE: {
                        char check_SF[10];
                        sprintf(check_SF, "%hhd", atoi(tokens[2]));
                        if (strlen(tokens[2]) != strlen(check_SF)) {
                            printf("Invalid command. Please try again.\n");
                            continue;
                        }

                        printf("topic: %s | SF: %d\n", tokens[1], atoi(tokens[2]));
                    }
                    break;
                }
                free(tokens);
            }
		}
	}

    return 0;
}