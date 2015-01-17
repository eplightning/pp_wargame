CC=gcc
CFLAGS=-c -Wall -std=gnu99 -I./include/
SERVERLIBS=-lpthread -lm
SERVERFLAGS=$(CFLAGS) -lm -lpthread -I./server/
CLIENTLIBS=-lncurses
CLIENTFLAGS=$(CFLAGS) -lncurses -I./client/
SERVER_INCLUDES=include/protocol.h server/config.h server/evqueue.h server/game_stuff.h server/list.h server/logic_processes.h server/main_queue.h server/mq_state.h server/player_processes.h server/utils.h
CLIENT_INCLUDES=include/protocol.h client/drawing_server.h client/game_screen.h client/message_stack.h

all: server client

clean:
	rm dist/*
	rm build/server/*.o
	rm build/client/*.o

server: dist/server

client: dist/client

build/server/%.o: server/%.c $(SERVER_INCLUDES)
	$(CC) $(SERVERFLAGS) $< -o $@

build/client/%.o: client/%.c $(CLIENT_INCLUDES)
	$(CC) $(CLIENTFLAGS) $< -o $@

dist/server: build/server/evqueue.o build/server/game_stuff.o build/server/list.o build/server/logic_processes.o build/server/main.o build/server/main_queue.o build/server/mq_state.o build/server/player_processes.o build/server/utils.o $(SERVER_INCLUDES)
	$(CC) $(SERVERLIBS) build/server/*.o -o dist/server

dist/client: build/client/drawing_server.o build/client/game_screen.o build/client/main.o build/client/message_stack.o $(CLIENT_INCLUDES)
	$(CC) $(CLIENTLIBS) build/client/*.o -o dist/client
