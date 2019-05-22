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

#include "myftp.h"

#define BUFFER_SIZE 4096

void ListFile(int sd) {

	/* Send the LIST_REQUEST */

	struct message_s LIST_REQUEST;
	struct message_s LIST_REPLY;
	unsigned char* payload;

	memset(LIST_REQUEST.protocol, 0, sizeof(LIST_REQUEST.protocol));
	strcpy(LIST_REQUEST.protocol, "myftp");
	LIST_REQUEST.type = 0xA1;
	LIST_REQUEST.length = htonl(sizeof(LIST_REQUEST));

	int len;
	if ((len = SendData(sd, (void *)&LIST_REQUEST, sizeof(LIST_REQUEST))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* Receive the LIST_REPLY */

	if ((len = RecvData(sd, (void *)&LIST_REPLY, sizeof(LIST_REPLY))) <= 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* Append the '\0' & Display the Message */

	unsigned char protocol[6];
	strcpy(protocol, LIST_REPLY.protocol);
	protocol[5] = '\0';

	printf("---LIST_REPLY---\nProtocol: %s\nType: %02X\nLength: %d\n----------------\n", protocol, LIST_REPLY.type, ntohl(LIST_REPLY.length));

	/* Receive the LIST */

	unsigned int payload_size = ntohl(LIST_REPLY.length) - sizeof(LIST_REPLY);
	unsigned int remaining_size = payload_size;
	payload = malloc(payload_size);

	while (remaining_size > BUFFER_SIZE) {
		if ((len = RecvData(sd, payload + payload_size - remaining_size, BUFFER_SIZE)) <= 0) {
			printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}

		remaining_size -= BUFFER_SIZE;
	}

	if ((len = RecvData(sd, payload + payload_size - remaining_size, remaining_size)) < 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	printf("List of Files:\n%s\n", payload);

	/* Free the Resource */

	free(payload);

}


void GetFile(int sd, char* filename) {

	struct message_s GET_REQUEST;
	struct message_s GET_REPLY;
	struct message_s FILE_DATA;
	unsigned char buffer[BUFFER_SIZE];
	FILE *fp;
	
	/* GET_REQUEST */

	memset(GET_REQUEST.protocol, 0, sizeof(GET_REQUEST.protocol));
	strcpy(GET_REQUEST.protocol, "myftp");
	GET_REQUEST.type = 0xB1;
	GET_REQUEST.length = htonl(sizeof(GET_REQUEST) + strlen(filename) + 1);
	
	int len;
	if ((len = SendData(sd, (void *)&GET_REQUEST, sizeof(GET_REQUEST))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	if ((len = SendData(sd, filename, strlen(filename) + 1)) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	
	/* Receive the GET_REPLY Message */

	if ((len = RecvData(sd, (void *)&GET_REPLY, sizeof(GET_REPLY))) <= 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	
	/* Display the Message & Append the '\0' */

	unsigned char protocol[6];
	strcpy(protocol, GET_REPLY.protocol);
	protocol[5] = '\0';
	
	printf("---GET_REPLY---\nProtocol: %s\nType: %02X\nLength: %d\n---------------\n", protocol, GET_REPLY.type, ntohl(GET_REPLY.length));
	
	/* Handle the GET_REPLY Message */

	if (GET_REPLY.type == 0xB3) {
		printf("[ERROR] Download Error: File %s does not exist on server.\n", filename);
		exit(0);
	}
	else if (GET_REPLY.type == 0xB2) {

		/* Receive the FILE_DATA Message */

		if ((len = RecvData(sd, (void *)&FILE_DATA, sizeof(FILE_DATA))) <= 0) {
			printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}
		
		/* Display the FILE_DATA & Append the '\0' */

		strcpy(protocol, FILE_DATA.protocol);
		protocol[5] = '\0';
	
		printf("---FILE_DATA---\nProtocol: %s\nType: %02X\nLength: %d\n---------------\n", protocol, FILE_DATA.type, ntohl(FILE_DATA.length));
		
		/* Open the File */

		if ((fp = fopen(filename, "wb")) == NULL) {
			printf("[ERROR] File I/O Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}
		
		/* Receieve the File */

		unsigned int filesize = ntohl(FILE_DATA.length) - sizeof(FILE_DATA);
		unsigned int remaining_size = filesize;
		
		while (remaining_size > BUFFER_SIZE) {
			if ((len = RecvData(sd, buffer, BUFFER_SIZE)) <= 0) {
				printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
				exit(0);
			}
			
			fwrite(buffer, BUFFER_SIZE, 1, fp);
			remaining_size -= BUFFER_SIZE;
		}
		
		if ((len = RecvData(sd, buffer, remaining_size)) <= 0) {
			printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}
			
		fwrite(buffer, remaining_size, 1, fp);
		fclose(fp);
		
		printf("File %s download complete.\n", filename);
	}
}


void PutFile(int sd, char* filename) {

	struct message_s PUT_REQUEST;
	struct message_s PUT_REPLY;
	struct message_s FILE_DATA;
	unsigned char buffer[BUFFER_SIZE];

	/* Send the PUT_REQUEST */

	memset(PUT_REQUEST.protocol, 0, sizeof(PUT_REQUEST.protocol));
	strcpy(PUT_REQUEST.protocol, "myftp");
	PUT_REQUEST.type = 0xC1;
	PUT_REQUEST.length = htonl(sizeof(PUT_REQUEST) + strlen(filename) + 1);
	
	int len;
	if ((len = SendData(sd, (void *)&PUT_REQUEST, sizeof(PUT_REQUEST))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	if ((len = SendData(sd, filename, strlen(filename) + 1)) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	
	/* Receive the PUT_REPLY */

	if ((len = RecvData(sd, (void *)&PUT_REPLY, sizeof(PUT_REPLY))) <= 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	
	/* Display Message and Append the '\0' */

	unsigned char protocol[6];
	strcpy(protocol, PUT_REPLY.protocol);
	protocol[5] = '\0';
	
	printf("---PUT_REPLY---\nProtocol: %s\nType: %02X\nLength: %d\n---------------\n", protocol, PUT_REPLY.type, ntohl(PUT_REPLY.length));
	
	/* Open the File */

	FILE *fp;

	if ((fp = fopen(filename, "rb")) == NULL) {
		printf("[ERROR] File I/O Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	struct stat st;
	stat(filename, &st);
	unsigned int filesize = st.st_size;
	unsigned int remaining_size = filesize;

	/* Send the FILE_DATA */

	memset(FILE_DATA.protocol, 0, sizeof(FILE_DATA.protocol));
	strcpy(FILE_DATA.protocol, "myftp");
	FILE_DATA.type = 0xFF;
	FILE_DATA.length = htonl(sizeof(FILE_DATA) + filesize);
	
	if ((len = SendData(sd, (void *)&FILE_DATA, sizeof(FILE_DATA))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	
	while (remaining_size > BUFFER_SIZE) {

		fread(buffer, BUFFER_SIZE, 1, fp);
		
		if ((len = SendData(sd, buffer, BUFFER_SIZE)) <= 0) {
			printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}
		
		remaining_size -= BUFFER_SIZE;

	}
		
	fread(buffer, remaining_size, 1, fp);	
	
	if ((len = SendData(sd, buffer, remaining_size)) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}
	
	fclose(fp);
		
	printf("File %s upload complete.\n", filename);
}


int main(int argc, char *argv[]) {

	/* Parse the Command-line Argument */

	int isList = 0;
	int isGet = 0;
	int isPut = 0;

	if (argc < 4 || argc > 5) {
		printf("[ERROR] Wrong Number of Arguments.(%d)\n",argc);
		exit(0);
	}

	if (strcmp(argv[3], "list") == 0) {
		isList = 1;
		if (argc > 4) {
			printf("[ERROR] Wrong Number of Arguments.\n");
			exit(0);
		}
	}
	else if (strcmp(argv[3], "get") == 0) {
		isGet = 1;
		if (argc < 5) {
			printf("[ERROR] Wrong Number of Arguments.\n");
			exit(0);
		}
	}
	else if (strcmp(argv[3], "put") == 0) {
		isPut = 1;
		if (argc < 5) {
			printf("[ERROR] Wrong Number of Arguments.\n");
			exit(0);
		}
	}
	else {
		printf("[ERROR] Wrong Command.\n");
		exit(0);
	}

	/* Set up the Connection*/

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));

	if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
		printf("[ERROR] inet_pton failed for address '%s'\n", argv[1]);
		exit(0);
	}

	if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		printf("[ERROR] Connection Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* Three Function */

	if (isList) {
		ListFile(sd);
	}
	else if (isGet) {
		GetFile(sd, argv[4]);
	}
	else if (isPut) {
		PutFile(sd, argv[4]);
	}

	return 0;

}
