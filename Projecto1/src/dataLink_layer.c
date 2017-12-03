#include "dataLink_layer.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>

#define BAUDRATE B38400

volatile int STOP = 0;
struct termios oldtio;
int connection_timeouts = 0;
int ignore_flag = 0;
char r = 0;
int total_time_outs=0;

struct {
  char port[20]; /* Serial port device e.g. /dev/ttyS0 */
  unsigned int sequence_num; /* Frame sequence number (0 or 1) */
  unsigned int timeout;      /* Time to timeout e.g. 1 second */
  unsigned int num_retries;  /* Maximum number of retries */
  status stat;
  struct sigaction old_action;
} data_link;


/*
* Definitions
*/

int set_terminal_attributes(int fd) {
  struct termios newtio;

  if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
    printf("Error getting port settings.\n");
    close(fd);
    return -1;
  }

  bzero(&newtio, sizeof(newtio));

  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused in 1/10th of a second*/
  newtio.c_cc[VMIN] = 1; /* blocking read until x chars received */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    printf("Error setting port settings.\n");
    close(fd);
    return -1;
  }

  return 0;
}

int I_frame_header_valid(char *frame, int frame_len) {
  if (frame_len < 6)
    return 0;

  return frame[0] == FLAG && frame[1] == SEND &&
         frame[3] == (frame[1] ^ frame[2]);
}

/**
* TODO:
* if stat == TRANSMITTER -> send SET, receive UA
* reverse if stat == RECEIVE
*/
int llopen(int port, status stat) {

  int fd; // value to be returned
  int frame_len;

  switch (port) {
  case COM1:
    strcpy(data_link.port, COM1_PORT);
    break;

  case COM2:
    strcpy(data_link.port, COM2_PORT);
    break;

  default:
    printf("Invalid port!\n");
    return -1;
  }

  if (stat != TRANSMITTER && stat != RECEIVER) {
    printf("Invalid status.\n");
    return -1;
  }

  data_link.stat = stat;
  /**
  * Opening the serial port
  */
  if ((fd = open(data_link.port, O_RDWR | O_NOCTTY)) < 0) {
    printf("Error opening terminal '%s'\n", data_link.port);
    return -1;
  }

  if (set_terminal_attributes(fd) != 0) {
    printf("Error set_terminal_attributes() in function llopen().\n");
    return -1;
  }

  struct sigaction new_action;
  new_action.sa_handler = timeout;
  new_action.sa_flags &=
      !SA_RESTART; // Needed in order to block read from restarting.

  if (sigaction(SIGALRM, &new_action, &data_link.old_action) == -1)
    printf("Error installing new SIGALRM handler.\n");

  if (stat == TRANSMITTER) {
    char *frame = create_US_frame(&frame_len, SET);
    if (send_US_frame(fd, frame, frame_len, frame_UA) == -1) {
      printf("send_US_frame failed\n");
      close(fd);
      return -1;
    }

  } else {
    char msg[255];
    int msg_len;
    if (readtty(fd, msg, &msg_len) == -1) {
      printf("Error readtty() in function llopen().\n");
      return -1;
    }

    char *frame = create_US_frame(&frame_len, UA);
    if (writetty(fd, frame, frame_len) == -1) {
      printf("Error writetty() in function llopen().\n");
      return -1;
    }
  }

  printf("connection succesfully established.\n");

  printf("DATA_LINK values: baudrate: %d,timeout: %d,num_retries: %d.\n\n\n\n\n",BAUDRATE,data_link.timeout,data_link.num_retries);

  return fd;
}

int llwrite(int fd, char *packet, int packet_len) {
  // Writes and checks for validity
  // Using send_I_frame
  int frame_len;
  char *frame = create_I_frame(&frame_len, packet, packet_len);
  return send_I_frame(fd, frame, frame_len);
}

int llread(int fd, char *packet, int *packet_len) {
  char *reply;
  int reply_len;

  char frame[512];
  int frame_len;
  ignore_flag = 0;
  static int s = 0;

  int read_succesful = 0;
  while (!read_succesful) {
    readtty(fd, frame, &frame_len);
    if (frame_DISC(frame)) {
      close_receiver_connection(fd);
      reset_settings(fd);
      return -1;
    }

    if (!I_frame_header_valid(frame, frame_len)) {// Invalid frame
      printf("Invalid frame header. Rejecting frame..\n");
      reply = create_US_frame(&reply_len, REJ);
    } else { // Frame is valid
      // Updates the packet length.
      *packet_len = frame_len - I_FRAME_HEADER_SIZE;

      char expected_bcc2;
      if (frame[frame_len - 3] == ESCAPE) {
        expected_bcc2 = frame[frame_len - 2] ^ STUFFING_BYTE;
        // If the BCC was stuffed, the frame header is one byte bigger
        // So the packet length will be one byte shorter.
        *packet_len = *packet_len - 1;
      } else
        expected_bcc2 = frame[frame_len - 2];

      destuff(frame + 4, packet, packet_len);

      /* Create a BCC2 for the I frame
      check if the received one is correct*/
      char calculated_bcc2 = 0;
      int i;
      for (i = 0; i < *packet_len; i++)
        calculated_bcc2 ^= packet[i];

      if (calculated_bcc2 ==
          expected_bcc2) { // valid BCC2 - may still be a duplicate

        reply = create_US_frame(&reply_len, RR);

        /* Only need to check sequence number if packet is a data packet.
        * If it is, and the sequence number is invalid, discard the packet
        * by setting its length to 0 */
        if (!has_valid_sequence_number(frame[2], s)) {   //sees if the actual sequence number is the same as the previous transfer
          *packet_len = 0;
          printf("Found duplicate frame. Discarding...\n");
        } else {
          //Only flip sequence number if the whole frame is valid
          //And not a duplicate.
          s = !s;
        }

        read_succesful = 1;
      } else { // BCC2 does not match -> check sequence number
        if (has_valid_sequence_number(frame[2], s)) { // new frame, request retry
          reply = create_US_frame(&reply_len, REJ);
          printf("Found new incorrect frame. Rejecting...\n");
        } else {
          reply = create_US_frame(&reply_len,
                                  RR); // duplicate frame, send RR and discard
          read_succesful = 1;
          *packet_len = 0;
          printf("Found duplicate frame. Discarding...\n");
        }
      }

      if (writetty(fd, reply, reply_len) != 0) {
        printf("Error writetty() in function llread().\n");
        return -1;
      }
  }

    if(!read_succesful)
      ignore_flag = 1;


  }

  return 0;
}

int llclose(int fd) {
  char *frame;
  int frame_len = 0;

  if (data_link.stat == TRANSMITTER) {
    frame = create_US_frame(&frame_len, DISC);
    if (send_US_frame(fd, frame, frame_len, frame_DISC) != 0) {
      printf("Couldn't send frame on llclose().\n");
      reset_settings(fd);
      return -1;
    }

    if (writetty(fd, create_US_frame(&frame_len, UA), frame_len) != 0) {
      printf("Couldn't write to tty on llclose()\n");
      reset_settings(fd);
      return -1;
    }

  } else {
    char msg[256];
    int msg_len = 0;

    if (readtty(fd, msg, &msg_len) != 0) {
      printf("Couldn't read from tty on llclose()\n");
      reset_settings(fd);
      return -1;
    }

    if (frame_DISC(msg)){
      close_receiver_connection(fd);
    }

    if (reset_settings(fd) == 0)
        printf("Connection succesfully closed.\n");
    }

  return 0;
}

void timeout(int signum) {

  total_time_outs++;
  connection_timeouts++;
}

char *create_US_frame(int *frame_len, int control_byte) {
  static char r = 0;
  char *buf = (char *)malloc(US_FRAME_LENGTH * sizeof(char));
  buf[0] = FLAG;

  if (data_link.stat == TRANSMITTER) {
    if (control_byte == SET || control_byte == DISC)
      buf[1] = SEND;
    else
      buf[1] = RECEIVE;
  } else {
    if (control_byte == RR || control_byte == REJ || control_byte == UA)
      buf[1] = SEND;
    else
      buf[1] = RECEIVE;
  }

  if (control_byte == RR || control_byte == REJ) {
    buf[2] = r << 7 | control_byte;
    r = !r;
  } else
    buf[2] = control_byte;

  buf[3] = buf[1] ^ buf[2];
  buf[4] = FLAG;
  *frame_len = US_FRAME_LENGTH;

  return buf;
}

int writetty(int fd, char *buf, int buf_length) {
  int total_written_chars = 0;
  int written_chars = 0;

  while (total_written_chars < buf_length) {
    written_chars = write(fd, buf, buf_length);

    if (written_chars <= 0) {
      printf("Written chars: %d\n", written_chars);
      printf("%s\n", strerror(errno));
      return -1;
    }

    total_written_chars += written_chars;
  }

  return 0;
}

char *create_I_frame(int *frame_len, char *packet, int packet_len) {
  static char s = 1;
  s = !s;

  // Calculate BCC2
  char bcc2 = 0;

  int i;
  for (i = 0; i < packet_len; i++)
    bcc2 ^= packet[i];

  int bcc_len = 1;
  char *stuffed_bcc = stuff(&bcc2, &bcc_len);
  char *stuffed_packet = stuff(packet, &packet_len);

  *frame_len = 5 + packet_len + bcc_len;
  char *frame = (char *)malloc(*frame_len * sizeof(char));

  frame[0] = FLAG;
  frame[1] = SEND;
  frame[2] = s << 6;
  frame[3] = frame[1] ^ frame[2];
  memcpy(frame + 4, stuffed_packet, packet_len);        // Copy packet content
  memcpy(frame + packet_len + 4, stuffed_bcc, bcc_len); // Copy bcc2

  frame[packet_len + 4 + bcc_len] = FLAG;
  return frame;
}

int readtty(int fd, char *frame, int *frame_len) {
  int read_chars = 0;
  char buf;
  *frame_len = 0;
  int initial_flag = 0;

  STOP = 0;
  while (!STOP) {
    read_chars = read(fd, &buf, 1); /* returns after x chars have been input */

    if (read_chars > 0) { // If characters were read
      if (buf == FLAG) { //If the char is a FLAG
        //Set frame start to true.
        if(!ignore_flag) {
          initial_flag = !initial_flag;

          //If it is the second flag, then the frame
          //has ended
          if (!initial_flag)
          STOP = 1;

          frame[*frame_len] = buf;
          (*frame_len)++;
        } else
          ignore_flag = 0;
      } else {
        //If the char is not a flag and
        //the final flag has not been found
        //then add it to the frame.
        if(initial_flag) {
          frame[*frame_len] = buf;
          (*frame_len)++;
        }
      }
    } else {// If no characters were read or there was an error
      return -1;
    }
  }

  return 0;
}

void destuff(char *packet, char *destuffed, int *packet_len) {
  int destuff_iterator = 0;

  int i;
  for (i = 0; i < *packet_len; i++) {
    if (packet[i] == ESCAPE) {
      destuffed[destuff_iterator] = packet[i + 1] ^ STUFFING_BYTE;
      i++;
    } else
      destuffed[destuff_iterator] = packet[i];
    destuff_iterator++;
  }

  *packet_len = destuff_iterator;
}

int frame_UA(char *reply) {

  return (reply[0] == FLAG &&
          reply[1] == ((data_link.stat == TRANSMITTER) ? SEND : RECEIVE) &&
          reply[2] == UA && reply[3] == (reply[1] ^ reply[2]) &&
          reply[4] == FLAG);
}

int frame_DISC(char *reply) {
  return (reply[0] == FLAG &&
          reply[1] == ((data_link.stat == TRANSMITTER) ? RECEIVE : SEND) &&
          reply[2] == DISC && reply[3] == (reply[1] ^ reply[2]) &&
          reply[4] == FLAG);
}


int send_US_frame(int fd, char *frame, int len, int (*is_reply_valid)(char *)) {
  char reply[255];
  int reply_len;

  connection_timeouts = 0;
  while (connection_timeouts <= data_link.num_retries + 1) {
    if (writetty(fd, frame, len)) {
      printf("Failed write.\n");
      return -1;
    }

    alarm(data_link.timeout);

    if (readtty(fd, reply, &reply_len) == 0) {
      connection_timeouts = 0;
      if (is_reply_valid(reply)) {
          alarm(0);
          return 0;
      }
    }

    alarm(0);
    if (connection_timeouts > 0 && connection_timeouts < data_link.num_retries)
    printf("Connection failed. Retrying %d out of %d...\n",
    connection_timeouts, data_link.num_retries);
  }

  return -1;
}

int send_I_frame(int fd, char *frame, int len) {

  char reply[255];
  int reply_len;

  connection_timeouts = 0;
  while (connection_timeouts <= data_link.num_retries + 1) {
    if (writetty(fd, frame, len)) {
      printf("Failed write.\n");
      return -1;
    }

    alarm(data_link.timeout);

    if (readtty(fd, reply, &reply_len) == 0) {
      // If the read() was successful
      if(connection_timeouts > 0)
      printf("Connection reestablished. Resuming...\n");

      connection_timeouts = 0;

      alarm(0);
      // If a RR is received, proceed to the next frame
      if (frame_RR(reply, reply_len)) {
          r = !r;
          return 0;
      } else if(frame_REJ(reply, reply_len)) {
        // If a REJ is received, resend the frame.
        connection_timeouts = 0;
      }
    }

    if (connection_timeouts > 0 && connection_timeouts <= data_link.num_retries)
      printf("Connection failed. Retrying %d out of %d...\n",
        connection_timeouts, data_link.num_retries);
    alarm(0);
  }

  return -1;
}

int frame_RR(char *reply, int reply_len) {
  if(reply_len != 5)
    return 0;

  return (
      reply[0] == (unsigned char)FLAG &&
      reply[1] ==
          (unsigned char)((data_link.stat == TRANSMITTER) ? SEND : RECEIVE) &&
      (unsigned char)reply[2] == (unsigned char)(r << 7 | RR) &&
      (unsigned char)reply[3] == (unsigned char)(reply[1] ^ reply[2]) &&
      reply[4] == (unsigned char)FLAG);
}

int frame_REJ(char *reply, int reply_len) {
  if(reply_len != 5)
    return 0;

  return (
      reply[0] == (unsigned char)FLAG &&
      reply[1] ==
          (unsigned char)((data_link.stat == TRANSMITTER) ? SEND : RECEIVE) &&
      (unsigned char)reply[2] == (unsigned char)(r << 7 | REJ) &&
      (unsigned char)reply[3] == (unsigned char)(reply[1] ^ reply[2]) &&
      reply[4] == (unsigned char)FLAG);
}

char *stuff(char *packet, int *packet_len) {
  // TODO: Variable size.
  char *stuffed = (char *)malloc(((*packet_len) + 255) * sizeof(char));

  int packet_iterator;
  int stuff_iterator = 0;

  for (packet_iterator = 0; packet_iterator < *packet_len; packet_iterator++) {

    if (packet[packet_iterator] == ESCAPE || packet[packet_iterator] == FLAG) {

      stuffed[stuff_iterator] = ESCAPE;
      stuffed[++stuff_iterator] = packet[packet_iterator] ^ STUFFING_BYTE;
    } else
      stuffed[stuff_iterator] = packet[packet_iterator];

    stuff_iterator++;
  }

  *packet_len = stuff_iterator;
  return stuffed;
}

int has_valid_sequence_number(char control_byte, int s) { //sees if the actual sequence number is the same as the previous one
  return (control_byte == (s << 6));
}

int close_receiver_connection(int fd) {
  int frame_len = 0;
  char *frame = create_US_frame(&frame_len, DISC);
  if (send_US_frame(fd, frame, frame_len, frame_UA) != 0) {
    printf("Couldn't send frame on llclose().\n");
    return -1;
  }

  return 0;
}

int reset_settings(int fd) {
  if (sigaction(SIGALRM, &data_link.old_action, NULL) == -1)
    printf("Error setting SIGALRM handler to original.\n");

  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    printf("Error settings old port settings.\n");

  if (close(fd)) {
    printf("Error closing terminal file descriptor.\n");
    return -1;
  }

  return 0;

}

void force_close(int fd) {
  printf("Forcing close application...\n");
  reset_settings(fd);
  close(fd);
}

void init_data_link(){

  data_link.timeout=3;
  data_link.num_retries=3;

}

int getTotalTimeouts(){
  return total_time_outs;
}
