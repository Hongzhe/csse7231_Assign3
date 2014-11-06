CC=gcc
CFLAGS = -Wall -pedantic -std=gnu99
DEBUGS= -g

all: player hub

player: player.c
	$(CC) $(CFLAGS) $(DEBUGS) player.c -o player 

hub: hub.c
	 $(CC) $(CFLAGS) $(DEBUGS) hub.c -o hub


clean:
		rm -rf player hub
