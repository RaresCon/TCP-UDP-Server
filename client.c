#include "common.h"
#include "client.h"

int16_t current_port = -1;
linked_list_t *subbed_topics;

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int rc;
	int tcp_sockfd, epoll_fd;
    subbed_topics = ll_create(sizeof(struct topic));
    struct sockaddr_in servaddr;
	struct epoll_event ev;
	struct epoll_event *events_list;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ID> <SERVER_IP> <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    current_port = check_valid_uns_number(argv[3]);
    if (current_port == -1) {
        fprintf(stderr, "Invalid port.");
        exit(EXIT_FAILURE);
    }

    if (strlen(argv[1]) > 10) {
        fprintf(stderr, "Subscriber id too long.\n");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(current_port);

    if (inet_pton(AF_INET, argv[2], &servaddr.sin_addr.s_addr) <= 0) {
        perror("inet_pton(SERVER_IP) failed ");
        exit(EXIT_FAILURE);
    }

    /* TCP Socket INIT */
    tcp_sockfd = init_socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd == -1) {
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("SO_REUSEADDR failed");
    }
    if (setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
		perror("TCP_NODELAY");
	}

    if (connect(tcp_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (connect(tcp_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            perror("connect() failed");
            exit(EXIT_FAILURE);
        }
    }

    if ((rc = verify_id(strdup(argv[1]), tcp_sockfd)) == -1) {
        fprintf(stderr, "Client %s already connected.\n", argv[1]);
        close(tcp_sockfd);

        exit(EXIT_FAILURE);
    }

    epoll_fd = init_epoll(2);
    if (epoll_fd == -1) {
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
	ev.data.fd = tcp_sockfd;
	if (add_event(epoll_fd, tcp_sockfd, &ev)) {
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;
	if (add_event(epoll_fd, STDIN_FILENO, &ev)) {
        exit(EXIT_FAILURE);
    }

	events_list = calloc(2, sizeof(ev));
    while (1) {
		int num_of_events = epoll_wait(epoll_fd, events_list, 2, 100);
		if(num_of_events == -1) {
			perror("epoll_wait() failed ");
			break;
		}

		for (int i = 0; i < num_of_events; i++) {
			if(events_list[i].data.fd == tcp_sockfd) {
                int rc, msgs_no, tot_len;
                char buf[BUFSIZ];

                char header[sizeof(msgs_no) + sizeof(tot_len)];
				rc = recv_all(tcp_sockfd, header, sizeof(msgs_no) + sizeof(tot_len));

				if (!rc) {
                    ll_free(&subbed_topics);
					close(tcp_sockfd);
                    close(epoll_fd);
                    free(events_list);

					return 0;
				}

                memcpy(&msgs_no, header, sizeof(msgs_no));
                memcpy(&tot_len, header + sizeof(msgs_no), sizeof(tot_len));

                if (msgs_no == 0) {
                    continue;
                }

                recv_all(tcp_sockfd, buf, tot_len);

                handle_incoming_msgs(buf, msgs_no, tot_len);
			} else if (events_list[i].data.fd == STDIN_FILENO) {
                comm_type req;
                char **tokens = calloc(4, sizeof(char *));
                char command[BUFSIZ];
				fgets(command, BUFSIZ, stdin);

                if ((req = parse_command(command, tokens)) == -1) {
                    fprintf(stderr, ERR_COMM);
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
                            fprintf(stderr, ERR_NSUB);
                            continue;
                        }

                        unsubscribe_topic(topic_id, tcp_sockfd);
                        ll_remove_nth_node(subbed_topics, get_topic_idx(topic_id));

                        printf("Unsubscribed from topic.\n");
                    }
                    break;
                    case SUBSCRIBE: {
                        int sf = check_valid_uns_number(tokens[2]);

                        if (sf == -1 || (sf != 0 && sf != 1)) {
                            fprintf(stderr, ERR_COMM);
                            continue;
                        }

                        uint8_t topic_id = subscribe_topic(tokens[1],
                                                           atoi(tokens[2]),
                                                           tcp_sockfd);
                        
                        if (topic_id == (uint8_t) -1) {
                            fprintf(stderr, ERR_NOTP);
                            continue;
                        } else if (topic_id == (uint8_t) -2) {
                            fprintf(stderr, ERR_ASUB);
                            continue;
                        }

                        add_subbed_topic(topic_id, tokens[1]);
                        printf("Subscribed to topic.\n");
                    }
                    break;
                    default:
                        fprintf(stderr, ERR_COMM);
                        continue;
                    break;
                }
                free(tokens);
            }
		}
	}

    close(tcp_sockfd);
    close(epoll_fd);
    free(events_list);

    return 0;
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

void add_subbed_topic(uint8_t id, char *topic_name)
{
    struct topic new_topic;
    new_topic.id = id;
    strcpy(new_topic.topic_name, topic_name);

    ll_add_nth_node(subbed_topics, 0, &new_topic);
}

int get_topic_id(char *topic_name)
{
    ll_node_t *head = subbed_topics->head;

    while (head) {
        struct topic *curr_topic = (struct topic*)head->data;
        if (!strcmp(curr_topic->topic_name, topic_name)) {
            return curr_topic->id;
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
    struct command_hdr res;

    send_command(tcp_sockfd, CHECK_ID, 0, strlen(id) + 1);
    send_all(tcp_sockfd, id, strlen(id) + 1);

    recv_all(tcp_sockfd, &res, sizeof(struct command_hdr));

    if (res.opcode == (uint8_t) ERR) {
        free(id);
        return -1;
    } else if(res.option == (uint8_t) 1) {
        int topics_no, tot_len;
        char header[sizeof(topics_no) + sizeof(tot_len)];
        char buf[BUFSIZ];

        recv_all(tcp_sockfd, header, sizeof(topics_no) + sizeof(tot_len));
        memcpy(&topics_no, header, sizeof(topics_no));
        memcpy(&tot_len, header + sizeof(topics_no), sizeof(tot_len));

        if (!tot_len) {
            free(id);
            return 0;
        }

        recv_all(tcp_sockfd, buf, tot_len);

        int offset = 0;
        for (int i = 0; i < topics_no; i++) {
            struct topic curr_topic;
            int name_len, topic_len;

            memcpy(&name_len, buf + offset, sizeof(name_len));
            topic_len = sizeof(curr_topic.id) + name_len;
            memcpy(&curr_topic, buf + offset + sizeof(name_len), topic_len);

            offset += sizeof(name_len) + topic_len;
            add_subbed_topic(curr_topic.id, curr_topic.topic_name);
        }
    }

    free(id);
    return 0;
}

uint8_t subscribe_topic(char *topic_name, int sf, int tcp_sockfd)
{
    if (get_topic_idx(get_topic_id(topic_name)) != -1) {
        return -2;
    }

    struct command_hdr sub_topic;
    send_command(tcp_sockfd, SUBSCRIBE, sf, strlen(topic_name) + 1);
    send_all(tcp_sockfd, topic_name, strlen(topic_name) + 1);

    recv_all(tcp_sockfd, &sub_topic, sizeof(struct command_hdr));

    if (sub_topic.opcode == (uint8_t) ERR) {
        return -1;
    }

    return sub_topic.option;
}

uint8_t unsubscribe_topic(uint8_t topic_id, int tcp_sockfd)
{
    if (!get_topic_name(topic_id)) {
        return -1;
    }

    struct command_hdr unsub_topic;
    send_command(tcp_sockfd, UNSUBSCRIBE, topic_id, 0);
    recv_all(tcp_sockfd, &unsub_topic, sizeof(struct command_hdr));

    if (unsub_topic.opcode == (uint8_t) ERR) {
        return -1;
    }

    return unsub_topic.option;
}

void handle_incoming_msgs(char *buf, int msgs_no, int tot_len)
{
    struct message_hdr server_msg;

    int offset = 0;
    for (int i = 0; i < msgs_no; i++) {
        char ip_addr[30];
        memcpy(&server_msg, buf + offset, sizeof(server_msg));
        char content[server_msg.buf_len];
        memcpy(content, buf + offset + sizeof(server_msg), server_msg.buf_len);

        if (!inet_ntop(AF_INET, &server_msg.ip_addr, ip_addr, sizeof(struct sockaddr_in))) {
            fprintf(stderr, "inet_ntop() failed. Message display may be affected\n");
        }


        uint16_t ntoh_port = ntohs(server_msg.port);
        switch (server_msg.data_type) {
            case INT: {
                printf(INT_MSG, ip_addr, ntoh_port,
                                get_topic_name(server_msg.topic_id),
                                *((int *)content));
            }
            break;
            case SHORT_REAL: {
                printf(SR_MSG, ip_addr, ntoh_port,
                               get_topic_name(server_msg.topic_id),
                               *((float *)content));
            }
            break;
            case FLOAT: {
                printf(FLT_MSG, ip_addr, ntoh_port,
                                get_topic_name(server_msg.topic_id),
                                *((double *)content));
            }
            break;
            case STRING: {
                printf(STR_MSG, ip_addr, ntoh_port,
                                get_topic_name(server_msg.topic_id),
                                content);
            }
            break;
            default: {
                fprintf(stderr, "Unrecognized data type\n.");
            }
            break;
        }

        offset += sizeof(server_msg) + server_msg.buf_len;
    }
}
