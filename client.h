#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#define USAGE "Usage: %s <ID> <SERVER_IP> <PORT>\n"

#define ERR_COMM "Invalid command. Please try again.\n"
#define ERR_ASUB "Already subscribed to this topic.\n"
#define ERR_NSUB "Requested topic isn't subscribed to. Please try again.\n"
#define ERR_NOTP "Requested topic doesn't exist. Please try again.\n"
#define ERR_CONN "Server closed connection.\n"


#define INT_MSG "%s:%d - %s - INT - %d\n"           /* format for INT type */
#define SR_MSG "%s:%d - %s - SHORT_REAL - %.2f\n"   /* format for SHORT_REAL type */
#define FLT_MSG "%s:%d - %s - FLOAT - %lf\n"        /* format for FLOAT type */
#define STR_MSG "%s:%d - %s - STRING - %s\n"        /* format for STRING type */


/*
 * @brief Function to add a new subscribed topic to the global
 * array of subscribed topics
 * 
 * @param id the id of the new subscribed topic
 * @param topic_name the name of the topic
 */
void add_subbed_topic(uint8_t id, char *topic_name);


/*
 * @brief Function to get the id of a subscribed topic
 * 
 * @param topic_name the name of the requested topic
 * 
 * @return the id of the requested topic if the client is
 * subscribed to it, -1 otherwise
 */
int get_topic_id(char *topic_name);


/*
 * @brief Function to get the name of a subscribed topic by it id
 * 
 * @param topic_id the id of the requested topic
 * 
 * @return the name of the topic if the client is
 * subscribed to it, NULL otherwise
 */
char *get_topic_name(uint8_t topic_id);


/*
 * @brief Function to get a stored topic's index in
 * the subscribed topics list
 * 
 * @param topic_id the id of the requested topic
 * 
 * @return the index of the requested topic if the client
 * is subscribed to it, -1 otherwise
 */
int get_topic_idx(uint8_t topic_id);


/*
 * @brief Function to make a request to the server to check if
 * the given client id is already connected to the server.
 * 
 * @attention The server will send a response that may be an ERR,
 * if there is a client connected with the given id or
 * an OK if there is no client connected with the given id,
 * then it establishes the connection to the server.
 * 
 * If the client id is registered on the server, the `optional`
 * field of the response is 1, so the client waits for the subscribed topics, 
 * if there are any stored for the given id on the server.
 * 
 * @attention This function can trigger the exit of the client if
 * the given id is already connected to the server.
 * 
 * @param id the id of the client
 * @param tcp_sockfd the TCP socket connected to the server
 * 
 * @return 0 if successful, -1 otherwise
 */
int verify_id(char *id, int tcp_sockfd);


/*
 * @brief Function to make a request to the server to subscribe
 * to a topic. Firstly, the client checks if the requested topic
 * is already subscribed to, without making a request. Then the
 * client waits for the response from the server.
 * 
 * @attention The response can be an OK, which will also contain the id of the
 * subscribed topic in the `option` field of the response (command_hdr).
 * 
 * @param topic_name the name of the topic to subscribe to
 * @param sf the Store-and-Forward flag (0 or 1)
 * @param tcp_sockfd the TCP socket connected to the server
 * 
 * @return the id of the subscribed topic, -1 if the requested topic
 * is not registered on the server, -2 if the requested topic is already
 * in the subscribed topics list for the client
 */
uint8_t subscribe_topic(char *topic_name, int sf, int tcp_sockfd);


/*
 * @brief Function to make a request to the server to unsubscribe
 * from a topic. Firstly, the client checks if the requested topic
 * is subscribed to, without making a request. Then the client
 * waits for the response from the server.
 * 
 * @attention The response can be an OK, which will also contain the return value
 * 0 (success) in the `option` field of the of the response (command_hdr).
 * Note: this field may contain other information in the future for this response.
 * 
 * @param topic_name the name of the topic to unsubscribe from
 * @param tcp_sockfd the TCP socket connected to the server
 * 
 * @return the id of the subscribed topic, -1 if the requested topic
 * is not registered on the server, -2 if the requested topic is already
 * in the subscribed topics list for the client
 */
uint8_t unsubscribe_topic(uint8_t topic_id, int tcp_sockfd);


/*
 * @brief Function to format and print a received message
 * coming from the server for a subscribed topic.
 * 
 * @attention This function is used to receive messages for the client
 * after exit.
 * 
 * @param buf the received buffer that may contain multipe messages
 * @param msgs_no the number of messages to be read from the buffer
 * @param tot_len total length of the messages buffer
 */
void handle_incoming_msgs(char *buf, int msgs_no, int tot_len);
