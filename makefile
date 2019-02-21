CC = gcc
CFLAGS = -Wall -g

all: MirrorInitiator MirrorServer ContentServer

MirrorInitiator: MirrorInitiator.o functions.o
	$(CC) $(CFLAGS) -o MirrorInitiator MirrorInitiator.o functions.o

MirrorServer: MirrorServer.o functions.o
	$(CC) $(CFLAGS) -o MirrorServer MirrorServer.o functions.o -pthread

ContentServer: ContentServer.o functions.o
	$(CC) $(CFLAGS) -o ContentServer ContentServer.o functions.o

MirrorInitiator.o: MirrorInitiator.c
	$(CC) $(CFLAGS) -c MirrorInitiator.c

MirrorServer.o: MirrorServer.c
	$(CC) $(CFLAGS) -c MirrorServer.c

ContentServer.o: ContentServer.c
	$(CC) $(CFLAGS) -c ContentServer.c

functions.o: functions.c
	$(CC) $(CFLAGS) -c functions.c

.PHONY clean:

clean:
	rm -f MirrorInitiator MirrorInitiator.o
	rm -f MirrorServer MirrorServer.o
	rm -f ContentServer ContentServer.o
	rm -f functions.o