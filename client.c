#include "common.h"
#include "client.h"

int16_t current_port = -1;
linked_list_t *subbed_topics;
linked_list_t *history;

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int rc;
	int tcp_sockfd, epoll_fd;
    subbed_topics = ll_create(sizeof(struct topic));
    history = ll_create(sizeof(char **));
    struct sockaddr_in servaddr;
	struct epoll_event ev;
	struct epoll_event *events_list;

    if (argc != 4) {
        fprintf(stderr, USAGE, argv[0]);
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

    rc = verify_id(strdup(argv[1]), tcp_sockfd);
    if (rc == -1) {
        fprintf(stderr, "Client %s already connected.\n", argv[1]);
        close(tcp_sockfd);

        exit(EXIT_FAILURE);
    } else if (rc == -2) {
        fprintf(stderr, ERR_CONN);
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
                    fprintf(stderr, ERR_CONN);

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

                if (recv_all(tcp_sockfd, buf, tot_len) < tot_len) {
                    fprintf(stderr, "The messages were not successfully received.\n");
                    continue;
                }

                handle_incoming_msgs(buf, msgs_no, tot_len);
			} else if (events_list[i].data.fd == STDIN_FILENO) {
                int nr;
                comm_type req;
                char **tokens = calloc(4, sizeof(char *));
                char command[BUFSIZ];
				fgets(command, BUFSIZ, stdin);

                nr = tokenize_command(command, tokens);

                if ((req = parse_command(nr, tokens)) == -1) {
                    fprintf(stderr, ERR_COMM);
                    continue;
                }

                switch (req) {
                    case REQ_TOPICS: {
                        rc = show_available_topics(tcp_sockfd);
                        if (rc == -1) {
                            close(tcp_sockfd);
                            close(epoll_fd);
                            free(events_list);
                            free(tokens);

                            exit(EXIT_SUCCESS);
                        }
                    }
                    break;
                    case REQ_UNSUB: {
                        int topic_id = get_topic_id(tokens[1]);

                        if (topic_id == 0) {
                            fprintf(stderr, ERR_NSUB);
                            continue;
                        }

                        rc = unsubscribe_topic(topic_id, tcp_sockfd);
                        if (rc == -1) {
                            fprintf(stderr, "Unsubscribe unavailable.\n");
                            continue;
                        } else if (rc == -2) {
                            close(tcp_sockfd);
                            close(epoll_fd);
                            free(events_list);
                            free(tokens);

                            exit(EXIT_SUCCESS);
                        }
                        ll_remove_nth_node(subbed_topics, get_topic_idx(topic_id));

                        printf("Unsubscribed from topic.\n");
                    }
                    break;
                    case REQ_SUB: {
                        int sf = check_valid_uns_number(tokens[2]);

                        if (strlen(tokens[1]) > 50) {
                            fprintf(stderr, ERR_COMM);
                            continue;
                        }

                        if (sf == -1 || (sf != 0 && sf != 1)) {
                            fprintf(stderr, ERR_COMM);
                            continue;
                        }

                        uint32_t topic_id = subscribe_topic(tokens[1],
                                                            atoi(tokens[2]),
                                                            tcp_sockfd);
                        
                        if (topic_id == -1) {
                            fprintf(stderr, ERR_NOTP);
                            continue;
                        } else if (topic_id == -2) {
                            fprintf(stderr, ERR_ASUB);
                            continue;
                        } else if (topic_id == -3) {
                            fprintf(stderr, ERR_CONN);

                            close(tcp_sockfd);
                            close(epoll_fd);
                            free(events_list);
                            free(tokens);

                            exit(EXIT_FAILURE);
                        }

                        add_subbed_topic(topic_id, tokens[1]);
                        printf("Subscribed to topic.\n");
                    }
                    break;
                    case REQ_HISTORY: {
                        show_history();
                    }
                    break;
                    case REQ_SUBBED: {
                        show_client_topics();
                    }
                    break;
                    case REQ_SAVE:
                        save_history();
                    break;
                    case EXIT: {
                        close(tcp_sockfd);
                        close(epoll_fd);
                        free(events_list);
                        free(tokens);

                        exit(EXIT_SUCCESS);
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

comm_type parse_command(int nr, char **tokens)
{
    if (!strcmp(tokens[0], "exit") && nr == 1) {
        return EXIT;
    } else if (!strcmp(tokens[0], "unsubscribe") && nr == 2) {
        return REQ_UNSUB;
    } else if (!strcmp(tokens[0], "subscribe") && nr == 3) {
        return REQ_SUB;
    } else if (!strcmp(tokens[0], "show") && nr == 2) {
        if (!strcmp(tokens[1], "server_topics")) {
            return REQ_TOPICS;
        } else if (!strcmp(tokens[1], "history")) {
            return REQ_HISTORY;
        } else if (!strcmp(tokens[1], "my_topics")) {
            return REQ_SUBBED;
        }
    } else if (!strcmp(tokens[0], "save")) {
        if (!strcmp(tokens[1], "history")) {
            return REQ_SAVE;
        }
    }

    return ERR;
}

void add_subbed_topic(uint32_t id, char *topic_name)
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

char *get_topic_name(uint32_t topic_id)
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

int get_topic_idx(uint32_t topic_id)
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

    if (send_command(tcp_sockfd, REQ_CHECK_ID, 0, strlen(id) + 1) == -1) {
        fprintf(stderr, ERR_CONN);
        free(id);
        return -2;
    }
    
    if (send_all(tcp_sockfd, id, strlen(id) + 1) < strlen(id) + 1) {
        fprintf(stderr, ERR_CONN);
        free(id);
        return -2;
    }

    if (recv_all(tcp_sockfd, &res, sizeof(struct command_hdr)) <
        sizeof(struct command_hdr)) {
        fprintf(stderr, ERR_CONN);
        free(id);
        return -2;
    }

    if (res.opcode == (uint8_t) ERR) {
        free(id);
        return -1;
    } else if(res.option == 1) {
        int topics_no, tot_len;
        char header[sizeof(topics_no) + sizeof(tot_len)];
        char buf[BUFSIZ];

        if (recv_all(tcp_sockfd, header, sizeof(topics_no) + sizeof(tot_len)) <
            sizeof(topics_no) + sizeof(tot_len)) {
            fprintf(stderr, ERR_CONN);
            return -2;
        }
        memcpy(&topics_no, header, sizeof(topics_no));
        memcpy(&tot_len, header + sizeof(topics_no), sizeof(tot_len));

        if (!tot_len) {
            free(id);
            return 0;
        }

        if (recv_all(tcp_sockfd, buf, tot_len) < tot_len) {
            fprintf(stderr, ERR_CONN);
            return -2;
        }

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

uint32_t subscribe_topic(char *topic_name, int sf, int tcp_sockfd)
{
    if (get_topic_idx(get_topic_id(topic_name)) != -1) {
        return -2;
    }

    struct command_hdr sub_topic;
    if (send_command(tcp_sockfd, REQ_SUB, sf, strlen(topic_name) + 1) == -1) {
        fprintf(stderr, ERR_CONN);
        return -3;
    }

    if (send_all(tcp_sockfd, topic_name, strlen(topic_name) + 1) <
        strlen(topic_name) + 1) {
        fprintf(stderr, ERR_CONN);
        return -3;
    }

    if (recv_all(tcp_sockfd, &sub_topic, sizeof(struct command_hdr)) <
        sizeof(struct command_hdr)) {
        fprintf(stderr, ERR_CONN);
        return -3;
    }

    if (sub_topic.opcode == (uint8_t) ERR) {
        return -1;
    }

    return sub_topic.option;
}

uint32_t unsubscribe_topic(uint32_t topic_id, int tcp_sockfd)
{
    if (!get_topic_name(topic_id)) {
        return -1;
    }

    struct command_hdr unsub_topic;
    send_command(tcp_sockfd, REQ_UNSUB, topic_id, 0);
    if (recv_all(tcp_sockfd, &unsub_topic, sizeof(struct command_hdr)) <
        sizeof(struct command_hdr)) {
        fprintf(stderr, ERR_CONN);
        return -2;
    }

    if (unsub_topic.opcode == (uint8_t) ERR) {
        return -1;
    }

    return unsub_topic.option;
}

int show_available_topics(int tcp_sockfd)
{
    int topics_no, tot_len;
    char header[sizeof(topics_no) + sizeof(tot_len)];

    if (send_command(tcp_sockfd, REQ_TOPICS, 0, 0) == -1) {
        fprintf(stderr, ERR_CONN);
        return -1;
    }

    if (recv_all(tcp_sockfd, header, sizeof(topics_no) + sizeof(tot_len)) <
        sizeof(topics_no) + sizeof(tot_len)) {
        fprintf(stderr, ERR_CONN);
        return -1;
    }

    memcpy(&topics_no, header, sizeof(topics_no));
    memcpy(&tot_len, header + sizeof(topics_no), sizeof(tot_len));

    if (!topics_no) {
        printf("No topics available to subscribe to yet.\n");
        return 0;
    }

    char buf[BUFSIZ];
    if (recv_all(tcp_sockfd, buf, tot_len) < tot_len) {
        fprintf(stderr, ERR_CONN);
        return -1;
    }

    int offset = 0;
    printf("Available topics on server:\n");
    for (int i = 0; i < topics_no; i++) {
        char tmp[51];
        strcpy (tmp, buf + offset);
        if (get_topic_id(tmp) == -1) {
            printf("\t[-] %s\n", buf + offset);
        } else {
            printf("\t[+] %s\n", buf + offset);
        }

        offset += strlen(buf + offset) + 1;
    }
    printf("\n");

    return 0;
}

void show_history()
{
    ll_node_t *head = history->head;

    int idx = 1;

    if (!head) {
        printf("No local history.\n");
        return;
    }

    printf("Local history:\n");
    while (head) {
        char *msg = *((char **)head->data);

        printf("\t[%d] %s\n", idx, msg);
        idx += 1;
        head = head->next;
    }
    printf("\n");
}

void save_history()
{
    FILE *save_file = fopen("history_save.txt","a+");

    if (!save_file) {
        fprintf(stderr, "Couldn't open a history save file.\n");
        return;
    }

    ll_node_t *head = history->head;

    if (!head) {
        printf("No history available. Not saving...\n");
        return;
    }

    int idx = 1;

    while (head) {
        char *msg = *((char **)head->data);

        fprintf(save_file, "\t[%d] %s\n", idx, msg);
        idx += 1;
        head = head->next;
    }

    fclose(save_file);
}

void show_client_topics()
{
    ll_node_t *head = subbed_topics->head;

    if (!head) {
        printf("No subscribed topics yet.\n");
        return;
    }

    printf("Currently subscribed topics:\n");
    while (head) {
        struct topic *topic = (struct topic*)head->data;

        printf("\t%s\n", topic->topic_name);

        head = head->next;
    }

    printf("\n");
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

        if (!inet_ntop(AF_INET, &server_msg.ip_addr,
                       ip_addr, sizeof(struct sockaddr_in))) {
            fprintf(stderr, "inet_ntop() failed. Message display may be affected\n");
        }

        uint16_t ntoh_port = ntohs(server_msg.port);
        char *h_msg = malloc(2000);

        switch (server_msg.data_type) {
            case INT: {
                sprintf(h_msg, INT_MSG, ip_addr, ntoh_port,
                                        get_topic_name(server_msg.topic_id),
                                        *((int *)content));
                printf(INT_MSG, ip_addr, ntoh_port,
                                get_topic_name(server_msg.topic_id),
                                *((int *)content));
            }
            break;
            case SHORT_REAL: {
                sprintf(h_msg, SR_MSG, ip_addr, ntoh_port,
                                       get_topic_name(server_msg.topic_id),
                                       *((float *)content));
                printf(SR_MSG, ip_addr, ntoh_port,
                               get_topic_name(server_msg.topic_id),
                               *((float *)content));
            }
            break;
            case FLOAT: {
                sprintf(h_msg, FLT_MSG, ip_addr, ntoh_port,
                                        get_topic_name(server_msg.topic_id),
                                        *((double *)content));
                printf(FLT_MSG, ip_addr, ntoh_port,
                                get_topic_name(server_msg.topic_id),
                                *((double *)content));
            }
            break;
            case STRING: {
                sprintf(h_msg, STR_MSG, ip_addr, ntoh_port,
                                        get_topic_name(server_msg.topic_id),
                                        content);
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
        if (ll_get_size(history) == MAX_HISTORY) {
            ll_remove_nth_node(history, 0);
        }
        ll_add_nth_node(history, MAX_HISTORY + 1, &h_msg);

        offset += sizeof(server_msg) + server_msg.buf_len;
    }
}
