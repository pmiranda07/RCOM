#ifndef DATALINK_LAYER_H
#define DATALINK_LAYER_H

#define FLAG 0x7E
#define ESCAPE 0x7D
#define STUFFING_BYTE 0x20

#define SEND 0x03
#define RECEIVE 0x01

#define SET 0x03
#define UA 0x07
#define DISC 0x0B
#define RR 0x05
#define REJ 0x01

#define COM1 0
#define COM2 1
#define COM1_PORT "/dev/ttyS0"
#define COM2_PORT "/dev/ttyS1"

#define US_FRAME_LENGTH 5
#define I_FRAME_HEADER_SIZE 6

#define DATA_PACKET_BYTE 1
#define START_PACKET_BYTE 2
#define END_PACKET_BYTE 3

typedef enum { TRANSMITTER, RECEIVER } status;

int writetty(int fd, char *buf, int buf_length);
int readtty(int fd, char *frame, int *frame_len);
int send_US_frame(int fd, char *frame, int len, int (*is_reply_valid)(char *));
int send_I_frame(int fd, char *frame, int len);
char *create_I_frame(int *frame_len, char *packet, int packet_len);
char *create_US_frame(int *frame_len, int control_byte);
int frame_UA(char *reply);
int frame_RR(char *reply, int reply_len);
int frame_REJ(char *reply, int reply_len);
int frame_DISC(char *reply);
int I_frame_header_valid(char *frame, int frame_len);
void timeout(int signum);
int has_valid_sequence_number(char control_byte, int s);
int reset_settings(int fd);
int close_receiver_connection(int fd);
/**
* Change the terminal settings
* return -1 on error
*/
int set_terminal_attributes(int fd);
/**
* Stuffing the frame given.
*/
char *stuff(char *packet, int *packet_len);
/**
* Destuffing the frame given.
*/
void destuff(char *packet, char *destuffed, int *packet_len);

/**
* Opens the terminal refered to by terminal.
* Updates the port settings and saves the old ones to be reset.
* Depending on status, it send a SET or UA frame.
* Returns the according file descriptor on success,
* returning -1 otherwise.
*/
int llopen(int port, status stat);

/**
* Writes the given msg with len length to the
* given fd.
* Returns -1 on error.
*/
int llwrite(int fd, char *msg, int len);

/**
* Reads the message from fd and places it on
* msg, updating len accordingly.
* Returns -1 on error.
*/
int llread(int fd, char *msg, int *packet_len);

/**
* Closes the given fd and sets the port settings.
* Returns -1 on error.
*/
int llclose(int fd);

/**
* Closes the given fd and resets the port settings.
* Should be used when there is a problem with the connection.
*/
void force_close(int fd);

/**
* Initialize data_link struct variables.
*/
void init_data_link();

/**
* Get the number of timeouts at the end.
*/
int getTotalTimeouts();

#endif
