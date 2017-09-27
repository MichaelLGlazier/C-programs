#define _POSIX_SOURCE
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
#include <signal.h>

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
	char nickName[128];
	char msg[129];
	char recMsg[260];
	int i = 0;
	char throwaway = 0;
	int num = 0;
	char inputBuf[128];
	pid_t child;
	if(argc != 3){
		exit(-1);
	}
	remhost = argv[1]; remport = atoi(argv[2]);

	sock = socket(AF_INET, SOCK_STREAM, 0);

	/*Connect to server*/
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

	sprintf(buf, "Connected to server successfully!\n");
	write(1, buf, strlen(buf));

	sprintf(buf, "Please input a nickname: ");
	write(1, buf, strlen(buf));

	i = 0;
	do{
		err = read(0, &ch, 1);
		if(err == 0 || ch == '\n'){
			break;
		}
		else{
			/*Don't exceed msg buffer space*/
			if(i < 128){
				nickName[i] = ch;
				i++;
			}
		}

	}while(ch != 0);

	strncpy(msg, nickName, 128);
	msg[128] = 1; /*set final bit as nickname indicator*/

	send(sock, msg, 129, 0);


	child = fork();
	if(child < 0){
		sprintf(buf, "%s\n", strerror(errno));
		write(1, buf, strlen(buf));
		exit(-1);
	}
	else if(child == 0){
		/*Child case*/
		/*Read msg from server in loop*/
		while(1){
			err = recv(sock, recMsg, 260, 0);
			if(err < 0){
				exit(-1);
			}
			else{
				sprintf(buf, "%s\n", recMsg);
				write(1, buf, strlen(buf));
			}
		}
	}
	else{
		/*Parent: send new messages from server and send quit command*/
		sprintf(buf, "Enter message or type \"/quit\" to quit\n");
		write(1, buf, strlen(buf));
		while(1){
			bzero(msg, sizeof(char) * 129);
			
			i = 0;
			do{
				err = read(0, &ch, 1);
				if(err == 0 || ch == '\n'){
					msg[i] = '\0';
					break;
				}
				else{
					/*Don't exceed msg buffer space*/
					if(i < 128){
						msg[i] = ch;
						i++;
					}
				}

			}while(ch != 0);
			/*
			err = read(0, inputBuf, 128);
			do{
				err = read(0, &throwaway, 1);
			}while(err != 0 || throwaway != '\n');
			*/
			if(strncmp(msg, "/quit", 5) == 0){
				/*kill child*/
				kill(child, 9);
				msg[128] = 0; 
				err = send(sock, msg, 129, 0);
				exit(0);
			}
			else{

				msg[128] = 2;
				err = send(sock, msg, 129, 0);
			}
		}
		fflush(stdin);
	}
	exit(0);

}