#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#include "URL.h"
#include "FTPcmd.h"


int main(int argc, char** argv){

  if(argc != 2){
    fprintf(stderr, "Usage: %s <address>\n", argv[0]);
    exit(1);
  }

  url_info url_info;

  if(parse_url(argv[1], &url_info) != 0){
    fprintf(stderr, "Invalid URL\n");
    exit(1);
  }

  int ctr_socket;
  //getip
  if((ctr_socket = init_connection(inet_ntoa(*((struct in_addr *)url_info.host_info->h_addr)), CONNECTION_PORT)) == 0){
    fprintf(stderr, "Error in control connection\n");
    exit(1);
  }

  login(ctr_socket, &url_info);
  char data_address[512];
  int p;
  passive_mode(ctr_socket, data_address, &p);

  int data_socket;
  if((data_socket = init_connection(data_address, p)) == 0){
    fprintf(stderr, "Error in data connection\n");
    exit(1);
  }
  send_retrieve(ctr_socket, &url_info);
  download(data_socket, &url_info);
  end_connection(ctr_socket, data_socket);

  return 0;
}
