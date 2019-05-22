#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include "myftp.h"

#define BUFFER_SIZE 4096


void ListFile(int client_sd) {

	struct dirent *entry;
	char* payload;
	char* tmp;
	unsigned int payload_len = BUFFER_SIZE;
	int len;
	
	/* Generate the LIST */

	DIR *dir;
		
	if ((dir = opendir("./data")) == NULL) {
		printf("[ERROR] opendir Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}
	
	payload = malloc(payload_len);
	payload[0] = '\0';
	
	while ((entry = readdir(dir)) != NULL) {
		if (strlen(payload) + strlen(entry->d_name) + strlen("\n") + 1 > payload_len) {
			payload_len += BUFFER_SIZE;
			tmp = malloc(payload_len);
			strcpy(tmp, payload);
			free(payload);
			payload = tmp;
		}
		strcat(payload, entry->d_name);
		strcat(payload, "\n");
	}
		
	closedir(dir);

	/* Send the LIST_REPLY */
	
	struct message_s LIST_REPLY;
	memset(LIST_REPLY.protocol, 0, sizeof(LIST_REPLY.protocol));
	strcpy(LIST_REPLY.protocol, "myftp");
	LIST_REPLY.type = 0xA2;
	LIST_REPLY.length = htonl(sizeof(LIST_REPLY) + strlen(payload) + 1);
		
	if ((len = SendData(client_sd, (void *)&LIST_REPLY, sizeof(LIST_REPLY))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}

	/* Send the List */
	
	unsigned int remaining_size = strlen(payload) + 1;
	
	while (remaining_size > BUFFER_SIZE) {
		if ((len = SendData(client_sd, payload + strlen(payload) + 1 - remaining_size, BUFFER_SIZE)) <= 0) {
			printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			return;
		}
		
		remaining_size -= BUFFER_SIZE;
	}
	
	if ((len = SendData(client_sd, payload + strlen(payload) + 1 - remaining_size, remaining_size)) < 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}
	
	/* Free the Resource */

	free(payload);

}


void GetFile(int client_sd, int RECV_LENGTH) {

	struct message_s GET_REPLY;
	struct message_s FILE_DATA;
	FILE *fp;
	int len;

	/* Receive the File Name */

	int fileLen = ntohl(RECV_LENGTH) - sizeof(GET_REPLY);
	char* fileName = malloc(fileLen);
	
	if ((len = RecvData(client_sd, fileName, fileLen)) <= 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}

	/* Generate the Path Name */

	char* pathName = malloc((fileLen + strlen("./data/")));
	strcat(pathName, "./data/");
	strcat(pathName, fileName);
	
	/* Open the File */

	fp = fopen(pathName, "rb");

	if (fp == NULL) {

		/* Send the GET_REPLY Message */

		memset(GET_REPLY.protocol, 0, sizeof(GET_REPLY.protocol));
		strcpy(GET_REPLY.protocol, "myftp");
		GET_REPLY.type = 0xB3;
		GET_REPLY.length = htonl(sizeof(GET_REPLY));

		if ((len = SendData(client_sd, (void *)&GET_REPLY, sizeof(GET_REPLY))) <= 0) {
			printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			return;
		}

		printf("[ERROR] File I/O Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;

	}

	/* Send the GET_REPLY Message */

	memset(GET_REPLY.protocol, 0, sizeof(GET_REPLY.protocol));
	strcpy(GET_REPLY.protocol, "myftp");
	GET_REPLY.type = 0xB2;
	GET_REPLY.length = htonl(sizeof(GET_REPLY));

	if ((len = SendData(client_sd, (void *)&GET_REPLY, sizeof(GET_REPLY))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}

	/* Send the FILE_DATA Message */

	struct stat st;
	stat(pathName, &st);
	unsigned int filesize = st.st_size;
	unsigned int remaining_size = filesize;

	memset(FILE_DATA.protocol, 0, sizeof(FILE_DATA.protocol));
	strcpy(FILE_DATA.protocol, "myftp");
	FILE_DATA.type = 0xFF;
	FILE_DATA.length = htonl(sizeof(FILE_DATA) + filesize);

	if ((len = SendData(client_sd, (void *)&FILE_DATA, sizeof(FILE_DATA))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}

	/* Send the File */

	unsigned char buffer[BUFFER_SIZE];

	while (remaining_size > BUFFER_SIZE) {
		fread(buffer, BUFFER_SIZE, 1, fp);

		if ((len = SendData(client_sd, buffer, BUFFER_SIZE)) <= 0) {
			printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			return;
		}

		remaining_size -= BUFFER_SIZE;
	}

	fread(buffer, remaining_size, 1, fp);

	if ((len = SendData(client_sd, buffer, remaining_size)) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}

	fclose(fp);
	printf("File %s sent.\n", fileName);

	/* Free the Resource */

	free(pathName);
	free(fileName);

}


void PutFile(int client_sd, char* filename) {

	struct message_s PUT_REPLY;
	struct message_s FILE_DATA;
	unsigned char buffer[BUFFER_SIZE];
	
	/* Parse the Path Name and Open the File */

	char* path;
	FILE *fp;

	path = malloc(strlen("./data/") + strlen(filename) + 1);
	strcpy(path, "./data/");
	strcat(path, filename);

	if ((fp = fopen(path, "wb")) == NULL) {
		printf("[ERROR] File I/O Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}
	
	/* Send the PUT_REPLY */

	memset(PUT_REPLY.protocol, 0, sizeof(PUT_REPLY.protocol));
	strcpy(PUT_REPLY.protocol, "myftp");
	PUT_REPLY.type = 0xC2;
	PUT_REPLY.length = htonl(sizeof(PUT_REPLY));
	
	int len;
	if ((len = SendData(client_sd, (void *)&PUT_REPLY, sizeof(PUT_REPLY))) <= 0) {
		printf("[ERROR] Send Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		fclose(fp);
		remove(path);
		return;
	}
	
	/* Send the FILE_DATA */

	if ((len = RecvData(client_sd, (void *)&FILE_DATA, sizeof(FILE_DATA))) <= 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		fclose(fp);
		remove(path);
		return;
	}
	
	/* Display the Message and Append the '\0' */

	unsigned char protocol[6];
	strcpy(protocol, FILE_DATA.protocol);
	protocol[5] = '\0';
	
	printf("---FILE_DATA---\nProtocol: %s\nType: %02X\nLength: %d\n---------------\n", protocol, FILE_DATA.type, ntohl(FILE_DATA.length));
	
	/* Upload the File */

	unsigned int filesize = ntohl(FILE_DATA.length) - sizeof(FILE_DATA);
	unsigned int remaining_size = filesize;
	
	while (remaining_size > BUFFER_SIZE) {
		if ((len = RecvData(client_sd, buffer, BUFFER_SIZE)) <= 0) {
			printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			fclose(fp);
			remove(path);
			return;
		}
		
		fwrite(buffer, BUFFER_SIZE, 1, fp);
		remaining_size -= BUFFER_SIZE;
	}
	
	if ((len = RecvData(client_sd, buffer, remaining_size)) <= 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		fclose(fp);
		remove(path);
		return;
	}
	
	fwrite(buffer, remaining_size, 1, fp);
	fclose(fp);
		
	printf("File %s received.\n", filename);
}


void HandleTCPClient(int client_sd) {
	
	/* Receieve the MESSAGE from Client */

	struct message_s RECV_MESSAGE;
	memset(RECV_MESSAGE.protocol, 0, sizeof(RECV_MESSAGE.protocol));
	int len;

	if ((len = RecvData(client_sd, (void *)&RECV_MESSAGE, sizeof(RECV_MESSAGE))) < 0) {
		printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		return;
	}

	/* Append the '\0' */
	unsigned char protocol[6];
	strcpy(protocol, RECV_MESSAGE.protocol);
	protocol[5] = '\0';
	
	if (strcmp(protocol, "myftp") == 0)
		printf("---RECV_MESSAGE---\nProtocol: %s\nType: %02X\nLength: %d\n------------------\n", protocol, RECV_MESSAGE.type, ntohl(RECV_MESSAGE.length));
			
	/* Send the Message to Client */

	if (RECV_MESSAGE.type == 0xA1) {

		ListFile(client_sd);

	}
	else if(RECV_MESSAGE.type == 0xB1){

		GetFile(client_sd, RECV_MESSAGE.length);

	}
	else if (RECV_MESSAGE.type == 0xC1){

		/* PUT_REQUEST payload */

		unsigned int payload_len = ntohl(RECV_MESSAGE.length) - sizeof(RECV_MESSAGE);
		char* filename = malloc(payload_len);
		
		if ((len = RecvData(client_sd, filename, payload_len)) <= 0) {
			printf("[ERROR] Recv Error: %s (ERRNO: %d)\n", strerror(errno), errno);
			return;
		}
		
		PutFile(client_sd, filename);
		
		free(filename);

	}
}

struct ThreadArgs {
	int client_sd;
};

void* ThreadMain(void *threadArgs) {

	int client_sd;
	client_sd = ((struct ThreadArgs *)threadArgs)->client_sd;
	free(threadArgs);
	HandleTCPClient(client_sd);
	pthread_detach(pthread_self()); 
	close(client_sd);
	return (NULL);

}

int main(int argc, char *argv[]) {

	/* Parse the Command-line argument*/

	if (argc != 2) {
		printf("[ERROR] Wrong Number of Arguments.\n");
		exit(0);
	}
	
	/* Set up the Listening Socket */

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	int val = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) == -1) {
		perror("setsocketopt");
		exit(1);
	}
	int client_sd;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(argv[1]));

	if (bind(sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		printf("[ERROR] Bind Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	if (listen(sd, 3) < 0) {
		printf("[ERROR] Listen Error: %s (ERRNO: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* Set up the Connection */

	int addr_len = sizeof(client_addr);

	while (1) {

		/* Assign one thread for each client */

		if ((client_sd = accept(sd, (struct sockaddr *) &client_addr, &addr_len)) < 0) {
			printf("[ERROR] Accept ERROR: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}

		struct ThreadArgs *threadArgs = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs));
		if (threadArgs == NULL) {
			printf("[ERROR]: malloc ERROR: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}

		threadArgs->client_sd = client_sd;

		pthread_t threadID;
		if (pthread_create(&threadID, NULL, ThreadMain, threadArgs) != 0) {
			printf("[ERROR]: pthread_create ERROR: %s (ERRNO: %d)\n", strerror(errno), errno);
			exit(0);
		}

	}

	return 0;

}
