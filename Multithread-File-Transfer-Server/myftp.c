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


int SendData(int sd, void* buffer, int length) {
	int n;
	int received = 0;
	while (received != length) {
		if ((n = send(sd, buffer + received, length - received, 0)) < 0) {
			if (errno == EINTR)
				n = 0;
			else
				return -1;
		} else if (n == 0) {
			return 0;
		}
		received += n;
	}
	return length;
}


int RecvData(int sd, void* buffer, int length) {
	int n;
	int received = 0;
	while (received != length) {
		if ((n = recv(sd, buffer + received, length - received, 0)) < 0) {
			if (errno == EINTR)
				n = 0;
			else
				return -1;
		} else if (n == 0) {
			return 0;
		}
		received += n;
	}
	return length;
}
