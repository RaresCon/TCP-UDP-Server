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
    ERR = -1,               /* Error code */
    OK,                     /* OK code */
    EXIT,                   /* Exit command code */
    CHECK_ID,               /* Check connected id code */
    SUBSCRIBE,              /* Subscribe command code */
    UNSUBSCRIBE,            /* Unsubscribe command code */
} comm_type;

typedef enum data_type {
    INT,                    /* Integer data type */
    SHORT_REAL,             /* Float .2 data type */
    FLOAT,                  /* Double data type */
    STRING,                 /* String data type */
} data_type;

/* Request/Response header */
struct command_hdr {
    uint8_t opcode;         /* Operation code */
    uint8_t option;         /* Optional data field */
    uint16_t buf_len;       /* Optional lenght field for buffer after header */
} __attribute__((__packed__));

struct topic {
    uint8_t id;             /* Id of topic */
    char topic_name[51];    /* Name of topic */
} __attribute__((__packed__));


/* Message header */
struct message_hdr {
    uint32_t ip_addr;   /* IP address of the UDP client */
    uint16_t port;      /* Port of the UDP client */
    uint8_t topic_id;   /* Message's id topic */
    uint8_t data_type;  /* Data type of the message */
    uint16_t buf_len;   /* Message length after header */
} __attribute__((__packed__));


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int check_valid_uns_number(char *num);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int send_all(int sockfd, void *buf, int len);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int recv_all(int sockfd, void *buf, int len);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int send_command(int sockfd, comm_type type, uint8_t option, uint16_t buf_len);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int init_socket(int addr_fam, int sock_type, int flags);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int init_epoll(int max_events);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
int add_event(int epoll_fd, int ev_fd, struct epoll_event *new_ev);
