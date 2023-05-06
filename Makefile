SERVER_SOURCE=server.c
CLIENT_SOURCE=client.c

MAIN_SOURCES=$(SERVER_SOURCE) $(CLIENT_SOURCE)
MAIN_OBJECTS=$(MAIN_SOURCES:.c=.o)

UTILS_SOURCES=common.c list.c
UTILS_OBJECTS=$(UTILS_SOURCES:.c=.o)

LOCAL_HOST=127.0.0.1
DEFAULT_PORT=12345

CFLAGS=-c -Wall -Werror -Wno-error=unused-variable
CC=gcc

build: clean server subscriber clean.o

.c.o: 
	$(CC) $(CFLAGS) $< -o $@

server: $(MAIN_SOURCES) $(UTILS_SOURCES) $(MAIN_OBJECTS) $(UTILS_OBJECTS)
	$(CC) $(SERVER_SOURCE:.c=.o) $(UTILS_OBJECTS) -o $@

subscriber: $(MAIN_SOURCES) $(UTILS_SOURCES) $(MAIN_OBJECTS) $(UTILS_OBJECTS)
	$(CC) $(CLIENT_SOURCE:.c=.o) $(UTILS_OBJECTS) -o $@

clean:
	rm -rf *.o $(SERVER_SOURCE:.c=) subscriber

clean.o:
	rm -rf *.o

run_server: build
	./$(SERVER_BIN) $(DEFAULT_PORT)

run_client: build
	@read -p "Client ID: " id; \
	./$(CLIENT_BIN) $$id $(LOCAL_HOST) $(DEFAULT_PORT)

