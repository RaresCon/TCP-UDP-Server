#include <arpa/inet.h>
#include <netinet/tcp.h>
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
#include "list.h"

#define STD_RETRIES 10

int16_t current_port = -1;
linked_list_t *subbed_topics;

int get_topic_id(char *topic_name) {
    ll_node_t *head = subbed_topics->head;

    while (head) {
        struct subbed_topic *curr_topic = (struct subbed_topic*)head->data;
        if (!strcmp(curr_topic->info.topic_name, topic_name)) {
            return curr_topic->info.id;
        }

        head = head->next;
    }

    return -1;
}

char *get_topic_name(uint8_t topic_id)
{
    ll_node_t *head = subbed_topics->head;

    while (head) {
		struct topic *curr_topic = (struct topic*)head->data;
		if (curr_topic->id == topic_id) {
			return curr_topic->topic_name;
		}
		head = head->next;
	}

    return NULL;
}

int get_topic_idx(uint8_t topic_id)
{
	ll_node_t *head = subbed_topics->head;
	int idx = 0;
	
	while (head) {
		struct topic *curr_topic = (struct topic*)head->data;
		if (curr_topic->id == topic_id) {
			return idx;
		}
		idx += 1;
		head = head->next;
	}

	return -1;
}

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
        return -1;
    } else if(check_client.option_sf == (uint8_t) 1) {
        int topics_no;
        recv(tcp_sockfd, &topics_no, sizeof(int), 0);

        for (int i = 0; i < topics_no; i++) {
            struct subbed_topic curr_topic;
            int len;

            recv(tcp_sockfd, &curr_topic.info.id, sizeof(uint8_t), 0);
            recv(tcp_sockfd, &curr_topic.sf, sizeof(uint8_t), 0);
            recv(tcp_sockfd, &len, sizeof(int), 0);

            recv(tcp_sockfd, curr_topic.info.topic_name, len, 0);

            ll_add_nth_node(subbed_topics, 0, &curr_topic);
        }
    }

    return 0;
}

uint8_t subscribe_topic(char *topic_name, int sf, int tcp_sockfd)
{
    if (get_topic_idx(get_topic_id(topic_name)) != -1) {
        return -2;
    }

    struct command_hdr sub_topic;
    sub_topic.opcode = SUBSCRIBE;
    sub_topic.option_sf = sf;
    sub_topic.buf_len = strlen(topic_name) + 1;

    send(tcp_sockfd, &sub_topic, sizeof(struct command_hdr), 0);
    send(tcp_sockfd, topic_name, sub_topic.buf_len, 0);

    recv(tcp_sockfd, &sub_topic, sizeof(struct command_hdr), 0);

    if (sub_topic.opcode == (uint8_t) ERR) {
        return -1;
    }

    return sub_topic.option_sf;
}

uint8_t unsubscribe_topic(uint8_t topic_id, int tcp_sockfd)
{
    struct command_hdr sub_topic;
    sub_topic.opcode = UNSUBSCRIBE;
    sub_topic.option_sf = topic_id;
    sub_topic.buf_len = 0;

    send(tcp_sockfd, &sub_topic, sizeof(struct command_hdr), 0);
    recv(tcp_sockfd, &sub_topic, sizeof(struct command_hdr), 0);

    if (sub_topic.opcode == (uint8_t) ERR) {
        return -1;
    }

    return sub_topic.option_sf;
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
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int rc;
	int tcp_sockfd, udp_sockfd, epoll_fd;
    subbed_topics = ll_create(sizeof(subbed_topic));
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
        perror("SO_REUSEADDR failed");
    }
    if (setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        set_errno(ECONNABORTED);
		perror("TCP_NODELAY");
	}

    if (connect(tcp_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (connect(tcp_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            set_errno(ECONNABORTED);
            perror("connect() failed");
            return 1;
        }
    }

    if ((rc = verify_id(strdup(argv[1]), tcp_sockfd)) == -1) {
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
			if(events_list[i].data.fd == tcp_sockfd) {
                int rc, msg_num;
                struct message_hdr server_msg;
				rc = recv(tcp_sockfd, &msg_num, sizeof(msg_num), 0);

				if (!rc) {
                    ll_free(&subbed_topics);
					close(tcp_sockfd);
                    close(epoll_fd);
                    free(events_list);

					return 0;
				}

                for (int i = 0; i < msg_num; i++) {
                    char buf[1500];
                    rc = recv(tcp_sockfd, &server_msg, sizeof(server_msg), 0);
                    rc = recv(tcp_sockfd, buf, server_msg.buf_len, 0);

                    char ip_addr[30];
                    inet_ntop(AF_INET, &server_msg.ip_addr, ip_addr, sizeof(struct sockaddr_in));
                    switch (server_msg.data_type) {
                    case INT: {
                        printf(INT_MSG, ip_addr, server_msg.port, get_topic_name(server_msg.topic_id), *((int *)buf));
                    }
                    break;
                    }
                }
			} else if (events_list[i].data.fd == STDIN_FILENO) {
                comm_type req;
                char **tokens = calloc(4, sizeof(char *));
                char command[BUFSIZ];
				fgets(command, BUFSIZ, stdin);

                if ((req = parse_command(command, tokens)) == -1) {
                    fprintf(stderr, "Invalid command. Please try again.\n");
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
                        int topic_id = get_topic_id(tokens[1]);

                        if (topic_id == -1) {
                            fprintf(stderr, "Requested topic isn't subscribed to. Please try again.\n");
                            continue;
                        }

                        unsubscribe_topic(topic_id, tcp_sockfd);
                        ll_node_t *removed = ll_remove_nth_node(subbed_topics, get_topic_idx(topic_id));

                        printf("Unsubscribed from topic.\n");
                    }
                    break;
                    case SUBSCRIBE: {
                        char check_SF[10];
                        sprintf(check_SF, "%hhd", atoi(tokens[2]));
                        if (strlen(tokens[2]) != strlen(check_SF)) {
                            fprintf(stderr, "Invalid command. Please try again.\n");
                            continue;
                        }

                        uint8_t topic_id = subscribe_topic(tokens[1], atoi(tokens[2]), tcp_sockfd);
                        
                        if (topic_id == (uint8_t) -1) {
                            fprintf(stderr, "Requested topic doesn't exist. Please try again.\n");
                            continue;
                        } else if (topic_id == (uint8_t) -2) {
                            fprintf(stderr, "Already subscribed to this topic.\n");
                            continue;
                        }

                        struct subbed_topic new_topic;
                        new_topic.info.id = topic_id;
                        strcpy(new_topic.info.topic_name, tokens[1]);
                        new_topic.sf = atoi(tokens[2]);

                        ll_add_nth_node(subbed_topics, 0, &new_topic);

                        printf("Subscribed to topic.\n");
                    }
                    break;
                }
                free(tokens);
            }
		}
	}

    return 0;
}