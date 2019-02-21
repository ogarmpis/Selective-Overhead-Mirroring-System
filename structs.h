#ifndef __structs__
#define __structs__

#define BUF 256
#define BUFFERSIZE 4		//The slots of MainBuffer


/*Struct ContentInfo for a list of information from MirrorInitiator about the ContentServers*/
typedef struct ContentInfo *ptrContentInfo;

typedef struct ContentInfo {
	char address[BUF];
	int  port;
	char dirorfile[BUF];
	int  delay;
	int  server_number;
	ptrContentInfo next;
}ContentInfo;



/*Struct FetchInfo for the information in the MainBuffer*/
typedef struct FetchInfo {
	char DirorFileName[BUF];
	char ContentServerAddress[BUF];
	int  ContentServerPort;
	int  ContentServerDelay;
}FetchInfo;



/*Struct buffer_t is the structure of the MainBuffer*/
typedef struct buffer_t {
	FetchInfo data[BUFFERSIZE];
	int start;
	int end;
	int count;
	int bufsize;
}buffer_t;



#endif