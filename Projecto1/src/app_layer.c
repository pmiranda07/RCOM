
#include "app_layer.h"
#include <errno.h>

#define FILE_SIZE_BYTE 0
#define FILE_NAME_BYTE 1
#define FILE_PERMISSIONS_BYTE 2

#define PACKET_SIZE 256
#define PACKET_HEADER_SIZE 4
#define PACKET_DATA_SIZE 252
#define FILE_SIZE 10968

int num_bytes_read=0;
app_layer app;

int set_up_connection(char *terminal, status stat) {
  if (stat != TRANSMITTER && stat != RECEIVER) {
    printf("Invalid status.\n");
    return -1;
  }

  app.app_layer_status = stat;

  int port;
  if (strcmp(terminal, COM1_PORT) == 0)
    port = COM1;
  else if (strcmp(terminal, COM2_PORT) == 0)
    port = COM2;
  else {
    printf("Invalid terminal\n");
  }

  if ((app.file_descriptor =
           llopen(port, app.app_layer_status)) < 0) {
    printf("llopen failed\n");
    return -1;
  }

  return app.file_descriptor;
}

off_t get_file_size(char *packet, int packet_len) {
  int i = 1;
  while (i < packet_len) {
    if (packet[i] == FILE_SIZE_BYTE)
      return *((off_t *)(packet + i + 2));

    i += 2 + packet[i + 1];
  }

  return 0;
}

char *get_file_name(char *packet, int packet_len) {
  int i = 1;
  while (i < packet_len) {
    if (packet[i] == FILE_NAME_BYTE) {
      char *file_name = (char *)malloc((packet[i + 1] + 1) * sizeof(char));
      memcpy(file_name, packet + i + 2, packet[i + 1]);
      file_name[(packet[i + 1] + 1)] = 0;
      return file_name;
    }

    i += 2 + packet[i + 1];
  }

  return NULL;
}

mode_t get_file_permissions(char *packet, int packet_len) {
  int i = 1;
  while (i < packet_len) {
    if (packet[i] == FILE_PERMISSIONS_BYTE)
      return *((mode_t *)(packet + i + 2));

    i += 2 + packet[i + 1];
  }

  return -1;
}

int send_data(char *path, char *filename) {
  char *full_path =
      (char *)malloc(sizeof(char) * (strlen(path) + 1 + strlen(filename)));
  strcpy(full_path, path);
  strcat(full_path, "/");
  strcat(full_path, filename);
  int fd = open(full_path, O_RDONLY);

  if (fd < 0) {
    printf("Error opening file. Exiting...\n");
    return -1;
  }

  struct stat file_info;
  fstat(fd, &file_info);

  int filename_len = strlen(filename);
  off_t file_size = file_info.st_size;
  mode_t file_mode = file_info.st_mode;

  /*
  * START PACKET
  */
  int start_packet_len = 7 + sizeof(mode_t) + sizeof(file_info.st_size) + filename_len;
  char *start_packet = (char *)malloc(sizeof(char) * start_packet_len);
  start_packet[0] = START_PACKET_BYTE;
  start_packet[1] = FILE_PERMISSIONS_BYTE;
  start_packet[2] = sizeof(mode_t);
  *((mode_t *)(start_packet + 3)) = file_mode;

  start_packet[3 + sizeof(mode_t)] = FILE_SIZE_BYTE;
  start_packet[4 + sizeof(mode_t)] = sizeof(file_info.st_size);
  *((off_t *)(start_packet + 5 + sizeof(mode_t))) = file_size;
  start_packet[5 + sizeof(mode_t) + sizeof(file_info.st_size)] = FILE_NAME_BYTE;
  start_packet[6 + sizeof(mode_t) + sizeof(file_info.st_size)] = filename_len;
  strcat(start_packet + 7 + sizeof(mode_t) + sizeof(file_info.st_size), filename);
  llwrite(app.file_descriptor, start_packet, start_packet_len);

  /*
  * DATA PACKET
  */
  char data[PACKET_DATA_SIZE];
  int i = 0;
  off_t bytes_remaining = file_size;

  while (bytes_remaining > 0) {
    int read_chars;
    if ((read_chars = read(fd, data, PACKET_DATA_SIZE)) <= 0) {
      printf("Error reading from file. Exiting...\n");
      return -1;
    }

    char information_packet[PACKET_SIZE];
    int packet_size = read_chars + PACKET_HEADER_SIZE;
    information_packet[0] = DATA_PACKET_BYTE;
    information_packet[1] = i % 256;
    information_packet[2] = read_chars / 256;
    information_packet[3] = read_chars % 256;

    memcpy(information_packet + PACKET_HEADER_SIZE, data, read_chars);
    if(llwrite(app.file_descriptor, information_packet, packet_size) == -1) {
      printf("Connection lost. Exiting program...\n");
      close(fd);
      return -1;
    }


    bytes_remaining -= read_chars;
    i++;
  }


  /*
  * END PACKET
  */
  char end_packet[] = {END_PACKET_BYTE};
  llwrite(app.file_descriptor, end_packet, 1);
  close(fd);

  printf("Total timeouts: %d.\n",getTotalTimeouts());

  return 0;
}

int receive_data() {
  char packet[PACKET_SIZE];
  struct timespec init, final;

  int packet_len;
  /**
  * Reading and parsing start packet
  */
  do {
    if (llread(app.file_descriptor, packet, &packet_len) != 0) {
      printf("Error llread() in function receive_data().\n");
      exit(-1);
    }
  } while (packet_len == 0 || packet[0] != (unsigned char)START_PACKET_BYTE);

  printf("Receiving data...\n");

  clock_gettime(CLOCK_REALTIME, &init);

  off_t file_size = get_file_size(packet, packet_len);
  char *file_name = get_file_name(packet, packet_len);
  mode_t file_mode = get_file_permissions(packet, packet_len);

  int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC);

  if (fd < 0) {
    printf("Error opening file. Exiting...\n");
    return -1;
  }

  /**
  * Reading and parsing data packets
  */
  char cur_seq_num = 0;
  if (llread(app.file_descriptor, packet, &packet_len) != 0) {
    printf("Error llread() in function receive_data().\n");
    close(fd);
    exit(-1);
  }

  while (packet_len == 0 || packet[0] != (unsigned char)END_PACKET_BYTE) {
    if (packet_len > 0 && cur_seq_num == packet[1]) {
      unsigned int data_len =
          (unsigned char)packet[2] * 256 + (unsigned char)packet[3];
      write(fd, packet + 4, data_len);

      num_bytes_read+=data_len;

      cur_seq_num++;
    }

    if (llread(app.file_descriptor, packet, &packet_len) != 0) {
      printf("Error llread() in function receive_data().\n");
      close(fd);
      exit(-1);
    }
  }

  struct stat file_info;
  fstat(fd, &file_info);
  printf("Expected %lu bytes.\nReceived %lu bytes.\n", file_size,file_info.st_size);

  clock_gettime(CLOCK_REALTIME, &final);
  printf("Time: %f\n", final.tv_sec-init.tv_sec + (final.tv_nsec-init.tv_nsec)/1E9);

  close(fd);
  chmod(file_name, file_mode);

  return 0;
}
