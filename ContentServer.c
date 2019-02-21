#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "functions.h"

#define BUF 256


/*ContentServer*/
int main(int argc, char const *argv[])
{
	int    i, port, sock, newsock, fd, remain_data, sent_bytes, delay;
	char   dirorfilename[BUF], command[BUF], line[BUF], tempfile[BUF], file_size[BUF];
	char   *token, s[2] = " ";
	struct sockaddr_in server, client;
	struct sockaddr *serverptr=(struct sockaddr *)&server;
	struct sockaddr *clientptr=(struct sockaddr *)&client;
	struct hostent  *rem;
	struct stat statbuf;
	off_t  offset;
	pid_t  pid;
	FILE   *fp;
	socklen_t clientlen;

	/*Read and seperate the program's arguments*/
	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-p"))
			port = atoi(argv[++i]);					//port
		else if (!strcmp(argv[i], "-d"))
			strcpy(dirorfilename, argv[++i]);		//dirorfilename
	}

	signal(SIGCHLD, sigchld_handler);		//Initialize a signal handler for forked children to avoid zombies

	/*Create a socket for communication as a server and initialize connection information*/
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) print_error("Failed to create socket", 1, 0);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port);

	/*Connection to the internet*/
	if (bind(sock, serverptr, sizeof(server)) < 0) print_error("Failed to bind the socket to port", 1, 0);
	if (listen(sock, 5) < 0) print_error("Failed to initiate listening", 1, 0);
	printf("Listening for connections to port %d\n", port);

	/*The server keeps working (waiting for customers) until we terminate it violently (SIGINT)*/
	while (1){
		clientlen = sizeof(client);
		if ((newsock = accept(sock, clientptr, &clientlen)) < 0) print_error("Failed to accept a connection", 1, 0);
		if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL)
			print_error("gethostbyaddr", 2, 0);
		printf("Accepted connection from %s\n", rem->h_name);

		/*If there is a customer, fork a child for this one and read the socket*/
		if ((pid = fork()) < 0) print_error("Failed on fork", 1, 0);
		if (pid == 0){
			close(sock);
			if (read(newsock, command, BUF) < 0) print_error("Failed to read from buffer", 1, 0);

			if (!strncmp(command, "LIST", 4)){			//If the command is LIST
				printf("%s\n", command);
				token = strtok(command, s);
				token = strtok(NULL, s);
				sprintf(tempfile, "%s_%d.txt", token, newsock);		//Create a temporary file to write the contents of dirorfilename
				strcpy(line, "");
				if ((fp = fopen(tempfile, "w")) == NULL) print_error("Failed to open new file", 1, 0);
				search_directories(dirorfilename, line, fp);
				fclose(fp);

				/*Find the temporary file status size, and send it back to the socket*/
				if ((fd = open(tempfile, O_RDONLY)) == -1) print_error("Failed to open file", 1, 0);
				if (stat(tempfile, &statbuf) == -1) print_error("Failed to get file status", 1, 0);
				sprintf(file_size, "%ld", statbuf.st_size);
				if (write(newsock, file_size, sizeof(file_size)) < 0) print_error("Failed to write on socket buffer", 1, 0);

				/*After sending the file's size, send the file itself*/
				offset      = 0;
				sent_bytes  = 0;
				remain_data = statbuf.st_size;
				while (((sent_bytes = sendfile(newsock, fd, &offset, BUF)) > 0) && (remain_data > 0)){
					remain_data -= sent_bytes;
				}
				close(fd);
				remove(tempfile);	//Delete the temporary file
			}
			else if (!strncmp(command, "FETCH", 5)){	//If the command is FETCH
				printf("%s\n", command);
				token = strtok(command, s);
				i = 1;									//Seperate the input string (file or directory to transfer and delay)
				while (token != NULL){
					if (i == 2) strcpy(line, token);
					else if (i == 3) delay = atoi(token);
					token = strtok(NULL, s);
					i++;
				}
				if (!strncmp(line, "/", 1))								//Fix the path to this file or directory
					sprintf(tempfile, "%s%s", dirorfilename, line);
				else sprintf(tempfile, "%s/%s", dirorfilename, line);

				/*Find the asked file (or directory) status*/
				if ((fd = open(tempfile, O_RDONLY)) == -1) print_error("Failed to open file", 1, 0);
				if (stat(tempfile, &statbuf) == -1) print_error("Failed to get file status", 1, 0);

				/*If this is a file, send firstly its name and size, sleep for given delay and then send the file back into socket*/
				if ((statbuf.st_mode & S_IFMT) == S_IFREG){
					sprintf(file_size, "File %s %ld", line, statbuf.st_size);
					if (write(newsock, file_size, sizeof(file_size)) < 0) print_error("Failed to write on socket buffer", 1, 0);
					offset      = 0;
					sent_bytes  = 0;
					remain_data = statbuf.st_size;
					sleep(delay);
					while (((sent_bytes = sendfile(newsock, fd, &offset, BUF)) > 0) && (remain_data > 0)){
						remain_data -= sent_bytes;
					}
				}													//If this is directory, just send back its name
				else if ((statbuf.st_mode & S_IFMT) == S_IFDIR){
					sprintf(file_size, "Dir %s %ld", line, statbuf.st_size);
					if (write(newsock, file_size, sizeof(file_size)) < 0) print_error("Failed to write on socket buffer", 1, 0);
				}
				else printf("Another type of data, cannot transfer!\n");

				close(fd);		//Close the file/directory
			}
			else printf("Wrong command given\n");

			printf("Closing connection.\n------------------------------------------------------------------\n");
			close(newsock);
			exit(1);			//Close the accepted socket and exit the forked child
		}
		close(newsock);
	}

	printf("Hello World Content-Server!\n");
	return 0;
}