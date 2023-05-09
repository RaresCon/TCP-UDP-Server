#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#define MAXCONNS 100
#define BACKLOG 200

#define ERR_CONN "The client closed connection\n"

#define CONN_STR "New client %s connected from %s:%d.\n"
#define DCONN_STR "Client %s disconnected.\n"

/*
 * @extends comm_type with private commands for server admin
 */
typedef enum admin_comm_type {
    ERR_ADMIN = -2,               /* Error code */
    EXIT_ADMIN = 1,               /* Exit code */
    SHOW_TOPICS = 5,              /* Show stored topics code */
    SHOW_CLIENTS,                 /* Show stored clients code */
} admin_comm_type;

struct subbed_topic {
    uint8_t sf;                   /* Store-and-Forward flag */
    struct topic info;            /* info about the subscribed topic */
} __attribute__((__packed__));

struct client {
    char id[11];                  /* Id of the client */
    uint32_t fd;                  /* Current fd of the client */
    uint8_t conned;               /* Connection flag */
    linked_list_t *client_topics; /* List of subscribed topics */
    linked_list_t *msg_queue;     /* List of stored messages */
};

struct message_t {
    struct message_hdr header;    /* Header of a message */
    char buf[1501];               /* The message */
};


/*
 * @brief Function to parse an admin command 
 * 
 * @param nr the number of tokens
 * @param tokens the array of tokens 
 * 
 * @return the command type if the command is valid, ERR otherwise
 */
admin_comm_type parse_command(int nr, char **tokens);


/*
 * @brief Function to get a stored client by id
 * 
 * @param id the id of a client
 * 
 * @return pointer to the client structure or NULL if there
 * is no client registered on the server with the given id
 */
struct client *get_client(char *id);


/*
 * @brief Function to get the size of a messages queue,
 * including the size of the data stored in it
 * 
 * @param msg_queue the list of messages
 * 
 * @return the size of the messages queue
 */
int get_msgs_size(linked_list_t *msg_queue);


/*
 * @brief Function to send a list of messages to a client
 * 
 * @param curr_client the client to which the messages are sent
 * @param msgs the list of messages to be sent
 */
void send_msgs(struct client *curr_client, linked_list_t *msgs);


/*
 * @brief Function to create a new message from an UDP client
 * 
 * @param ip the IP address of the UDP client sending the message
 * @param port the port of the UDP client sending the message
 * @param topic_id the topic's unique id
 * @param buf the message itself
 * 
 * @return pointer to the message structure
 */
struct message_t *parse_msg(int ip, int port, uint8_t data_type,
                            int topic_id, char *buf);


/*
 * @brief Function to handle an already parsed messaged coming from an UDP client
 * 
 * @param new_msg the parsed message
 */
void handle_msg(struct message_t new_msg);


/*
 * @brief Function to handle the stored messages for an user
 * 
 * @param curr_client the client to which the messages will be sent
 */
void handle_sf_queue(struct client *curr_client);


/*
 * @brief Function to get send a client its subscribed topics.
 * This function is used so the client doesn't need to store
 * its subscribed topics in its memory. This lets the client
 * connect from any device without any other storage file needed.
 * 
 * @param curr_client the client to which the topics will be sent
 */
void send_subbed_topics(struct client *curr_client);


/*
 * @brief Function to add a new topic to the server
 * 
 * @param topic_name the name of the new topic
 * 
 * @return the id of the new topic, if the topic is already
 * on the server, then it returns its id
 */
int add_new_topic(char *topic_name);


/*
 * @brief Function to get a topic's id by its name
 * 
 * @param topic_name the name of the requested topic
 * 
 * @return the id of the topic or -1 if there is no
 * topic registered on the server with the given name
 */
uint32_t get_topic_id(char *topic_name);


/*
 * @brief Function to get the index in the subscribed
 * topics list of a given topic
 * 
 * @param client_topics the list of subscribed topics for a client
 * @param topic_id the requested topic id
 * 
 * @return the index of the requested topic in the subscribed
 * topics list or -1 if there is no subscribed topic with that id in the list
 */
int get_topic_idx(linked_list_t *client_topics, uint32_t topic_id);


/*
 * @brief Function to get the Store-and-Forward flag for a subscribed topic
 * 
 * @param client_topics the list of subscribed topics for a client
 * @param topic_id the requested topic id
 * 
 * @return the Store-and-Forward flag
 */
uint8_t get_topic_sf(linked_list_t *client_topics, uint32_t topic_id);


/*
 * @brief Function to get the exact the size of a topics list,
 * including its data size
 * 
 * @param topics the list of topics
 * 
 * @return the size of the given topics list
 */
int get_topics_size(linked_list_t *topics);


/*
 * @brief Function to subscribe a client to a topic
 * 
 * @param curr_client the client that requested the subscribe
 * @param sf Store-and-Forward flag
 * @param topic_name the name of the topic to be subscribed to
 */
void sub_client(struct client *curr_client, uint8_t sf, char *topic_name);


/*
 * @brief Function to unsubscribe a client from a topic
 * 
 * @param curr_client the client that requested the unsubscribe
 * @param topic_id the topic's id to be unsubscribed from
 */
void unsub_client(struct client *curr_client, uint32_t topic_id);


/*
 * @brief Function to display registered topics and their id
 */
void show_topics();


/*
 * @brief Function to display registered users and their topics
 */
void show_clients();


/*
 * @brief Function to hash a string (used primarly for topics)
 * Found on https://web.archive.org/web/20190108202303/http://www.hackersdelight.org/hdcodetxt/crc.c.txt
 * 
 * @param message string to be hashed
 * 
 * @return the hash of the string as `uint32_t`
 */
uint32_t crc32_hash(unsigned char *message);
