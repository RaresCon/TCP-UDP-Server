#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

struct client {
    char id[11];
    uint32_t fd;
    uint8_t serv_conned;
} client;

struct command_header {
    uint8_t opcode;
    uint8_t option_sf;
    uint16_t topic_len;
} command_header;

struct udp_packet {
    char topic[50];
    uint8_t type;
    char content[1500];
} udp_packet;