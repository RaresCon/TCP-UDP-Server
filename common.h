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

/* Message header */
struct message_hdr {
    uint32_t ip_addr;       /* IP address of the UDP client */
    uint16_t port;          /* Port of the UDP client */
    uint8_t topic_id;       /* Message's id topic */
    uint8_t data_type;      /* Data type of the message */
    uint16_t buf_len;       /* Message length after header */
} __attribute__((__packed__));

struct topic {
    uint8_t id;             /* Id of topic */
    char topic_name[51];    /* Name of topic */
} __attribute__((__packed__));


/*
 * @brief Utility function to check if a string contains only
 * a valid unsigned number
 * 
 * @param num the string that will be checked
 * 
 * @return the number if it is valid, -1 otherwise
 */
int check_valid_uns_number(char *num);


/*
 * @brief Function to parse a command given at STDIN
 * 
 * @param comm the given command
 * @param tokens the array in which the function will save
 * the tokens of the command
 * 
 * @return the command type if the command is valid, ERR otherwise
 */
comm_type parse_command(char *comm, char **tokens);


/*
 * @brief Function to force `send` to work with a fixed length.
 * This function will work with blocking/non-blocking TCP sockets
 * without being concerned about the size of the socket buffer
 * 
 * @param sockfd the socket on which to send the buffer
 * @param buf the buffer to be sent
 * @param len the length of the buffer
 * 
 * @return the number of bytes sent
 */
int send_all(int sockfd, void *buf, int len);


/*
 * @brief Function to force `send` to work with a fixed length.
 * This function will work with blocking/non-blocking TCP sockets
 * without being concerned about the size of the socket buffer
 * 
 * @param sockfd the socket from which to receive the buffer
 * @param buf the buffer to which the function will save received data
 * @param len the length of the buffer to be received
 * 
 * @return the number of bytes received
 */
int recv_all(int sockfd, void *buf, int len);


/*
 * @brief Function to send a request/response 
 * 
 * @param sockfd the socket on which to send
 * @param type type of request/response
 * @param option optional data to store in the `option` field
 * @param buf_len length of the buffer coming after this header, if any
 * 
 * @return 0 if the request/response was sent successfully, -1 otherwise
 */
int send_command(int sockfd, comm_type type, uint8_t option, uint16_t buf_len);


/*
 * @brief Function to create a new socket
 * 
 * @param addr_fam the address family for the socket
 * @param sock_type the type of socket that will be created
 * @param flags the flags to set for the socket
 * 
 * @return file descriptor of the socke if it was
 * successfully created, -1 otherwise
 */
int init_socket(int addr_fam, int sock_type, int flags);


/*
 * @brief Function to create an epoll handler
 * 
 * @param max_events the maximum number of events that
 * could be added to the handler
 * 
 * @return file descriptor of the handler if it was
 * successfully created, -1 otherwise
 */
int init_epoll(int max_events);


/*
 * @brief Function to add a new event to an epoll handler
 * 
 * @param epoll_fd the epoll handler
 * @param ev_fd file descriptor for which this event is added
 * @param new_ev new event structure to be added to epoll
 * 
 * @return 0 if the event was added successfully, -1 otherwise
 */
int add_event(int epoll_fd, int ev_fd, struct epoll_event *new_ev);


/*
 * @brief Function to remove an event from an epoll handler
 * 
 * @param epoll_fd the epoll handler
 * @param ev_fd file descriptor for which this event is added
 * 
 * @return 0 if the event was removed successfully, -1 otherwise
 */
int rm_event(int epoll_fd, int ev_fd);
