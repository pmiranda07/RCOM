#include "URL.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define CONNECTION_PORT 21


int init_connection(char* address, int p);

void login(int ctr_socket, url_info* url_info);

void passive_mode(int sockfd, char* ip, int* p);

void send_retrieve(int ctr_socket, url_info* url_info);

int download(int data_socket, url_info* url_info);

int end_connection(int ctr_socket, int data_socket);
