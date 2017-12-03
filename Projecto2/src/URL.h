#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct {
  char user[256];
  char password[256];
  char host_url[256];
  struct hostent* host_info;
  char file_path[256];
  char filename[256];
} url_info ;


int parse_url(char url[], url_info* url_info);
