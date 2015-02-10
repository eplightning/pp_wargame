# Bartosz Sławianowski 117205
# skompilowane .o lecą do build/[server,client]
# zlinkowane aplikacje do dist

# kompilator
CC=gcc
#CC=clang

# flagi kompilatora ogólne dla wszystkich .c
# O2 optymalizacja, Wall wszystkie warningi, gnu99 to jest c99 z paroma define'ami żeby niektóre posixowe funkcje się zaincludowały
CFLAGS=-c -O2 -Wall -std=gnu99 -I./include/

# flagi kompilatora dla serwera/klienta
SERVERFLAGS=$(CFLAGS) -I./server/
CLIENTFLAGS=$(CFLAGS) -I./client/

# flagi dla linkera klienta/serwera
# pthread dla semaforów POSIXowych, m(libmath) dla floora, rt dla clock_gettime (POSIXowe)
SERVERLIBS=-lm -lrt
CLIENTLIBS=-lncurses
# zależy jak dystrubucja linkuje ncurses
# jeśli narzeka na jakąś funkcje ncurses to zakomentować powyższe i odkomentować to niżej
#CLIENTLIBS=-lncurses -ltinfo

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
	$(CC) -pthread build/server/*.o -o dist/server $(SERVERLIBS)

# jak budować wykonywalny z obiektów klienta
dist/client: build/client/drawing_server.o build/client/game_screen.o build/client/main.o build/client/message_stack.o $(CLIENT_INCLUDES)
	$(CC) -pthread build/client/*.o -o dist/client $(CLIENTLIBS)
