#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#define INT_MSG "%s:%d - %s - INT - %d\n"
#define SR_MSG "%s:%d - %s - SHORT_REAL - %.2f\n"
#define FLT_MSG "%s:%d - %s - FLOAT - %lf\n"
#define STR_MSG "%s:%d - %s - STRING - %s\n"

comm_type parse_command(char *comm, char **tokens);
void add_subbed_topic(uint8_t id, char *topic_name);
int get_topic_id(char *topic_name);
char *get_topic_name(uint8_t topic_id);
int get_topic_idx(uint8_t topic_id);
int verify_id(char *id, int tcp_sockfd);
uint8_t subscribe_topic(char *topic_name, int sf, int tcp_sockfd);
uint8_t unsubscribe_topic(uint8_t topic_id, int tcp_sockfd);
void handle_incoming_msgs(char *buf, int msgs_no, int tot_len);
