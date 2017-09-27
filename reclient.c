#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>

#define h_addr h_addr_list[0]
char buf[1024];
int main(int argc, char **argv){
	char *remhost;
	char ch = 0;
	unsigned short remport;
	int sock;
	struct sockaddr_in remote;
	struct hostent *h;
	int err = 0;
	char recieve[6000];

	char *request = NULL, *tempPointer = NULL;
	int count = 0, sizeRequest = 1024;

	int num = 0;
	remhost = argv[1]; remport = atoi(argv[2]);

	sock = socket(AF_INET, SOCK_STREAM, 0);

	bzero((char *)&remote, sizeof(remote));
	remote.sin_family = (short) AF_INET;
	h = gethostbyname(remhost);
	if(h == NULL){
		sprintf(buf, "%s is not a host\n", argv[1]);
		write(1, buf, strlen(buf));
		exit(-1);
	}
	bcopy((char *)h->h_addr, (char *)&remote.sin_addr.s_addr, h->h_length);
	remote.sin_port = htons(remport);

	err = connect(sock, (struct sockaddr *)&remote, sizeof(remote));
	if(err < 0){
		sprintf(buf, "%s\n", strerror(errno));
		write(1, buf, strlen(buf));
		exit(-1);
	}

	request = malloc(sizeRequest);
	if(request == NULL){
		sprintf(buf, "Malloc failed\n");
		write(1, buf, strlen(buf));
		exit(-1);
	}

	/*Get client input*/
	do{
		if(count >= sizeRequest){
			sizeRequest = sizeRequest * 2;
			tempPointer = realloc(request, sizeRequest);
			if(tempPointer ==  NULL){
				sprintf(buf, "Reallocation error\n");
				write(1, buf, strlen(buf));
				free(request);
				exit(-1);
			}
			else{
				request = tempPointer;
			}

		}
		err = read(0, &ch, 1);
	
		if(err == 0 || ch == '\n'){
			break;
		}
		else{
			request[count] = ch;
		}
		count++;

	}while(err != 0 && ch != '\n');


	/*write output to server*/

	if((num = send(sock, request, strlen(request), 0)) < 0){
		printf("error");
	}
	free(request);


	/*recieve response from server*/
	recv(sock, recieve, sizeof(recieve), 0);
	write(1, recieve, strlen(recieve));

	exit(0);

}