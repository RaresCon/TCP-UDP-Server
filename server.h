#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

struct udp_packet {
    char topic[50];
    uint8_t type;
    char content[1500];
} udp_packet;