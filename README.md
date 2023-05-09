## **TCP-UDP Server - TCP Client** ##
#### **Made by Rares Constantin - 2023** ####
---
## **Description** ##

This is a *TCP-UDP Server and TCP Client* implementation that use **TCP sockets** for communication. The main functionality of the programs is subscribing and receiving messages regarding different topics coming from *UDP clients*. The server can sustain a maximum number of connections equal to **50** as default and it can manage messages for a *subscriber* while it is disconnected if needed. 

---

## **How to use** ##

The server and the client both need the `common` and `list` files for them to work, as they share some functions and headers. Use `make` or `make build` to compile them.

To run the server, use: `./server <PORT_NUMBER>`, where the port number must be a valid unsigned integer.

To run the client, use: `./subscriber <ID> <IP_ADDR> <PORT_NUMBER>`, where the id must be a string of maximum 10 characters, the IP address must be the address of the server (**127.0.0.1** by default) and the port number must be the port on which the server is listening.

To make it easier to use them, the `Makefile` has to commands that will run the server and the client with default settings:

- `make run_server` will start the server with `12345` as default port number
- `make run_client` will ask for a client ID and then start the client with the default settings

The default settings can be modified in the `Makefile`.

---

## **Flow of communcation** ##

After the server is started, when a new client is trying to connect, it sends to the server a `CHECK_ID` request, followed by the id of the client and the server accepts the connection, then firstly checks if another client is already connected to the server with that id. If the latter is true, then the server returns an error response and closes the socket, the client exits after printing an error.

If the client has a new id, then the server registers this client and returns an **OK response**. If the client doesn't have a new id, meaning that **the server already stores a client with that id**, it will send an **OK response**, but now the response contains a **flag for the client** to inform it to wait for its subscribed topics from the server, if there are any. This is done to **not rely on local files** to store the client's subscribed topics, so the user can run the client using any device without worrying about losing subscribed topics.

After the connection is established, the client can use **three commands**, as follows:

- `subscribe <TOPIC_NAME> <SF>`, where the topic name must be a registered topic on the server (if no message was ever received by the server, then the topic doesn't exists yet) or a topic to which the client is not already subscribed, otherwise this command will return an error. The Store-and-Forward flag informs the server to hold the messages that arrive for that topic in a personal message queue for when the client reconnects. After subscribing to a topic, the client stores in a list the new topic

- `unsubscribe <TOPIC_NAME>`, where the topic name must be a topic to which the client is subscribed locally, without sending a request to the server, otherwise it will return an error

- `show`, which admits three arguments:
    - `server_topics`, which prints all available topics to subscribe registered on the server, indicating with `+` the ones already subscribed to
    - `my_topics`, which prints all subscribed topics
    - `history`, which prints local history of messages, it stores a maximum of **10** old messages by default, this value can be modified in the `common.h` file

- `save`, which admits for now only one argument:
    - `history`, which saves locally by appending in `history_save.txt` file the current message history

- `exit`, this command closes every handler and socket, then frees used memory and triggers the exit for the client

The **subscribe command** will firstly send a `SUBSCRIBE` request to the server, containing the topic's name length, followed by the topic's name itself. Then it will either receive an **error response** from the server, meaning that the requested topic is not registered yet on the server, or an **OK response, containing the id of the topic on the server**, so the client can store the topic with the given id. The id of the topic is hashed using *CRC32* so in the future, the server may delete topics without worrying the id is relying on the size of the registered topics list.

The **unsubscribe command** will firstly check if the given topic is in the local list of topics, then it will send to the server an `UNSUBSCRIBE` request, containing just the id of the topic and wait for a response from the server, which can be an **error or OK**. The topic will be removed from the local list.

To set the **Store-and-Forward flag** to 0 or 1, the client must unsubscribe from the topic and resubscribe.

The server has three commands currently, used by an admin, as follows:
- `show`, which admits two arguments:
    - `topics`, which prints all topics registered currently on the server, including their ids (hashes)
    - `clients`, which prints all clients registered currently on the server, including their subscribed topics and their Store-and-Forward flag
- `exit`, which closes all the handlers and **shuts down the TCP listening socket**, frees the used memory and exits

The server receives messages from **UDP clients** and handles them for the clients, sending a message instantly if a client is connected to the server, or stores it to be sent at reconnection if the client is subscribed to its topic with **Store-and-Forward active**. The message is sent to the client in a compact and efficient way, presented below.

To be as reliable as possible, both the server and the client catch all possible system errors, **treating them in different ways**. For example, if a socket is not created on first try, then **the server/client retries to create that socket ten more times (by default, see `common.h`)**, this is done for all major parts of the applications, so we know that **the server/client has tried its best to open the communication**. Another example, when the server/client sends or receives something, **it checks requested size is actually sent/received** and if not, **it doesn't complete a request or a response**, so the server/client **do not run into major problems**, like keeping a socket open even if on the other side of it something has gone wrong.

---

## **Communication Protocol and Efficiency** ##

There are two protocols used during the communication of the server and client, one being used to control both of them while running and one being the way the server sends the messages to the client after receiving them from an UDP client. These protocols help with the efficiency of the communication itself, making data more compact and only sending the needed information.

It's important to note before explaining the protocols that **the topics are stored as ids on the server and in the client**, so the server and the client **don't need the name of the topic** which can be 50 bytes long to understand the messages, but only 4 bytes. As an improvement, the name of the topic is hashed, so the id doesn't rely on any extern factor, like the size of the local list of topics, and the possibility of collision is minimal. This allows for new admin commands, like *delete topic*.

**First protocol** is based on the `command_hdr` structure which allows the client to send requests to the server and the server to send responses back to the client. It has an `opcode` field that represents the **operation of the request or the response type**, an `option` field that can hold an **optional flag for the operation or response** and a `buf_len` field that represents the **length of an optional buffer coming after this header** (inspired by HTTP protocol).

**Second protocol** is based on a simplier way of storing and sending a message to a client by using the `message_hdr` structure, that lets **the server and client represent an UDP client and its message without using a literal string**, but smaller data types. The `ip_addr` and `port` fields are used to **print the IP adress and port of the UDP client** sending the message, the `topic_id` and `data_type` fields **help the client understand what is the topic and data type contained in the message**, and at last, the `buf_len` field representing the size of the message coming after this header in a larger buffer. It becomes easy to separate information this way and **even if the TCP protocol concatenates data**, it will still function as inteneded without any errors.

When a longer buffer containing more than one message is sent, the server puts all messages one after the other in a larger buffer, then sends a smaller header containing the number of messages and the total length of this larger buffer to the client, so the client receives on the socket exactly this length. After having this buffer, the client deconstructs the messages and prints them for the user.

These structures can be found inside the `common.h` file.

---

## **Things to improve** ##

I could implement other commands for the client and the server to have more control over the data and information and I would have also liked to use a config file for some of the static values like the maximum number of connections. These would come in a later update, so stay tuned.

---
## **Copy-Right 2023** ##
---
