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

- `subscribe <TOPIC_NAME> <SF>`, where the topic name must be a registered topic on the server or a topic to which the client is not already subscribed, otherwise this command will return an error. The Store-and-Forward flag informs the server to hold the messages that arrive for that topic in a personal message queue for when the client reconnects. After subscribing to a topic, the client stores in a list the new topic

- `unsubscribe <TOPIC_NAME>`, where the topic name must be a topic to which the client is subscribed locally, without sending a request to the server, otherwise it will return an error

- `exit`, this command closes every handler and socket, then frees used memory and triggers the exit for the client

The **subscribe command** will firstly send a `SUBSCRIBE` request to the server, containing the topic's name length, followed by the topic's name itself. Then it will either receive an **error response** from the server, meaning that the requested topic is not registered yet on the server, or an **OK response, containing the id of the topic on the server**, so the client can store the topic with the given id.

The **unsubscribe command** will firstly check if the given topic is in the local list of topics, then it will send to the server an `UNSUBSCRIBE` request, containing just the id of the topic and wait for a response from the server, which can be an **error or OK**. The topic will be removed from the local list.

To set the **Store-and-Forward flag** to 0 or 1, the client must unsubscribe from the topic and resubscribe.

The server has only one command, `exit`, which closes all the handlers and **shuts down the TCP listening socket**, frees the used memory and exits.

---

## **Communication Protocl and Efficiency** ##