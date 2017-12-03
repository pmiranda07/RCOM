#include "dataLink_layer.h"
#include "app_layer.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1


int main(int argc, char **argv){

  if ((argc < 2) || ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
                     (strcmp("/dev/ttyS1", argv[1]) != 0))) {
    printf("Usage:\tnserial <SerialPort>\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  printf("Mode: Receiver(r)/Transmitter(t): ");
  int flagMode=0;
  char mode[1];

  do{


    scanf("%s",mode);

    if(strcmp(mode,"r")==0 || strcmp(mode,"t")==0){
      flagMode=1;
    }
    else{
      printf("\nThis mode doesn't exists,try again: ");
    }

  }while(!flagMode);

  init_data_link();

  if(strcmp(mode,"r")==0){ //RECEIVER

    printf("\nReceiver.\n\n");

    int fd = set_up_connection(argv[1], RECEIVER);

    if (fd < 0) {
      printf("Error opening file descriptor. Exiting...\n");
      return -1;
    }

    receive_data();

    llclose(fd);

  } else if(strcmp(mode,"t")==0){ //TRANSMITTER

    printf("\nTransmitter.\n\n");

    int fd = set_up_connection(argv[1], TRANSMITTER);

    if(fd < 0) {
      printf("Error opening file descriptor. Exiting...\n");
      return -1;
    }

    char path[] = ".";
    char filename[] = "pinguim.gif";
    if(send_data(path, filename) == -1)
      force_close(fd);
    else
      llclose(fd);


  }

  return 0;

}
