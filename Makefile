# Bartosz Sławianowski
# skompilowane .o lecą do build/[server,client]
# zlinkowane aplikacje do dist

# kompilator
CC=gcc
#CC=clang

# flagi kompilatora ogólne dla wszystkich .c
CFLAGS=-c -O2 -Wall -std=gnu99 -I./include/
#CFLAGS=-c -m32 -O2 -Wall -std=gnu99 -I./include/

# flagi kompilatora dla serwera/klienta
SERVERFLAGS=$(CFLAGS) -I./server/
CLIENTFLAGS=$(CFLAGS) -I./client/

# flagi dla linkera klienta/serwera
SERVERLIBS=-pthread -lm -lrt
CLIENTLIBS=-pthread -lncurses
#SERVERLIBS=-pthread -lm -m32
#CLIENTLIBS=-lncurses -m32
# zależy jak dystrubucja linkuje ncurses
#CLIENTLIBS=-pthread -lncurses -ltinfo

# wszystkie pliki .h
SERVER_INCLUDES=include/protocol.h server/config.h server/evqueue.h server/game_stuff.h server/list.h server/logic_processes.h server/main_queue.h server/mq_state.h server/player_processes.h server/utils.h
CLIENT_INCLUDES=include/protocol.h client/drawing_server.h client/game_screen.h client/message_stack.h

# domyślny target
all: server client

# czyszczenie
clean:
	rm -f dist/*
	rm -f build/server/*.o
	rm -f build/client/*.o

# aliasy
server: dist/server

client: dist/client

# jak budować .o serwera ze źródeł
build/server/%.o: server/%.c $(SERVER_INCLUDES)
	$(CC) $(SERVERFLAGS) $< -o $@

# jak budować .o klienta ze źródeł
build/client/%.o: client/%.c $(CLIENT_INCLUDES)
	$(CC) $(CLIENTFLAGS) $< -o $@

# jak budować wykonywalny z obiektów serwera
dist/server: build/server/evqueue.o build/server/game_stuff.o build/server/list.o build/server/logic_processes.o build/server/main.o build/server/main_queue.o build/server/mq_state.o build/server/player_processes.o build/server/utils.o $(SERVER_INCLUDES)
	$(CC) $(SERVERLIBS) build/server/*.o -o dist/server

# jak budować wykonywalny z obiektów klienta
dist/client: build/client/drawing_server.o build/client/game_screen.o build/client/main.o build/client/message_stack.o $(CLIENT_INCLUDES)
	$(CC) $(CLIENTLIBS) build/client/*.o -o dist/client
