#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#define MAXCONNS 100
#define BACKLOG 200

struct subbed_topic {
    uint8_t sf;
    struct topic info;
} __attribute__((__packed__)) subbed_topic;

struct client {
    char id[11];
    uint32_t fd;
    uint8_t conned;
    linked_list_t *client_topics; // struct subbed_topic
    linked_list_t *msg_queue; // for sf topics
} client;

struct message_t {
    struct message_hdr header;
    char buf[1501];
} message_t;

struct client *get_client(char *id);
int get_msgs_size(linked_list_t *msg_queue);
void send_msgs(int client_fd, linked_list_t *msgs);
struct message_t *parse_msg(int ip, int port, uint8_t data_type, int topic_id, char *buf);
void handle_msg(struct message_t new_msg);
void handle_sf_queue(struct client *curr_client);
void send_subbed_topics(struct client *curr_client);
int add_new_topic(char *topic_name);
uint8_t get_topic_id(char *topic_name);
int get_topic_idx(linked_list_t *client_topics, uint8_t topic_id);
uint8_t get_topic_sf(linked_list_t *client_topics, uint8_t topic_id);
int get_topics_size(linked_list_t *topics);
void sub_client(struct client *curr_client, uint8_t sf, char *topic_name);
void unsub_client(struct client *curr_client, uint8_t topic_id);
