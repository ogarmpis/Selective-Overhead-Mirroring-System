#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "functions.h"
#include "structs.h"

#define BUF 256


/*Global variables for synchronization of threads*/
int  total_workers = 0, numDevicesDone = 0, total_servers = 0, found = 0;
int  bytesTransferred = 0, filesTransferred = 0;
int  *worker_busy;
char dirname[BUF];
pthread_t *workers;
pthread_mutex_t transferInfo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t allDone      = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_allDone = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx;
pthread_cond_t  cond_nonempty;
pthread_cond_t  cond_nonfull;
buffer_t MainBuffer;


/*MirrorServer*/
int main(int argc, char const *argv[])
{
	int    i, err, threadnum, port, sock, newsock;
	struct sockaddr_in server, client;
	struct sockaddr *serverptr=(struct sockaddr *)&server;
	struct sockaddr *clientptr=(struct sockaddr *)&client;
	struct hostent  *rem;
	pthread_t main_server;
	socklen_t clientlen;

	/*Read and seperate the program's arguments*/
	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-p"))
			port = atoi(argv[++i]);				//port
		else if (!strcmp(argv[i], "-m"))
			strcpy(dirname, argv[++i]);			//dirname
		else if (!strcmp(argv[i], "-w"))
			threadnum = atoi(argv[++i]);		//threadnum
	}
	mkdir(dirname, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);		//Create the dirname to store the data from ContentServers

	/*Initialize the MainBuffer, mutexes and condition variables*/
	initialize_buffer(&MainBuffer);
	pthread_mutex_init(&mtx, 0);
	pthread_cond_init(&cond_nonempty, 0);
	pthread_cond_init(&cond_nonfull, 0);

	signal(SIGINT, mirror_sigint_handler);		//Create a signal handler for the proper termination of program (by SIGINT)

	/*Initialize the worker_busy array (if each separate worker is busy or not)*/
	total_workers = threadnum;
	worker_busy = malloc(threadnum * sizeof(int));
	for (i = 0; i < threadnum; i++)
		worker_busy[i] = 0;

	/*Create the workers threads*/
	workers = malloc(threadnum * sizeof(pthread_t));
	for (i = 0; i < threadnum; i++){
		if ((err = pthread_create(workers+i, NULL, Worker_thread, (void *)(intptr_t) (i+1))) != 0)
			print_error("Failed on pthread_create", 3, err);
	}

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

		/*If there is a customer, create a new thread for this one (such as fork)*/
		if ((err = pthread_create(&main_server, NULL, Server_thread, (void *)(intptr_t) newsock)) != 0)
			print_error("Failed on pthread_create", 3, err);
	}

	printf("Hello World Mirror-Server1!\n");
	return 0;
}




/*Server thread (one for each connection from MirrorInitiator)*/
void *Server_thread(void *arg)
{
	int   i, err, servers, newsock;
	char  ContentServerInfo[BUF], InitiatorInfo[BUF];
	float average;
	pthread_t *mirrors;
	ptrContentInfo *content_server_list, current;

	newsock = (intptr_t) arg;		//Socket of connection with MirrorInitiator

	/*Read and initiate (as a list) the information received from socket*/
	if (read(newsock, ContentServerInfo, BUF) < 0) print_error("Failed to read from buffer", 1, 0);
	content_server_list = malloc(sizeof(ptrContentInfo));
	*content_server_list = NULL;
	create_contentlist(content_server_list, ContentServerInfo);

	/*Create the MirrorManagers as threads and pass into each one the ContentServer information*/
	found = 0;
	current = *content_server_list;
	servers = (*content_server_list)->server_number;
	total_servers = servers;
	mirrors = malloc(servers * sizeof(pthread_t));
	for (i = 0; i < servers; i++){
		if ((err = pthread_create(mirrors+i, NULL, Mirror_thread, (void *) current)) != 0)
			print_error("Failed on pthread_create", 3, err);
		current = current->next;
	}

	/*Wait until all MirrorManagers are finished*/
	for (i = 0; i < servers; i++){
		if ((err = pthread_join(*(mirrors+i), NULL)) != 0)
			print_error("Failed on pthread_join", 3, err);
	}
	printf("All %d mirror threads have terminated\n", servers);
	free(mirrors);

	/*Wait until the last worker has finished its job and then write into MirrorInitiator socket the overall statistics*/
	pthread_mutex_lock(&allDone);
	pthread_cond_wait(&cond_allDone, &allDone);
	average = bytesTransferred/filesTransferred;
	sprintf(InitiatorInfo, "Files or Directories found!\n-Bytes Transferred: %d\n-Files Transferred: %d\n-Average: %.0f\n", 
				bytesTransferred, filesTransferred, average);
	if (found == 0){
		if (write(newsock, "None such file or directory found", BUF) < 0)
			print_error("Failed to write on socket buffer", 1, 0);
	}
	else{												//If none file or directory has been found, a fail message returns
		if (write(newsock, InitiatorInfo, BUF) < 0)
			print_error("Failed to write on socket buffer", 1, 0);
	}
	found = 0;				//Initiate the global variables for statistics for a possible next connection with a MirrorInitiator
	bytesTransferred = 0;
	filesTransferred = 0;
	pthread_mutex_unlock(&allDone);

	/*Close connection with MirrorInitiator, delete the content info list and terminate the server thread*/
	printf("Closing connection.\n------------------------------------------------------------------\n");
	close(newsock);
	delete_contentlist(content_server_list);
	pthread_exit(NULL);
}




/*Mirror threads (managers)*/
void *Mirror_thread(void *arg)
{
	int    sock, file_size, remain_data, len, len2;
	char   ReceivedInfo[BUF], command[BUF], tempfile[BUF], path[BUF];
	struct sockaddr_in server;
	struct sockaddr *serverptr = (struct sockaddr*)&server;
	struct hostent  *rem;
	FILE   *fp;

	ptrContentInfo content_server = (ptrContentInfo) arg;	//ContentServer information from server thread

	/*Create a socket for communication as a client and initialize connection information*/
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) print_error("Failed to create socket", 1, 0);
	if ((rem = gethostbyname(content_server->address)) == NULL) print_error("gethostbyname", 2, 0);
	server.sin_family = AF_INET;
	memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	server.sin_port = htons(content_server->port);

	/*Connect in the given ContentServerAddress and ContentServerPort*/
	if (connect(sock, serverptr, sizeof(server)) < 0) print_error("Failed to connect on socket", 1, 0);
	printf("Thread:%d Connecting to %s port %d\n", content_server->server_number, content_server->address, content_server->port);

	/*Send a LIST request in the ContentServer that thiw MirrorManager communicates with*/
	sprintf(command, "LIST %s_%d", content_server->address, content_server->port);
	if (write(sock, command, BUF) < 0) print_error("Failed to write into socket buffer", 1, 0);

	/*Read the file size (in which there is a list of all catalogs that ContentServer has to provide)*/
	if (read(sock, ReceivedInfo, BUF) < 0) print_error("Failed to read from socket buffer", 1, 0);
	file_size = atoi(ReceivedInfo);

	/*Create a new temporary file to store the input file*/
	sprintf(tempfile, "%s%d_%d-%d.txt", content_server->address, content_server->port, content_server->server_number, sock);
	if ((fp = fopen(tempfile, "w")) == NULL) print_error("Failed to open new file", 1, 0);

	/*Receive the LIST data from ContentServer and copy them to the new file*/
	remain_data = file_size;
	while (((len = recv(sock, ReceivedInfo, BUF, 0)) > 0) && (remain_data > 0)){
		fwrite(ReceivedInfo, sizeof(char), len, fp);
		remain_data = remain_data - len;
	}
	fclose(fp);

	/*Open the temporary file for reading, and if the given path is one of the desired ones, then send it to the MainBuffer*/
	if ((fp = fopen(tempfile, "r")) == NULL) print_error("Failed to open new file", 1, 0);
	len2 = strlen(content_server->dirorfile);
	fgets(path, BUF, fp);
	while (!feof(fp)){				//Read the temporary file till the end
		len  = strlen(path);
		path[len-1] = '\0';
		if (!strncmp(path, content_server->dirorfile, len2)){
			producer(content_server, path);
			found = 1;						//If the directory or file is one of the desired ones, then put it to buffer
		}
		fgets(path, BUF, fp);
	}
	fclose(fp);
	remove(tempfile);		//Delete the temporary file

	/*One more device has been served, if this is the last one then send a message to the buffer*/
	pthread_mutex_lock(&allDone);
	numDevicesDone++;
	if (numDevicesDone == total_servers){
		producer(content_server, "?0|All_done");		//There cannot be such a file(!) so send it as message
		numDevicesDone = 0;
		total_servers  = 0;
	}
	pthread_mutex_unlock(&allDone);

	close(sock);				//Close the socket with ContentServer
	pthread_exit(NULL);			//This mirror thread terminates
}




/*Workers threads*/
void *Worker_thread(void *arg)
{
	int    i, worker_number, sock, file_size, remain_data, len, type;
	char   command[BUF], ReceivedInfo[BUF], newdir[BUF], line[BUF], tempfile[BUF], current[BUF];
	char   *token, s[2] = " ", s2[2] = "/";
	struct sockaddr_in server;
	struct sockaddr *serverptr = (struct sockaddr*)&server;
	struct hostent  *rem;
	FILE   *fp;

	worker_number = (intptr_t) arg;			//The current worker's number (id)

	/*The workers keep being alive and receive new jobs until the end of the program (by SIGINT)*/
	while (1) {
		FetchInfo data;
		consumer(&data);						//If the worker receive something from buffer, becomes busy
		worker_busy[worker_number-1] = 1;

		/*If this is the ending message from buffer, wait until all workers finish their jobs*/
		if (!strcmp(data.DirorFileName, "?0|All_done")){
			for (i = 0; i < total_workers; i++){
				if (i != (worker_number-1))
					while (worker_busy[i] == 1);
			}
			worker_busy[worker_number-1] = 0;
			pthread_cond_signal(&cond_allDone);		//Permit the MirrorServer to send back into MirrorInitiator the statistics*/
			continue;
		}

		/*Create the new directory to store the incoming data (the path and ContentServer that buffer points)*/
		sprintf(newdir, "%s/%s_%d", dirname, data.ContentServerAddress, data.ContentServerPort);
		mkdir(newdir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

		/*Create a socket for communication as a client and initialize connection information*/
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) print_error("Failed to create socket", 1, 0);
		if ((rem = gethostbyname(data.ContentServerAddress)) == NULL) print_error("gethostbyname", 2, 0);
		server.sin_family = AF_INET;
		memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
		server.sin_port = htons(data.ContentServerPort);

		/*Connect in the given ContentServerAddress and ContentServerPort given from buffer*/
		if (connect(sock, serverptr, sizeof(server)) < 0) print_error("Failed to connect on socket", 1, 0);
		printf("Worker:%d Connecting to %s port %d\n", worker_number, data.ContentServerAddress, data.ContentServerPort);

		/*Send a FETCH request at ContentServer*/
		sprintf(command, "FETCH %s %d", data.DirorFileName, data.ContentServerDelay);
		if (write(sock, command, BUF) < 0) print_error("Failed to write into socket buffer", 1, 0);

		/*Read if it is a file or directory, its size and name*/
		if (read(sock, ReceivedInfo, BUF) < 0) print_error("Failed to read from socket buffer", 1, 0);
		token = strtok(ReceivedInfo, s);
		i = 1;
		type = 0;
		while (token != NULL){			//Separate the arguments from input string
			if (i == 1){
				if (!strcmp(token, "File")) type = 1;
				else if (!strcmp(token, "Dir")) type = 2;
			}
			else if (i == 2) strcpy(line, token);
			else if (i == 3) file_size = atoi(token);
			token = strtok(NULL, s);
			i++;
		}
		if (!strncmp(line, "/", 1))						//Adjust its name for the MirrorServer directory
			sprintf(tempfile, "%s%s", newdir, line);
		else sprintf(tempfile, "%s/%s", newdir, line);

		if (type == 1) {						//This is a file
			strcpy(command, tempfile);
			strcpy(line, newdir);
			token = strtok(command, s2);		//Check the parent-directories of the given file-directory
			token = strtok(NULL, s2);			//Check if there is such a directory in the store directory of MirrorServer
			token = strtok(NULL, s2);			//If there is not, then create it and each of his parents one by one
			while (token != NULL){
				sprintf(current, "%s/%s", line, token);
				token = strtok(NULL, s2);
				if (token == NULL) break;
				mkdir(current, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);	//The above explanation is about this line!
				strcpy(line, current);
			}

			/*Create a new file to copy into it the data received from ContentServer*/
			if ((fp = fopen(tempfile, "w")) == NULL) print_error("Failed to open new file", 1, 0);
			remain_data = file_size;
			while (((len = recv(sock, ReceivedInfo, BUF, 0)) > 0) && (remain_data > 0)){
				fwrite(ReceivedInfo, sizeof(char), len, fp);
				remain_data = remain_data - len;				//Create exactly the same file as the one requested
			}
			fclose(fp);

			pthread_mutex_lock(&transferInfo);					//Adjust the global variables for statistics
			bytesTransferred += file_size;
			filesTransferred++;
			pthread_mutex_unlock(&transferInfo);
		}
		else if (type == 2){					//This is a directory
			strcpy(line, newdir);
			token = strtok(tempfile, s2);
			token = strtok(NULL, s2);			//Adjust its path for the current pc
			token = strtok(NULL, s2);
			while (token != NULL){
				sprintf(current, "%s/%s", line, token);
				mkdir(current, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);	//Create the directory (if needed)
				token = strtok(NULL, s2);
				strcpy(line, current);
			}
		}
		else printf("Something wrong happened, data transfer failed.\n");

		pthread_cond_broadcast(&cond_nonfull);		//Send a signal for the capacity (full) of MainBuffer
		worker_busy[worker_number-1] = 0;			//This worker is no more busy
	}
	printf("end consumer\n");
	pthread_exit(NULL);
}




/*Copies the data given into the last position of the MainBuffer queue*/
void producer(ptrContentInfo content_server, char path[])
{
	pthread_mutex_lock(&mtx);
	while (MainBuffer.count >= MainBuffer.bufsize) {
		printf(">> Found Buffer Full \n");
		pthread_cond_wait(&cond_nonfull, &mtx);		//If the MainBuffer is full, wait until a place is released
	}
	MainBuffer.end = (MainBuffer.end + 1) % MainBuffer.bufsize;
	strcpy(MainBuffer.data[MainBuffer.end].DirorFileName, path);
	strcpy(MainBuffer.data[MainBuffer.end].ContentServerAddress, content_server->address);
	MainBuffer.data[MainBuffer.end].ContentServerPort  = content_server->port;
	MainBuffer.data[MainBuffer.end].ContentServerDelay = content_server->delay;
	MainBuffer.count++;
	pthread_mutex_unlock(&mtx);
	pthread_cond_broadcast(&cond_nonempty);			//Send a signal for the capacity (empty) of MainBuffer
}




/*Receives the front data block from MainBuffer queue and copies them into data (struct) variable*/
void consumer(FetchInfo *data)
{
	pthread_mutex_lock(&mtx);
	while (MainBuffer.count <= 0) {
		printf(">> Found Buffer Empty \n");
		pthread_cond_wait(&cond_nonempty, &mtx);	//If the MainBuffer is empty, wait until something comes in
	}
	strcpy(data->DirorFileName, MainBuffer.data[MainBuffer.start].DirorFileName);
	strcpy(data->ContentServerAddress, MainBuffer.data[MainBuffer.start].ContentServerAddress);
	data->ContentServerPort  = MainBuffer.data[MainBuffer.start].ContentServerPort;
	data->ContentServerDelay = MainBuffer.data[MainBuffer.start].ContentServerDelay;
	MainBuffer.start = (MainBuffer.start + 1) % MainBuffer.bufsize;
	MainBuffer.count--;
	//fprintf(stdout, "Consumer: %s-%s-%d-%d\n", data->DirorFileName, data->ContentServerAddress, 
	//			data->ContentServerPort, data->ContentServerDelay);
	pthread_mutex_unlock(&mtx);
}




/*Signal Handler for SIGINT signal - for termination of MirrorServer*/
void mirror_sigint_handler (int sig)
{
	int i, err;

	/*Cancel (terminate) all workers and free the dynamically allocated data*/
	for (i = 0; i < total_workers; i++){
		if ((err = pthread_cancel(*(workers+i))) != 0)
			print_error("Failed on pthread_cancel", 3, err);
	}
	printf("\nAll %d worker threads have terminated\n", total_workers);
	free(workers);
	free(worker_busy);

	/*Destroy the mutexes and the condition variables which initialized by init function)*/
	pthread_cond_destroy(&cond_nonempty);
	pthread_cond_destroy(&cond_nonfull);
	pthread_mutex_destroy(&mtx);

	printf("Hello World Mirror-Server!\n");
	exit(1);										//Exit the program
}