#ifndef __functions__
#define __functions__

#include "structs.h"


/*Functions from functions.c*/
void initialize_buffer(buffer_t *buffer);
void create_contentlist(ptrContentInfo *first, char buffer_input[]);
void delete_contentlist(ptrContentInfo *first);
void search_directories(char *dir, char *line, FILE *fp);
void print_error(char* const error, int type, int err);
void sigchld_handler (int sig);


/*Functions from MirrorServer.c*/
void *Server_thread(void *arg);
void *Mirror_thread(void *arg);
void *Worker_thread(void *arg);
void producer(ptrContentInfo content_server, char path[]);
void consumer(FetchInfo *data);
void mirror_sigint_handler (int sig);


#endif