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
#include "server.h"
#include "list.h"

#define MAXCONNS 100
#define BACKLOG 200
#define STD_RETRIES 10

int16_t current_port = -1;
linked_list_t *registered_users;
linked_list_t *topics;

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
	send(curr_client->fd, header, sizeof(topics_no) + sizeof(tot_len), 0);

	if (topics_no == 0) {
		return;
	}

	int offset = 0;
	char buf[BUFSIZ];
	ll_node_t *head = curr_client->client_topics->head;

	for (int i = 0; i < topics_no; i++) {
		struct subbed_topic *curr_topic = (struct subbed_topic *)head->data;
		int len = strlen(curr_topic->info.topic_name) + 1;

		memcpy(buf + offset, &len, sizeof(len));
		memcpy(buf + offset + sizeof(len), &curr_topic->sf, sizeof(curr_topic->sf));
		memcpy(buf + offset + sizeof(len) + sizeof(curr_topic->sf), &curr_topic->info.id, sizeof(curr_topic->info.id));
		memcpy(buf + offset + sizeof(len) + sizeof(curr_topic->sf) + sizeof(curr_topic->info.id),  curr_topic->info.topic_name, len);

		offset += sizeof(len) + sizeof(curr_topic->sf) + sizeof(curr_topic->info.id) + len;
		head = head->next;
	}

	send(curr_client->fd, buf, tot_len, 0);
}

void send_msg(struct message_t new_msg)
{
	ll_node_t *head = registered_users->head;

	while (head) {
		struct client *curr_client = *((struct client**)head->data);
		
		if (get_topic_idx(curr_client->client_topics, new_msg.header.topic_id) != -1 &&
			curr_client->serv_conned) {
			int no, tot_len;
			no = 1;
			tot_len = sizeof(struct message_hdr) + new_msg.header.buf_len;

			char header[sizeof(no) + sizeof(tot_len)];
			memcpy(header, &no, sizeof(no));
			memcpy(header + sizeof(no), &tot_len, sizeof(tot_len));

			char buf[sizeof(struct message_hdr) + new_msg.header.buf_len];
			memcpy(buf, &new_msg.header, sizeof(struct message_hdr));
			memcpy(buf + sizeof(struct message_hdr), new_msg.buf, new_msg.header.buf_len);

			send(curr_client->fd, header, sizeof(no) + sizeof(tot_len), 0);
			send(curr_client->fd, buf, tot_len, 0);
		} else if (!curr_client->serv_conned &&
				   get_topic_sf(curr_client->client_topics, new_msg.header.topic_id) == (uint8_t) 1) {
			ll_add_nth_node(curr_client->msg_queue, ll_get_size(curr_client->msg_queue), &new_msg);
		}
		head = head->next;
	}
}

void send_sf_msgs(struct client *curr_client)
{
	linked_list_t *msg_queue = curr_client->msg_queue;
	int msg_no, tot_len;
	msg_no = ll_get_size(msg_queue);
	tot_len = get_msgs_size(msg_queue);

	char header[sizeof(msg_no) + sizeof(tot_len)];
	memcpy(header, &msg_no, sizeof(msg_no));
	memcpy(header + sizeof(msg_no), &tot_len, sizeof(tot_len));
	send(curr_client->fd, header, sizeof(msg_no) + sizeof(tot_len), 0);

	if (msg_no == 0) {
		return;
	}

	int offset = 0;
	char buf[BUFSIZ];
	ll_node_t *head = msg_queue->head;

	for (int i = 0; i < msg_no; i++) {
		struct message_t *curr_msg = (struct message_t*)head->data;

		memcpy(buf + offset, &curr_msg->header, sizeof(struct message_hdr));
		memcpy(buf + offset + sizeof(struct message_hdr), curr_msg->buf, curr_msg->header.buf_len);
		offset += sizeof(struct message_hdr) + curr_msg->header.buf_len;

		head = head->next;
	}

	send(curr_client->fd, buf, tot_len, 0);
	ll_free_elems(&curr_client->msg_queue);
}

void sub_client(struct client *curr_client, uint8_t sf, char *topic_name)
{
	struct command_hdr res;
	uint8_t rc = get_topic_id(topic_name);

	if (rc == (uint8_t) -1) {
		res.opcode = ERR;
		res.option_sf = 0;
		res.buf_len = 0;

		send(curr_client->fd, &res, sizeof(struct command_hdr), 0);
		return;
	}

	struct subbed_topic client_topic;
	client_topic.info.id = rc;
	strcpy(client_topic.info.topic_name, topic_name);
	client_topic.sf = sf;
	ll_add_nth_node(curr_client->client_topics, 0, &client_topic);

	res.opcode = OK;
	res.option_sf = rc;
	res.buf_len = 0;
	send(curr_client->fd, &res, sizeof(struct command_hdr), 0);
}

void unsub_client(struct client *curr_client, uint8_t topic_id)
{
	struct command_hdr res;
	int topic_idx = get_topic_idx(curr_client->client_topics, topic_id);

	if (topic_idx == -1) {
		res.opcode = ERR;
		res.option_sf = 0;
		res.buf_len = 0;

		send(curr_client->fd, &res, sizeof(struct command_hdr), 0);
		return;
	}

	res.opcode = OK;
	res.option_sf = 0;
	res.buf_len = 0;
	send(curr_client->fd, &res, sizeof(struct command_hdr), 0);

	ll_remove_nth_node(curr_client->client_topics, topic_idx);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int tcp_sockfd, udp_sockfd, epoll_fd;
	registered_users = ll_create(sizeof(struct client*));
	topics = ll_create(sizeof(struct topic));
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
		exit(EXIT_FAILURE);
	}

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
            exit(EXIT_FAILURE);
        }
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
        perror("SO_REUSEADDR");
    }
	if (setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
		perror("TCP_NODELAY");
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

	ev.events = EPOLLIN;
	ev.data.fd = tcp_sockfd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0) {
		int retries = STD_RETRIES;
        while (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockfd, &ev) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("epoll_ctl(EPOLL_CTL_ADD) failed. Aborting...\n");
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }
	}

	events_list = calloc(MAXCONNS, sizeof(ev));
	while (1) {
		int num_of_events = epoll_wait(epoll_fd, events_list, MAXCONNS, 100);
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

				struct client *old_client = get_client(buf);
				if (old_client && old_client->serv_conned) {
					printf("Client %s already connected.\n", buf);
					check_client.opcode = ERR;
					check_client.option_sf = 0;
					check_client.buf_len = 0;

					send(new_client_fd, &check_client, sizeof(struct command_hdr), 0);
					close(new_client_fd);
					continue;
				} else if (old_client) {
					check_client.opcode = OK;
					check_client.option_sf = 1;
					check_client.buf_len = 0;
					send(new_client_fd, &check_client, sizeof(struct command_hdr), 0);

					old_client->fd = new_client_fd;
					old_client->serv_conned = 1;
					send_subbed_topics(old_client);
					send_sf_msgs(old_client);

					ev.events = EPOLLIN;
					ev.data.ptr = old_client;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &ev) < 0) {
						fprintf(stderr, "Failed to insert socket into epoll.\n");
						close(old_client->fd);
					}
				} else {
					check_client.opcode = OK;
					check_client.option_sf = 0;
					check_client.buf_len = 0;
					send(new_client_fd, &check_client, sizeof(struct command_hdr), 0);

					struct client *new_client = calloc(1, sizeof(struct client));
					new_client->serv_conned = 1;
					strcpy(new_client->id, buf);
					new_client->fd = new_client_fd;
					new_client->client_topics = ll_create(sizeof(struct subbed_topic));
					new_client->msg_queue = ll_create(sizeof(struct message_t));

					ll_add_nth_node(registered_users, 0, &new_client);
					ev.events = EPOLLIN;
					ev.data.ptr = new_client;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client->fd, &ev) < 0) {
						fprintf(stderr, "Failed to insert socket into epoll.\n");
					}
				}

				char ip_addr[30];
				if (!inet_ntop(AF_INET, &new_client_addr.sin_addr, ip_addr, addr_size)) {
					fprintf(stderr, "inet_ntop() failed. Aborting...\n");
					exit(EXIT_FAILURE);
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
					exit(EXIT_FAILURE);
				}

				char topic_name[51];
				uint8_t data_type;
				struct message_t msg;

				char *topic_delim = strchr(new_content, '\0');

				if (topic_delim) {
					strcpy(topic_name, new_content);
				} else {
					memcpy(topic_name, new_content, 50);
					topic_name[51] = '\0';
				}
				int topic_id = add_new_topic(topic_name);
				memcpy(&data_type, (new_content + 50), 1);

				msg.header.ip_addr = udp_client.sin_addr.s_addr;
				msg.header.port = udp_client.sin_port;
				msg.header.data_type = data_type;
				msg.header.topic_id = topic_id;

				switch (data_type) {
				case INT: {
					uint8_t sign;
					uint32_t number;

					memcpy(&sign, (new_content + 51), sizeof(sign));
					memcpy(&number, (new_content + 52), sizeof(number));
					number = ntohl(number);
					if (sign) {
						number = -number;
					}

					msg.header.buf_len = sizeof(number);
					memcpy(msg.buf, &number, sizeof(number));
				}
				break;
				case SHORT_REAL: {
					uint16_t number;
					memcpy(&number, (new_content + 51), sizeof(uint16_t));
					number = ntohs(number);

					float short_real = (float) number / 100;

					msg.header.buf_len = sizeof(short_real);
					memcpy(msg.buf, &short_real, sizeof(short_real));
				}
				break;
				case FLOAT: {
					uint8_t sign, pow;
					uint32_t number;

					memcpy(&sign, (new_content + 51), sizeof(sign));
					memcpy(&number, (new_content + 52), sizeof(number));
					number = ntohl(number);
					memcpy(&pow, (new_content + 52 + sizeof(number)), sizeof(pow));

					int div_pow = 1;
					for (int i = 0; i < pow; i++) {
						div_pow *= 10;
					}
					double real = (double) number / div_pow;

					if (sign) {
						real = -real;
					}

					msg.header.buf_len = sizeof(real);
					memcpy(msg.buf, &real, sizeof(real));
				}
				break;
				case STRING: {
					msg.header.buf_len = strlen(new_content + 51) + 1;
					strcpy(msg.buf, (new_content + 51));
				}
				break;
				default:
					fprintf(stderr, "Unrecognized data type from UDP client - dropping.\n");
				break;
				}

				send_msg(msg);
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

				rc = recv(curr_client->fd, &command_from_client, sizeof(command_from_client), 0);

				if (!rc) {
					printf("Client %s disconnected.\n", curr_client->id);
					curr_client->serv_conned = 0;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events_list[i].data.fd, NULL);
					close(curr_client->fd);
					continue;
				}

				switch (command_from_client.opcode) {
				case SUBSCRIBE: {
					char *buf = malloc(command_from_client.buf_len);
					recv(curr_client->fd, buf, command_from_client.buf_len, 0);
					sub_client(curr_client, command_from_client.option_sf, buf);
					free(buf);
				}
				break;
				case UNSUBSCRIBE: {
					unsub_client(curr_client, command_from_client.option_sf);
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