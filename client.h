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
    
} client;

struct command_hdr {
    uint8_t opcode;
    uint8_t option_sf;
    uint16_t buf_len;
} __attribute__((__packed__)) command_hdr;