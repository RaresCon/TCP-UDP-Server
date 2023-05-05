SERVER=server.c
CLIENT=client.c
SERVER_BIN=server
CLIENT_BIN=subscriber

LOCAL_HOST=127.0.0.1
DEFAULT_PORT=12345

CFLAGS=-c
CC=gcc

build: clean server subscriber

list.o: list.c
	$(CC) $(CFLAGS) list.c

server.o:
	$(CC) $(CFLAGS) $(SERVER)

client.o:
	$(CC) $(CFLAGS) $(CLIENT)

server: server.o list.o
	$(CC) server.o list.o -o $@

subscriber: client.o list.o
	$(CC) client.o list.o -o $@

clean:
	rm -rf *.o server subscriber

run_server: build
	./$(SERVER_BIN) $(DEFAULT_PORT)

run_client: build
	@read -p "Client ID: " id; \
	./$(CLIENT_BIN) $$id $(LOCAL_HOST) $(DEFAULT_PORT)

