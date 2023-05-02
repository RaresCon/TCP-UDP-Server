#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include "list.h"

typedef enum comm_type {
    ERR = -1,
    OK,
    EXIT,
    CHECK_ID,
    SUBSCRIBE,
    UNSUBSCRIBE,
} comm_type;

typedef enum data_type {
    INT,
    SHORT_REAL,
    FLOAT,
    STRING,
} data_type;

struct topic {
    char topic_name[51];
    uint8_t id;
} __attribute__((__packed__)) topic;

struct subbed_topic {
    struct topic info;
    uint8_t sf;
} __attribute__((__packed__)) subbed_topic;

struct message_hdr {
    uint32_t ip_addr;
    uint16_t port;
    uint8_t topic_id;
    uint8_t data_type;
    uint8_t buf_len;
} __attribute__((__packed__)) message_hdr; 

struct message_t {
    struct message_hdr header;
    char buf[1500];
} message_t;

struct client {
    char id[11];
    uint32_t fd;
    uint8_t serv_conned;
    linked_list_t *client_topics; // struct subbed_topic
    linked_list_t *msg_queue; // for sf topics
} client;

struct command_hdr {
    uint8_t opcode;
    uint8_t option_sf;
    uint16_t buf_len;
} __attribute__((__packed__)) command_hdr;
