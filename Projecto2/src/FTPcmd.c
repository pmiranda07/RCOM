#include "FTPcmd.h"


int read_socket(int sockfd, char* str){
  	FILE* fp = fdopen(sockfd, "r");
  int allocated = 0;
  if(str == NULL){
    str = (char*) malloc(sizeof(char) * 512);
    allocated = 1;
  }
  do {
    memset(str, 0, 512);
    str = fgets(str, 512, fp);
    printf("%s", str);
}  while (!('1' <= str[0] && str[0] <= '5') || str[3] != ' ');
  char reply_series = str[0];
  if(allocated)
    free(str);
  return (reply_series > '4');
}

int write_socket(int sockfd, char* cmd, char* response, int read){
    int return_val = write(sockfd, cmd, strlen(cmd));
    if(read)
      return read_socket(sockfd, response);
    else return (return_val == 0);
}
//clientTCP
int init_connection(char* address, int p){
  int	sockfd;
	struct	sockaddr_in server_addr;

	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(address);
	server_addr.sin_port = htons(p);

	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	return 0;
    	}
    	if(connect(sockfd,
	           (struct sockaddr *)&server_addr,
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		return 0;
	}
  return sockfd;
}

void login(int ctr_socket, url_info* url_info){

  char username_cmd[512], password_cmd[512];

  read_socket(ctr_socket, NULL);

  sprintf(username_cmd, "USER %s\r\n", url_info->user);
  write_socket(ctr_socket, username_cmd, NULL, 1);
  sprintf(password_cmd, "PASS %s\r\n", url_info->password);
  if(write_socket(ctr_socket, password_cmd, NULL, 1) != 0){
      fprintf(stderr, "Bad login. Exiting...\n"); //TODO: Ask for valid login
      exit(1);
  }
}

void passive_mode(int sockfd, char* ip, int* p){
  char response[512];

  if(write_socket(sockfd, "PASV\r\n", response, 1) != 0){
    fprintf(stderr, "Error entering passive mode.\n");
    exit(1);
  }

  int values[6];
  char* data = strchr(response, '(');
  sscanf(data, "(%d, %d, %d, %d, %d, %d)", &values[0],&values[1],&values[2],&values[3],&values[4],&values[5]);
  sprintf(ip, "%d.%d.%d.%d", values[0],values[1],values[2],values[3]);
  *p = values[4]*256+values[5];
}

void send_retrieve(int ctr_socket, url_info* url_info){
  char cmd[512];

  write_socket(ctr_socket, "TYPE L 8\r\n", NULL, 1);
  sprintf(cmd, "RETR %s%s\r\n", url_info->file_path, url_info->filename);
  if(write_socket(ctr_socket, cmd, NULL, 1) != 0){
    fprintf(stderr, "Error retrieving file.\n");
    exit(1);
  }
}

int download(int data_socket, url_info* url_info){
  FILE* outfile;
  if(!(outfile = fopen(url_info->filename, "w"))) {
		printf("Cannot open file.\n");
		return 1;
	}

  char buf[1024];
  int bytes;
  while ((bytes = read(data_socket, buf, sizeof(buf)))) {
    if (bytes < 0) {
      fprintf(stderr, "Nothing received from data socket.\n");
      return 1;
    }

    if ((bytes = fwrite(buf, bytes, 1, outfile)) < 0) {
      fprintf(stderr, "Cannot write data in file.\n");
      return 1;
    }
  }

  fclose(outfile);

  printf("File Downloaded\n");

  return 0;
}

int end_connection(int ctr_socket, int data_socket){

  printf("Ending connection\n");
  if(write_socket(ctr_socket, "QUIT\r\n", NULL, 0) != 0){
    fprintf(stderr, "Error closing connection.\n");
    close(data_socket);
    close(ctr_socket);
    exit(1);
  }

  close(data_socket);
  close(ctr_socket);


  return 0;
}
