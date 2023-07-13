CC=gcc
CFLAGS=-Wall -Werror -Wextra
XFLAGS=-lsqlite3
SERVER=./src/tinyCchat.c
S_SRC=server_test
C_SRC=client_test

server: $(S_SRC).c
	$(CC) $(CFLAGS) $(S_SRC).c $(SERVER) $(XFLAGS) -o server

client: $(C_SRC).c
	$(CC) $(CFLAGS) $(C_SRC).c $(SERVER) $(XFLAGS) -o client

all: server client

clean: 
	rm server client file.db
