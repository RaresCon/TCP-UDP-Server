#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

typedef enum comm_type {
    ERR = -1,
    EXIT = 1,
    SUBSCRIBE,
    UNSUBSCRIBE,
} comm_type;

struct client {
    
} client;

struct command {
    uint8_t opcode;
    uint8_t option_sf;
    uint16_t topic_len;
} command;