#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "functions.h"
#include "structs.h"

#define BUF 256
#define BUFFERSIZE 4				//For any change of BUFFERSIZE we must also change the value in structs.h



/*Initialize the MainBuffer members*/
void initialize_buffer(buffer_t *buffer)
{
	buffer->start   = 0;
	buffer->end     = -1;
	buffer->count   = 0;
	buffer->bufsize = BUFFERSIZE;
}



/*Create a list from the arguments of MirrorInitiator in order to define the ContentServers and LIST information*/
void create_contentlist(ptrContentInfo *first, char buffer_input[])
{
	int  i, server_number = 1;
	char *token, s[3] = ":,";

	token = strtok(buffer_input, s);							//Seperate the tokens of the received string
	while (token != NULL){
		ptrContentInfo node = malloc(sizeof(ContentInfo));		//Create the list of arguments for ContentServers
		node->next = *first;
		*first     = node;
		for (i = 0; i < 4; i++){
			if (i == 0) strcpy(node->address, token);
			else if (i == 1) node->port  = atoi(token);
			else if (i == 2) strcpy(node->dirorfile, token);
			else if (i == 3) node->delay = atoi(token);
			token = strtok(NULL, s);
		}
		node->server_number = server_number;					//Number of MirrorManager and its ContentServer
		server_number++;
	}
}



/*Delete the content list that we created above*/
void delete_contentlist(ptrContentInfo *first)
{
	ptrContentInfo current, next;
	current = *first;
	while (current != NULL){
		next = current->next;
		free(current);				//Delete every node of the list
		current = next;
	}
}



/*Search the directories recursively in order to find the contents of the initial directory given*/
void search_directories(char *dir, char *line, FILE *fp)
{
	char   str[BUF], current[BUF];
	struct dirent *entry;
	struct stat statbuf;
	DIR    *dp;

	strcpy(str, "");
	if ((dp = opendir(dir)) == NULL) print_error("Cannot open content directory", 1, 0);			//Open the directory
	chdir(dir);
	while ((entry = readdir(dp)) != NULL){															//Read its contents
		if (stat(entry->d_name, &statbuf) == -1) print_error("Failed to get file status", 1, 0);	//Find each one status
		if ((statbuf.st_mode & S_IFMT) == S_IFDIR){													//This is a directory
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
			strcpy(current, line);
			sprintf(str, "/%s", entry->d_name);
			strcat(line, str);										//Write in the file fp this directory
			fprintf(fp, "%s\n", line);
			search_directories(entry->d_name, line, fp);			//Move into this subdirectory
			strcpy(line, current);
		}
		else{
			if (!strcmp(line, "")) fprintf(fp, "%s\n", entry->d_name);		//Write in the file fp this file
			else fprintf(fp, "%s/%s\n", line, entry->d_name);
		}
	}
	chdir("..");
	closedir(dp);			//Close the current directory
}



/*Print differrent types of fatal error in functions of programs*/
void print_error(char* const error, int type, int err)
{
	switch(type){					//Depending on error type print this error
		case 1:
			perror(error);
			break;
		case 2:
			herror("gethostbyname");
			break;
		case 3:
			fprintf(stderr, "%s %s\n", error, strerror(err));
			break;
		default:
			printf("Something wrong happened\n");
			break;
	}
	exit(1);			//Exit the program
}



/*Signal handler for forked children*/
void sigchld_handler (int sig) {
	while (waitpid(-1, NULL, WNOHANG) > 0);
}