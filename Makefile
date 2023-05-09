SERVER_SOURCE=server.c
CLIENT_SOURCE=client.c

SERVER_BIN=server
CLIENT_BIN=subscriber

UTILS_SOURCES=common.c list.c
UTILS_OBJECTS=$(UTILS_SOURCES:.c=.o)

LOCAL_HOST=127.0.0.1
DEFAULT_PORT=12345

CFLAGS=-c -Wall -Werror -Wno-error=unused-variable
CC=gcc

build: clean server subscriber clean.o

.c.o: 
	$(CC) $(CFLAGS) $< -o $@

server: $(SERVER_SOURCE) $(UTILS_SOURCES) $(SERVER_SOURCE:.c=.o) $(UTILS_OBJECTS)
	$(CC) $(SERVER_SOURCE:.c=.o) $(UTILS_OBJECTS) -o $@

subscriber: $(CLIENT_SOURCE) $(UTILS_SOURCES) $(CLIENT_SOURCE:.c=.o) $(UTILS_OBJECTS)
	$(CC) $(CLIENT_SOURCE:.c=.o) $(UTILS_OBJECTS) -o $@

clean:
	rm -rf *.o $(SERVER_BIN) $(CLIENT_BIN)

clean.o:
	rm -rf *.o

run_server: server clean.o
	./$(SERVER_BIN) $(DEFAULT_PORT)

run_client: subscriber clean.o
	@read -p "Client ID: " id; \
	./$(CLIENT_BIN) $$id $(LOCAL_HOST) $(DEFAULT_PORT)

