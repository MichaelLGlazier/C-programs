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
#include <time.h> 
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

int numCmds = 0;
int cleanup = 0;
char buf[1024];
void* parseInput(char* input);
void* parseArg(char* input);

void printsin(s_in, s1, s2)
struct sockaddr_in *s_in; char *s1, *s2;
{
    sprintf(buf, "Program: %s\n%s ", s1, s2);
    write(1, buf, strlen(buf));
    sprintf(buf, "(%d,%d) \n", s_in->sin_addr.s_addr, s_in->sin_port);
    write(1, buf, strlen(buf));
}

void sig_handler(int sig)
{
    if (sig == SIGINT){
        exit(0);
    }
}
int main(int argc, char *argv[])
{
    int listenfd = 0;
    int client_s[5];
    int maxClient = 5;
    int i, maxFD, activity, tempSocket, n = 0;
    char nickName[5][131];
    socklen_t length = 0;
    struct sockaddr_in serv;

    int err = 0;
    int respond[5];

    char tempBuf[128];
    char msg[129];
    char msgQue[5][260]; /*128 byte messages + 131 bytes for nickname. Last byte marks who message is from*/

    fd_set readfds;
    signal(SIGINT, sig_handler);
    memset(&serv, '0', sizeof(serv));
    memset(buf, '0', sizeof(buf)); 
    bzero(respond, sizeof(int) * 5);
    for(i = 0; i < maxClient; i++){
        client_s[i] = 0;
    }

    /*Create listening socket*/
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        sprintf(buf, "Error creating listening socket\n");
        write(1, buf, strlen(buf));
        exit(-1);
    }

    bzero((char *) &serv, sizeof(serv));
    serv.sin_family = (short)AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(0); 

    err = bind(listenfd, (struct sockaddr*)&serv, sizeof(serv)); 
    if(err < 0){
        sprintf(buf, "%s\n", strerror(errno));
        write(1, buf, strlen(buf));
        exit(-1);
    }
    length = sizeof(serv);

    getsockname(listenfd, (struct sockaddr *)&serv, &length); 
    /*print address and port number*/
    sprintf(buf, "%d\n", ntohs(serv.sin_port));
    write(1, buf, strlen(buf));

    err = listen(listenfd, 1); /*Listen for 1 connection at a time*/
    if(err < 0){
        sprintf(buf, "%s\n", strerror(errno));
        write(1, buf, strlen(buf));
        exit(-1);
    }

    /*Accept and monitor connections*/
    while(1){
        /*Clean message buffer so that no old data gets sent*/
        for(i = 0; i < 5; i++){
            memset(msgQue[i], 0, sizeof(msgQue[i]));
        }
        bzero(respond, sizeof(int) * 5);

        /*Clear socket sets*/
        FD_ZERO(&readfds);

        FD_SET(listenfd, &readfds);
        maxFD = listenfd;
        /*Add active clients to the read set*/
        for(i = 0; i < maxClient; i++){
            if(client_s[i] > 0){
                FD_SET(client_s[i], &readfds);
                if(client_s[i] > maxFD){
                    maxFD = client_s[i];
                }
            }
        }
        /*Waits for activity on a socket. No timeout needed*/
        activity = select(maxFD + 1, &readfds, NULL, NULL, NULL);
        if(activity < 0){
            sprintf(buf, "%s\n", strerror(errno));
            write(1, buf, strlen(buf));
            exit(-1);
        }

        /*Check listener for incoming connections*/
        if(FD_ISSET(listenfd, &readfds)){
            if((tempSocket = accept(listenfd, (struct sockaddr *)&serv, &length)) < 0){
               sprintf(buf, "%s\n", strerror(errno));
               write(1, buf, strlen(buf));
               exit(-1);
            }
            else{
                /*new connection made*/
                /*Find open client spot*/
                for(i = 0; i < maxClient; i++){
                    if(client_s[i] == 0){
                        client_s[i] = tempSocket;
                        break;
                    }
                }
            }
        }

        /*Read input from other socket*/
        for(i = 0; i < maxClient; i++){
            if(FD_ISSET(client_s[i], &readfds)){
                bzero(msg, sizeof(char) * 129);
                err = recv(client_s[i], msg, 129, 0);
                switch(msg[128]){
                    case 0:
                        /*client exiting*/
                        close(client_s[i]);
                        client_s[i] = 0;
                        break;
                    case 1:
                        /*Connection Message*/
                        for(n = 0; n < 128; n++){
                            if(msg[n] == '\n' || msg[n] < 48 || msg[n] > 122){
                                msg[n] = '\0';
                                break;
                            }
                        }
                        sprintf(buf, "SERVER: creating nickname %s\n for user %d\n", msg, i);
                        write(1, buf, strlen(buf));

                        sprintf(nickName[i], "<%s> ", msg);

                        break;
                    case 2:
                        /*Message*/
                        strncpy(tempBuf, msg, 128);
                        tempBuf[127] = '\0';
                        respond[i] = 1;
                        sprintf(msgQue[i], "%s%s", nickName[i], tempBuf);
                        printf("Message recieved: %s\n", msg);
                        /*Add message to message que*/
                        break;
                }

            }
        }

        /*Write output to clients*/
        for(i = 0; i < maxClient; i++){
            if(client_s[i] != 0){
                 for(n = 0; n < maxClient; n++){
                    /*Don't send own message back to client*/
                    if(n != i && respond[i] == 1 && client_s[n] != 0){
                        /*Send first 128 bytes of message*/
                        sprintf(buf, "Sending \"%s\" to %d\n", msgQue[i], n);
                        write(1, buf, strlen(buf));
                        send(client_s[n], msgQue[i], 260, 0);
                    }
                }
            }
        }
    }


    exit(0);
}