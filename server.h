#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

typedef enum comm_type {
    ERR = -1,
    OK,
    EXIT,
    CHECK_ID,
    SUBSCRIBE,
    UNSUBSCRIBE,
} comm_type;

struct client {
    char id[11];
    uint32_t fd;
    uint8_t serv_conned;
} __attribute__((__packed__)) client ;

struct command_hdr {
    uint8_t opcode;
    uint8_t option_sf;
    uint16_t buf_len;
} __attribute__((__packed__)) command_hdr ;

struct udp_packet {
    char topic[50];
    uint8_t type;
    char content[1500];
} udp_packet;