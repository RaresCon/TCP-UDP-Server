#include "common.h"
#include "server.h"

int16_t current_port = -1;
linked_list_t *registered_users;
linked_list_t *topics;

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int rc, tcp_sockfd, udp_sockfd, epoll_fd;
	registered_users = ll_create(sizeof(struct client*));
	topics = ll_create(sizeof(struct topic));
    struct sockaddr_in servaddr;
	struct epoll_event ev;
	struct epoll_event *events_list;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
		return 1;
	}

    current_port = check_valid_uns_number(argv[1]);
	if (current_port == -1) {
		perror("Invalid server port. Aborting...\n");
		exit(EXIT_FAILURE);
	}

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(current_port);

    /* TCP Socket INIT */
    tcp_sockfd = init_socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_sockfd == -1) {
		exit(EXIT_FAILURE);
	}

	/* Setting TCP Socket as non-blocking */
	int current_tcp_flags = fcntl(tcp_sockfd, F_GETFL, 0);
	if (current_tcp_flags == -1) {
		perror("fcntl(F_GETFL) failed.");
		exit(EXIT_FAILURE);
	} else if (fcntl(tcp_sockfd, F_SETFL, current_tcp_flags | O_NONBLOCK) == -1) {
		perror("fcntl(F_SETFL) failed.");
		exit(EXIT_FAILURE);
	}

    int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("SO_REUSEADDR error\n");
    }
	if (setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
		perror("TCP_NODELAY error\n");
	}

    if (bind(tcp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (bind(tcp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("bind() failed. Aborting...\n");
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }
	}

	/* UDP Socket INIT */
    udp_sockfd = init_socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_sockfd == -1) {
		exit(EXIT_FAILURE);
	}

	if (setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("SO_REUSEADDR error");
    }

    if (bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        int retries = STD_RETRIES;
        while (bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("bind() failed. Aborting...\n");
            exit(EXIT_FAILURE);
        }
    }

	epoll_fd = init_epoll(MAXCONNS);
	if (epoll_fd == -1) {
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = tcp_sockfd;
	rc = add_event(epoll_fd, tcp_sockfd, &ev);
	if (rc == -1) {
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = udp_sockfd;
	rc = add_event(epoll_fd, udp_sockfd, &ev);
	if (rc == -1) {
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;
	rc = add_event(epoll_fd, STDIN_FILENO, &ev);
	if (rc == -1) {
		exit(EXIT_FAILURE);
	}

	events_list = calloc(MAXCONNS, sizeof(ev));
	while (1) {
		int num_of_events = epoll_wait(epoll_fd, events_list, MAXCONNS, 100);
		if(num_of_events == -1) {
			perror("epoll_wait() failed. Aborting...");
			break;
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
				recv_all(new_client_fd, &check_client, sizeof(struct command_hdr));

				if (check_client.opcode == ERR) {
					fprintf(stderr, "Error opcode.\n");

					close(new_client_fd);
					continue;
				} else if (check_client.opcode != CHECK_ID) {
					fprintf(stderr, "Invalid opcode.\n");

					send_command(new_client_fd, ERR, 0, 0);
					close(new_client_fd);
					continue;
				}

				char *buf = malloc(check_client.buf_len);
				if (!recv_all(new_client_fd, buf, check_client.buf_len)) {
					fprintf(stderr, "New client closed connection before registering.\n");
					close(new_client_fd);
					continue;
				}

				struct client *old_client = get_client(buf);
				if (old_client && old_client->conned) {
					printf("Client %s already connected.\n", buf);

					send_command(new_client_fd, ERR, 0, 0);
					close(new_client_fd);
					continue;
				} else if (old_client) {
					send_command(new_client_fd, OK, 1, 0);

					old_client->fd = new_client_fd;
					old_client->conned = 1;
					send_subbed_topics(old_client);
					handle_sf_queue(old_client);

					ev.events = EPOLLIN;
					ev.data.ptr = old_client;
					rc = add_event(epoll_fd, old_client->fd, &ev);
				} else {
					send_command(new_client_fd, OK, 0, 0);

					struct client *new_client = calloc(1, sizeof(struct client));
					new_client->conned = 1;
					strcpy(new_client->id, buf);
					new_client->fd = new_client_fd;
					new_client->client_topics = ll_create(sizeof(struct subbed_topic));
					new_client->msg_queue = ll_create(sizeof(struct message_t));

					ll_add_nth_node(registered_users, 0, &new_client);
					ev.events = EPOLLIN;
					ev.data.ptr = new_client;
					rc = add_event(epoll_fd, new_client->fd, &ev);
				}

				char ip_addr[30];
				if (!inet_ntop(AF_INET, &new_client_addr.sin_addr, ip_addr, addr_size)) {
					fprintf(stderr, "inet_ntop() failed. Aborting...\n");
					free(buf);	
					continue;
				}

				printf("New client %s connected from %s:%d.\n", buf, ip_addr, new_client_addr.sin_port);
				free(buf);
			} else if (events_list[i].data.fd == udp_sockfd) {
				struct sockaddr_in udp_client;
				socklen_t addr_size = sizeof(struct sockaddr_in);

				char new_content[1551];
				memset(new_content, 0, 1551);
				int rc = recvfrom(udp_sockfd, &new_content,
								  sizeof(new_content), 0,
								  (struct sockaddr *) &udp_client, &addr_size);

				if (rc < 0) {
					fprintf(stderr, "recv() error\n");
					continue;
				}

				char topic_name[51];
				uint8_t data_type;

				char *topic_delim = strchr(new_content, '\0');

				if (topic_delim) {
					strcpy(topic_name, new_content);
				} else {
					memcpy(topic_name, new_content, 50);
					topic_name[51] = '\0';
				}
				int topic_id = add_new_topic(topic_name);
				memcpy(&data_type, (new_content + 50), 1);

				struct message_t *msg = parse_msg(udp_client.sin_addr.s_addr,
												  udp_client.sin_port,
												  data_type,
												  topic_id,
												  new_content);

				handle_msg(*msg);
			} else if (events_list[i].data.fd == STDIN_FILENO) {
				char command[BUFSIZ];
				fgets(command, BUFSIZ, stdin);

				if (strlen(command) != 5 || strcmp(command, "exit\n")) {
					printf("Invalid command. Please try again.\n");
					continue;
				}

				ll_free(&registered_users);
				ll_free(&topics);
				free(events_list);
				close(tcp_sockfd);
				close(udp_sockfd);
				close(epoll_fd);

				return 0;
			} else {
				int rc;
				struct client *curr_client = (struct client*)events_list[i].data.ptr;
				struct command_hdr command_from_client;

				rc = recv_all(curr_client->fd, &command_from_client, sizeof(command_from_client));

				if (!rc) {
					printf("Client %s disconnected.\n", curr_client->id);
					curr_client->conned = 0;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events_list[i].data.fd, NULL);
					close(curr_client->fd);
					continue;
				}

				switch (command_from_client.opcode) {
				case SUBSCRIBE: {
					char *buf = malloc(command_from_client.buf_len);
					recv_all(curr_client->fd, buf, command_from_client.buf_len);
					sub_client(curr_client, command_from_client.option, buf);
					free(buf);
				}
				break;
				case UNSUBSCRIBE: {
					unsub_client(curr_client, command_from_client.option);
				}
				break;
				default:
					fprintf(stderr, "Unrecognized client request.\n");
				break;
				}
			}
		}
	}

	ll_free(&registered_users);
	ll_free(&topics);
	free(events_list);
	close(tcp_sockfd);
	close(udp_sockfd);
	close(epoll_fd);
	
	return 0;
}

int add_new_topic(char *topic_name)
{
	ll_node_t *head = topics->head;
	
	while (head) {
		struct topic *curr_topic = (struct topic*)head->data;
		if (!strcmp(curr_topic->topic_name, topic_name)) {
			return curr_topic->id;
		}
		head = head->next;
	}

	struct topic new_topic;
	new_topic.id = ll_get_size(topics) + 1;
	strcpy(new_topic.topic_name, topic_name);
	ll_add_nth_node(topics, 0, &new_topic);

	return new_topic.id;
}

uint8_t get_topic_id(char *topic_name)
{
	ll_node_t *head = topics->head;
	
	while (head) {
		struct topic *curr_topic = (struct topic*)head->data;
		if (!strcmp(curr_topic->topic_name, topic_name)) {
			return curr_topic->id;
		}
		head = head->next;
	}

	return -1;
}

int get_topic_idx(linked_list_t *client_topics, uint8_t topic_id)
{
	ll_node_t *head = client_topics->head;
	int idx = 0;
	
	while (head) {
		struct subbed_topic *curr_topic = (struct subbed_topic*)head->data;
		if (curr_topic->info.id == topic_id) {
			return idx;
		}
		idx += 1;
		head = head->next;
	}

	return -1;
}

uint8_t get_topic_sf(linked_list_t *client_topics, uint8_t topic_id)
{
	ll_node_t *head = client_topics->head;

	while (head) {
		struct subbed_topic *curr_topic = (struct subbed_topic*)head->data;
		if (curr_topic->info.id == topic_id) {
			return curr_topic->sf;
		}
		head = head->next;
	}

	return -1;
}

struct client *get_client(char *id)
{
	ll_node_t *head = registered_users->head;

	while (head) {
		struct client *curr_client = *((struct client**)head->data);
		if (!strcmp(curr_client->id, id)) {
			return curr_client;
		}
		head = head->next;
	}

	return NULL;
}

struct message_t *parse_msg(int ip, int port, uint8_t data_type, int topic_id, char *buf)
{
	struct message_t *msg = calloc(1, sizeof(struct message_t));

	msg->header.ip_addr = ip;
	msg->header.port = port;
	msg->header.data_type = data_type;
	msg->header.topic_id = topic_id;

	switch (data_type) {
		case INT: {
			uint8_t sign;
			uint32_t number;

			memcpy(&sign, (buf + 51), sizeof(sign));
			memcpy(&number, (buf + 52), sizeof(number));
			number = ntohl(number);
			if (sign) {
				number = -number;
			}

			msg->header.buf_len = sizeof(number);
			memcpy(msg->buf, &number, sizeof(number));
		}
		break;
		case SHORT_REAL: {
			uint16_t number;
			memcpy(&number, (buf + 51), sizeof(uint16_t));
			number = ntohs(number);

			float short_real = (float) number / 100;

			msg->header.buf_len = sizeof(short_real);
			memcpy(msg->buf, &short_real, sizeof(short_real));
		}
		break;
		case FLOAT: {
			uint8_t sign, pow;
			uint32_t number;

			memcpy(&sign, (buf + 51), sizeof(sign));
			memcpy(&number, (buf + 52), sizeof(number));
			number = ntohl(number);
			memcpy(&pow, (buf + 52 + sizeof(number)), sizeof(pow));

			int div_pow = 1;
			for (int i = 0; i < pow; i++) {
				div_pow *= 10;
			}
			double real = (double) number / div_pow;

			if (sign) {
				real = -real;
			}

			msg->header.buf_len = sizeof(real);
			memcpy(msg->buf, &real, sizeof(real));
		}
		break;
		case STRING: {
			msg->header.buf_len = strlen(buf + 51) + 1;
			strcpy(msg->buf, (buf + 51));
		}
		break;
		default:
			fprintf(stderr, "Unrecognized data type from UDP client - dropping.\n");
			return NULL;
		break;
	}

	return msg;
}

int get_msgs_size(linked_list_t *msg_queue)
{
	int size = ll_get_size(msg_queue) * sizeof(struct message_hdr);
	ll_node_t *head = msg_queue->head;

	while (head) {
		struct message_t *curr_msg = (struct message_t *)head->data;
		size += curr_msg->header.buf_len;

		head = head->next;
	}

	return size;
}

int get_topics_size(linked_list_t *topics)
{
	int size = ll_get_size(topics) * (2 * sizeof(uint8_t));
	ll_node_t *head = topics->head;

	while (head) {
		struct subbed_topic *curr_topic = (struct subbed_topic *)head->data;
		size += strlen(curr_topic->info.topic_name) + 1;

		head = head->next;
	}

	return size;
}

void send_subbed_topics(struct client *curr_client)
{
	int topics_no, tot_len;
	topics_no = ll_get_size(curr_client->client_topics);
	tot_len = get_topics_size(curr_client->client_topics) + topics_no * sizeof(int);

	char header[sizeof(topics_no) + sizeof(tot_len)];
	memcpy(header, &topics_no, sizeof(topics_no));
	memcpy(header + sizeof(topics_no), &tot_len, sizeof(tot_len));
	send_all(curr_client->fd, header, sizeof(topics_no) + sizeof(tot_len));

	if (topics_no == 0) {
		return;
	}

	int offset = 0;
	char buf[BUFSIZ];
	ll_node_t *head = curr_client->client_topics->head;

	for (int i = 0; i < topics_no; i++) {
		struct topic curr_topic = ((struct subbed_topic *)head->data)->info;
		int name_len, topic_len;
		name_len = strlen(curr_topic.topic_name) + 1;
		topic_len = sizeof(curr_topic.id) + name_len;

		memcpy(buf + offset, &name_len, sizeof(name_len));
		memcpy(buf + offset + sizeof(name_len), &curr_topic, topic_len);

		offset += sizeof(name_len) + topic_len;
		head = head->next;
	}

	send_all(curr_client->fd, buf, tot_len);
}

void send_msgs(int client_fd, linked_list_t *msgs)
{
	int msgs_no = ll_get_size(msgs);
	int tot_len = get_msgs_size(msgs);

	char header[sizeof(msgs_no) + sizeof(tot_len)];
	memcpy(header, &msgs_no, sizeof(msgs_no));
	memcpy(header + sizeof(msgs_no), &tot_len, sizeof(tot_len));
	send_all(client_fd, header, sizeof(msgs_no) + sizeof(tot_len));

	int offset = 0;
	char buf[BUFSIZ];
	ll_node_t *head = msgs->head;

	for (int i = 0; i < msgs_no; i++) {
		struct message_t *curr_msg = (struct message_t*)head->data;

		memcpy(buf + offset, &curr_msg->header, sizeof(struct message_hdr));
		memcpy(buf + offset + sizeof(struct message_hdr), curr_msg->buf, curr_msg->header.buf_len);
		offset += sizeof(struct message_hdr) + curr_msg->header.buf_len;

		head = head->next;
	}

	send_all(client_fd, buf, tot_len);
}

void handle_msg(struct message_t new_msg)
{
	ll_node_t *head = registered_users->head;
	linked_list_t *tmp = ll_create(sizeof(new_msg));
	ll_add_nth_node(tmp, 0, &new_msg);

	while (head) {
		struct client *curr_client = *((struct client**)head->data);
		
		if (get_topic_idx(curr_client->client_topics, new_msg.header.topic_id) != -1 &&
			curr_client->conned) {
			send_msgs(curr_client->fd, tmp);
		} else if (!curr_client->conned &&
				   get_topic_sf(curr_client->client_topics, new_msg.header.topic_id) == (uint8_t) 1) {
			ll_add_nth_node(curr_client->msg_queue, ll_get_size(curr_client->msg_queue), &new_msg);
		}
		head = head->next;
	}

	ll_free(&tmp);
}

void handle_sf_queue(struct client *curr_client)
{
	linked_list_t *msg_queue = curr_client->msg_queue;

	if (!ll_get_size(msg_queue)) {
		return;
	}

	send_msgs(curr_client->fd, msg_queue);
	ll_free_elems(&curr_client->msg_queue);
}

void sub_client(struct client *curr_client, uint8_t sf, char *topic_name)
{
	uint8_t id = get_topic_id(topic_name);

	if (id == (uint8_t) -1) {
		send_command(curr_client->fd, ERR, 0, 0);
		return;
	}

	struct subbed_topic client_topic;
	client_topic.info.id = id;
	strcpy(client_topic.info.topic_name, topic_name);
	client_topic.sf = sf;
	ll_add_nth_node(curr_client->client_topics, 0, &client_topic);

	send_command(curr_client->fd, OK, id, 0);
}

void unsub_client(struct client *curr_client, uint8_t topic_id)
{
	int topic_idx = get_topic_idx(curr_client->client_topics, topic_id);

	if (topic_idx == -1) {
		send_command(curr_client->fd, ERR, 0, 0);
		return;
	}
	ll_remove_nth_node(curr_client->client_topics, topic_idx);

	send_command(curr_client->fd, OK, 0, 0);
}
