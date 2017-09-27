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
    /*Don't exit here, let program free memory first*/
    cleanup = 1;
  }
 
}

int main(int argc, char *argv[])
{
    int listenfd = 0, conn = 0;
    socklen_t length = 0;
    struct sockaddr_in serv, serv2; 
    char ch = 0;
    int err = 0;

    char *msg;
    int msgSize = 1024;
    int num = 0;

    int pfd[2];
    pid_t child;
    int status = 0;
    int count = 0, i = 0;

    char **args = NULL;
    char response[6000];
    char **parcedMsg = NULL;

    signal(SIGINT, sig_handler);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv, '0', sizeof(serv));
    memset(buf, '0', sizeof(buf)); 

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

    length = sizeof(serv2);    
    while(cleanup == 0)
    {
        numCmds = 0;
        conn = accept(listenfd, (struct sockaddr *)&serv2, &length); 
        if(conn < 0){
            sprintf(buf, "%s\n", strerror(errno));
            write(1, buf, strlen(buf));
            exit(-1);
        }
        printsin(&serv2, "RSTREAM::", "accepted connection from");
        sprintf(buf, "Accepted connection\n");
        write(1, &buf, strlen(buf));

        num = 0;
        count = 0;
        msgSize = 1024;
        msg = calloc(msgSize, 1);

        /*read msg from client*/

        if((num = recv(conn, msg, msgSize, 0)) < 0){
            sprintf(buf, "%s\n", strerror(errno));
            write(1, buf, strlen(buf));
            exit(-1);
        }
        /*parse message*/
        printf("msg: \"%s\"\n", msg);
        parcedMsg = parseInput(msg);
        printf("\"%s\"\n", parcedMsg[0]);
        /*msg parced, fork and execvp*/
        for(i = 0; i < numCmds; i++){
            pipe(pfd);
            if((child = fork()) < 0){
                sprintf(buf, "%s\n", strerror(errno));
                write(1, buf, strlen(buf));
                exit(-1);
            }
            else if(child > 0){
                args = parseArg(parcedMsg[i]);
                printf("arg1 \"%s\"\n", args[1]);
                /*change stdout to conn*/
                dup2(pfd[1], 1);
                close(pfd[0]);
                err = execvp(args[0], args);
                sprintf(buf, "%s\n", strerror(errno));
                write(1, buf, strlen(buf));
                exit(-1);
            }
            else{
                /*Sends output to client*/
                close(pfd[1]);
                while(read(pfd[0], &ch, 1) > 0 && count < 6000){
                    response[count] = ch;
                    count++;
                }
                response[count + 1] = '\0';

                close(pfd[0]);
                wait(&status);
            }
        }
        send(conn, response, strlen(response) + 1, 0);
        close(conn);
        free(msg);
        for(i = 0; i < numCmds; i++){
            free(parcedMsg[i]);
        }
        free(parcedMsg);
     }
     exit(0);
}

/*Seperates commands seperated by ","*/
void* parseInput(char* input){
    char *token = NULL;
    char *tempFilePath, *backupPointer, **ddPointer;
    const char *delim = {","};
    char **cmd = NULL;


    tempFilePath = calloc(1, strlen(input) + 1);
    backupPointer = tempFilePath;
    strncpy(tempFilePath, input, strlen(input) + 1);

    cmd = malloc(1 * sizeof (char*));
    do{
        token = strtok(tempFilePath, delim);
        if(token == NULL){
            break;
        }
        else{
            numCmds++;
            ddPointer = realloc(cmd, numCmds * sizeof(char*));
            if(ddPointer == NULL){
                /*todo*/
            }
            else{
                cmd = ddPointer;
            }
            cmd[numCmds - 1] = malloc(strlen(token) + 1);
            if(cmd[numCmds - 1] == NULL){
                /*todo*/
            }
            else{
                strncpy(cmd[numCmds - 1], token, strlen(token) + 1);
            }

            tempFilePath = NULL;
        }
    }while(token != NULL);

    free(backupPointer);

    return cmd;
}

/*Seperates commands from arguments,also adds a null value to the end of the array*/
void* parseArg(char* input){
    char *token = NULL;
    char *tempFilePath, *backupPointer, **ddPointer;
    const char *delim = {" "};
    char **cmd = NULL;
    int parts = 0;

    tempFilePath = calloc(1, strlen(input) + 1);
    backupPointer = tempFilePath;
    strncpy(tempFilePath, input, strlen(input) + 1);

    cmd = malloc(1 * sizeof (char*));
    do{
        token = strtok(tempFilePath, delim);
        if(token == NULL){
            break;
        }
        else{
            parts++;
            ddPointer = realloc(cmd, parts * sizeof(char*));
            if(ddPointer == NULL){
                /*todo*/
            }
            else{
                cmd = ddPointer;
            }
            cmd[parts - 1] = malloc(strlen(token) + 1);
            if(cmd[parts - 1] == NULL){
                /*todo*/
            }
            else{
                strncpy(cmd[parts - 1], token, strlen(token) + 1);
            }

            tempFilePath = NULL;
        }
    }while(token != NULL);

    /*Add Null to end*/
     ddPointer = realloc(cmd, (parts + 1) * sizeof(char*));
     if(ddPointer == NULL){
            /*todo*/
     }
    else{
         cmd = ddPointer;
     }

     cmd[parts] = malloc(1);
     if(cmd[parts] == NULL){
        free(backupPointer);
        exit(-1);
     }
     cmd[parts] = '\0';
    free(backupPointer);

    return cmd;
}