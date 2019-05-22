#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


struct message_s {
	unsigned char protocol[5];	/* protocol string (5 bytes) */
	unsigned char type;			/* type (1 byte) */
	unsigned int length;		/* length (header + payload) (4 bytes) */
} __attribute__ ((packed));


int SendData(int sd, void* buffer, int length);


int RecvData(int sd, void* buffer, int length);
