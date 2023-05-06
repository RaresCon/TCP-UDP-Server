#pragma once

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
#include "list.h"

#define STD_RETRIES 10

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

struct command_hdr {
    uint8_t opcode;
    uint8_t option;
    uint16_t buf_len;
} __attribute__((__packed__));

struct topic {
    uint8_t id;
    char topic_name[51];
} __attribute__((__packed__));

struct message_hdr {
    uint32_t ip_addr;
    uint16_t port;
    uint8_t topic_id;
    uint8_t data_type;
    uint16_t buf_len;
} __attribute__((__packed__));

int check_valid_uns_number(char *num);
int send_all(int sockfd, void *buf, int len);
int recv_all(int sockfd, void *buf, int len);
int send_command(int sockfd, comm_type type, uint8_t option, uint16_t buf_len);
int init_socket(int addr_fam, int sock_type, int flags);
int init_epoll(int max_events);
int add_event(int epoll_fd, int ev_fd, struct epoll_event *new_ev);
