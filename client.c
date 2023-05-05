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

void add_subbed_topic(uint8_t id, uint8_t sf, char *topic_name)
{
    struct subbed_topic new_topic;
    new_topic.info.id = id;
    strcpy(new_topic.info.topic_name, topic_name);
    new_topic.sf = sf;

    ll_add_nth_node(subbed_topics, 0, &new_topic);
}

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
        int topics_no, tot_len;
        char header[sizeof(topics_no) + sizeof(tot_len)];
        char buf[BUFSIZ];

        recv(tcp_sockfd, header, sizeof(topics_no) + sizeof(tot_len), 0);
        memcpy(&topics_no, header, sizeof(topics_no));
        memcpy(&tot_len, header + sizeof(topics_no), sizeof(tot_len));

        if (!tot_len) {
            return 0;
        }

        recv(tcp_sockfd, buf, tot_len, 0);

        int offset = 0;
        for (int i = 0; i < topics_no; i++) {
            struct subbed_topic curr_topic;
            int len;

            // as putea incerca sa dau memcpy la toata structura daca pun in len tot din server
            memcpy(&len, buf + offset, sizeof(len));
            memcpy(&curr_topic.sf, buf + offset + sizeof(len), sizeof(curr_topic.sf));
            memcpy(&curr_topic.info.id, buf + offset + sizeof(len) + sizeof(curr_topic.sf), sizeof(curr_topic.info.id));
            memcpy(curr_topic.info.topic_name, buf + offset + sizeof(len) + sizeof(curr_topic.sf) + sizeof(curr_topic.info.id), len);

            offset += sizeof(len) + sizeof(curr_topic.sf) + sizeof(curr_topic.info.id) + len;
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

    char check_port[10];
    sprintf(check_port, "%hd", atoi(argv[3]));
    if (strlen(argv[3]) != strlen(check_port)) {
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
		int num_of_events = epoll_wait(epoll_fd, events_list, 2, 100);
		if(num_of_events == -1) {
            set_errno(ECONNABORTED);
			perror("epoll_wait() failed ");
			break;
		}

		for (int i = 0; i < num_of_events; i++) {
			if(events_list[i].data.fd == tcp_sockfd) {
                int rc, msg_num, tot_len;
                struct message_hdr server_msg;
                char buf[BUFSIZ];

                char header[sizeof(msg_num) + sizeof(tot_len)];
				rc = recv(tcp_sockfd, header, sizeof(msg_num) + sizeof(tot_len), 0);

				if (!rc) {
                    ll_free(&subbed_topics);
					close(tcp_sockfd);
                    close(epoll_fd);
                    free(events_list);

					return 0;
				}

                memcpy(&msg_num, header, sizeof(msg_num));
                memcpy(&tot_len, header + sizeof(msg_num), sizeof(tot_len));

                if (msg_num == 0) {
                    continue;
                }

                recv(tcp_sockfd, buf, tot_len, 0);

                int offset = 0;
                for (int i = 0; i < msg_num; i++) {
                    memcpy(&server_msg, buf + offset, sizeof(server_msg));
                    char content[server_msg.buf_len];
                    memcpy(content, buf + offset + sizeof(server_msg), server_msg.buf_len);

                    char ip_addr[30];
                    inet_ntop(AF_INET, &server_msg.ip_addr, ip_addr, sizeof(struct sockaddr_in));

                    char *print_string;
                    switch (server_msg.data_type) {
                    case INT: {
                        printf(INT_MSG, ip_addr, server_msg.port, get_topic_name(server_msg.topic_id), *((int *)content));
                    }
                    break;
                    case SHORT_REAL: {
                        printf(SR_MSG, ip_addr, server_msg.port, get_topic_name(server_msg.topic_id), *((float *)content));
                    }
                    break;
                    case FLOAT: {
                        printf(FLT_MSG, ip_addr, server_msg.port, get_topic_name(server_msg.topic_id), *((double *)content));
                    }
                    break;
                    case STRING: {
                        printf(STR_MSG, ip_addr, server_msg.port, get_topic_name(server_msg.topic_id), content);
                    }
                    break;
                    default: {
                        fprintf(stderr, "Unrecognized data type\n.");
                    }
                    }

                    offset += sizeof(server_msg) + server_msg.buf_len;
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

                        return 0;
                    }
                    break;
                    case UNSUBSCRIBE: {
                        int topic_id = get_topic_id(tokens[1]);

                        if (topic_id == -1) {
                            fprintf(stderr, "Requested topic isn't subscribed to. Please try again.\n");
                            continue;
                        }

                        unsubscribe_topic(topic_id, tcp_sockfd);
                        ll_remove_nth_node(subbed_topics, get_topic_idx(topic_id));

                        printf("Unsubscribed from topic.\n");
                    }
                    break;
                    case SUBSCRIBE: {
                        char check_SF[10];
                        sprintf(check_SF, "%hhd", atoi(tokens[2]));
                        if (strlen(tokens[2]) != strlen(check_SF) ||
                            (atoi(tokens[2]) != 0 && atoi(tokens[2]) != 1)) {
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

                        add_subbed_topic(topic_id, atoi(tokens[2]), tokens[1]);
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