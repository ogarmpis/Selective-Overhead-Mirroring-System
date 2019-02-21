#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include "functions.h"

#define BUF 256


/*MirrorInitiator*/
int main(int argc, char const *argv[])
{
	int    i, j, MirrorServerPort, sock, length;
	char   MirrorServerAddress[BUF], ContentServerInfo[BUF], ReceivedInfo[BUF];
	struct sockaddr_in server;
	struct sockaddr *serverptr = (struct sockaddr*)&server;
	struct hostent  *rem;

	/*Read and seperate the program's arguments*/
	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-n"))
			strcpy(MirrorServerAddress, argv[++i]);			//MirrorServerAddress
		else if (!strcmp(argv[i], "-p"))
			MirrorServerPort = atoi(argv[++i]);				//MirrorServerPort
		else if (!strcmp(argv[i], "-s")){
			strcpy(ContentServerInfo, argv[++i]);			//Information for ContentServers
			for (j = (i+1); j < argc; j++)
				strcat(ContentServerInfo, argv[j]);
		}
	}

	/*Create a socket for communication as a client and initialize connection information*/
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) print_error("Failed to create socket", 1, 0);
	if ((rem = gethostbyname(MirrorServerAddress)) == NULL) print_error("gethostbyname", 2, 0);
	server.sin_family = AF_INET;
	memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	server.sin_port = htons(MirrorServerPort);

	/*Connect in the given MirrorServerAddress and MirrorServerPort*/
	if (connect(sock, serverptr, sizeof(server)) < 0) print_error("Failed to connect on socket", 1, 0);
	printf("Connecting to %s port %d\n", MirrorServerAddress, MirrorServerPort);

	/*Write the ContentServers information in the socket*/
	length = strlen(ContentServerInfo);
	if (write(sock, ContentServerInfo, length) < 0) print_error("Failed to write into socket buffer", 1, 0);

	/*Read the return statistics from the MirrorServer and print them out*/
	if (read(sock, ReceivedInfo, BUF) < 0) print_error("Failed to read from socket buffer", 1, 0);
	printf("Received Information: %s", ReceivedInfo);

	close(sock);		//Close socket

	printf("Hello World Mirror-Initiator!\n");
	return 0;
}