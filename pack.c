/*Michael Glazier
  File Packing Program
*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int checkValid(int argc, char* argv[]);
int max(int a, int b);
void writeError(int err);
int min(int a, int b);
int dataStructs(int fd, char* argv, int start_at);
int packageFile(int fd, char* argv);

char error[512];
int main(int argc, char* argv[]){
	int err = 0;
	int fd;
	int magicNumber = 918237;
	int numFiles = argc - 2; /*argc minus first and last arguments*/
	int i = 0;
	int start_at = 8;
	/*check for packages to pack.*/
	if(argc == 1){
		exit(0);
	}
	err = checkValid(argc - 1, argv);
	if(err == -1){
		exit(0);
	}
	strncpy(error, "", 1); /*Initialize memory of error*/
	/*Create package*/
	fd = open(argv[argc - 1], O_CREAT | O_TRUNC | O_RDWR);
	if(fd < 0){
		sprintf(error, "Package file: %s\n", strerror(errno));
		write(0, error, strlen(error));
		exit(0);
	}
	err = write(fd, &magicNumber, 4);
	writeError(err);

	err = write(fd, &numFiles, 4);
	writeError(err);

	start_at = start_at + (((sizeof(off_t) * 2) + 256) * numFiles);
	/*Write basic data into package file for typedef struct*/
	for(i = 0; i < numFiles; i++){
		err = dataStructs(fd, argv[i + 1], start_at);
		if(err < 0){
			exit(0);
		}
		start_at = start_at + err;
	}

	/*Write files content to package*/
	for(i = 0; i < numFiles; i++){
		err = packageFile(fd, argv[i + 1]);
		if(err < 0){
			exit(0);
		}
	}

	exit(0);
}
/*Checks whether about to be packed are valid files for reading.
 *Returns 1 on success and -1 on failure.
*/
int checkValid(int argc, char* argv[]){
	struct stat statBuf;
	int i = 1;
	int j = 1;
	int err = 0;
	for(i = 1; i < argc ; i++){
		/*Check that file exist and has correct permissions*/
		err = access(argv[i], F_OK | R_OK);
		if(err < 0){
			sprintf(error, "arg %d: %s\n", i, strerror(errno));
			write(0, error, strlen(error));
			return -1;
		}
		
		/*Check that file is regular file*/
		err = stat(argv[i], &statBuf);
		if(err < 0){
			sprintf(error, "arg %d: %s\n", i, strerror(errno));
			write(0, error, strlen(error));
			return -1;
		}
		if(!S_ISREG(statBuf.st_mode)){
			sprintf(error, "arg %d is not a regular file\n", i);
			write(0, error, strlen(error));
			return -1;
		}

		/*Check for duplicates*/
		for(j = 1; j < argc + 1; j++){
			if(j != i){
				if(strncmp(argv[i], argv[j], 
					max(strlen(argv[i]), strlen(argv[j]))) == 0){
						sprintf(error, "Duplicate files in parameters\n");
						write(0, error, strlen(error));
					return -1;
				}
			}
		}
	}
	return 1;

}

int max(int a, int b){
	if(a > b){
		return a;
	}
	else{
		return b;
	}
}
int min(int a, int b){
	if(a < b){
		return a;
	}
	else{
		return b;
	}
}

void writeError(int err){
	if(err < 0){
		sprintf(error, "Write error: %s\n", strerror(errno));
			write(0, error, strlen(error));
			exit(0);
	}
	else{
		return;
	}
}

/*Writes struct of files into the package.
 *This function makes sure everything is formatted
 *correctly. Returns 1 on success and -1 on failure.
 */
int dataStructs(int fd, char* argv, int start_at){
	struct stat statBuf;
	int err;
	off_t fileSize = 0;
	char fileName[256] ="";
	char *token;
	char* tempFilePath;
 	char* pointerStart;
 	const char *delim = "/";
	/*Malloc memory for duplicate array*/
	tempFilePath = calloc(1 , strlen(argv) + 1);
	if(tempFilePath == NULL){
		return -1;
	}
	strncpy(tempFilePath, argv, strlen(argv)); /*Duplicate string*/
	pointerStart = tempFilePath; /*Create pointer to start of mallocd memory*/
	err = stat(argv, &statBuf);
	if(err < 0){
		sprintf(error, "%s: %s\n", argv, strerror(errno));
		write(0, error, strlen(error));
		free(tempFilePath);
		return -1;
	}
	do{
		token = strtok(tempFilePath, delim);
		if(token == NULL){
			break;
		}
		else{
			strncpy(fileName, token, min(strlen(token), 256));
			tempFilePath = NULL;
		}
	}while(token != NULL);

	free(pointerStart);
	fileSize = statBuf.st_size;

	err = write(fd, &start_at, (int)sizeof(off_t));
	writeError(err);

	err = write(fd, &fileSize, (int)sizeof(off_t));
	writeError(err);

	err = write(fd, fileName, 256);
	return fileSize;	
}

/*Writes the content of the file into the 
 *package.
 *Returns 1 on success and -1 on failure.
 */
int packageFile(int fd, char* argv){
	int err;
	int readfd;
	ssize_t readSize;
	char ch;
	readfd = open(argv, O_RDONLY);
	if(readfd < 0){
		sprintf(error, "%s: %s\n", argv, strerror(errno));
		write(0, error, strlen(error));
		return -1;
	}

	/*Read a byte from a file and write it to the package*/
	do{
		readSize = read(readfd, &ch, 1);
		if(readSize < 0){
			sprintf(error, "%s Read Error: %s\n", argv, strerror(errno));
			write(0, error, strlen(error));
			return -1;
		}
		if(readSize == 0){
			break;
		}
		err = write(fd, &ch, 1);
		if(err < 0){
			sprintf(error, "%s Write Error: %s\n", argv, strerror(errno));
			write(0, error, strlen(error));
			return -1;
		}
	}while(readSize != 0);
	return 1;
}