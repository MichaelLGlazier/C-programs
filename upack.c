#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

char buf[512];
int packageIndx;
int err;
const int magicNumber = 918237;
int structSize = 0;
int numFilesInPackage = 0;

int findPackage(char *argv);
int inPackage(int argc, char* argv[]);
int openPackage(char* argv);
int isFileWritable(char* argv);
int resetCursor(int fd);
int unpackFiles(int fd, int argc, char* argv[]);
int unpackFilesAll(int fd);
typedef struct{
	off_t starts_at;
	off_t file_size;
	char file_name[256];
}FileData;

int main(int argc, char* argv[]){
	int fd = 0;
	structSize = (sizeof(off_t) * 2) + 256;
	packageIndx = argc - 1;
	/*Check number of arguments*/
	if(argc == 1){
		sprintf(buf, "Not enough arguments\n");
		write(0, buf, strlen(buf));
		exit(0);
	}	
	/*Unpack all files*/
	else if(argc == 2){
		err = findPackage(argv[1]);
		if(err < 0){
			sprintf(buf, "Package not found\n");
			write(0, buf, strlen(buf));
			exit(0);
		}

		fd = inPackage(argc, argv);
		if(fd < 0){
			sprintf(buf, "Error unpacking during file and directory inspection.\n");
			write(0, buf, strlen(buf));
			exit(0);
		}

		err = unpackFilesAll(fd);
		if(err < 0){
			sprintf(buf, "Error Writing Files\n");
			write(0, buf, strlen(buf));
			exit(0);
		}

	}
	/*Unpack specific files*/
	else{
		err = findPackage(argv[argc - 1]);
		if(err < 0){
			sprintf(buf, "Package not found\n");
			write(0, buf, strlen(buf));
			exit(0);
		}

		fd = inPackage(argc, argv);
		if(fd < 0){
			sprintf(buf, "Error unpacking specific file during file and directory inspection.\n");
			write(0, buf, strlen(buf));
			exit(0);
		}

		err = unpackFiles(fd, argc, argv);
		if(err < 0){
			sprintf(buf, "Error writing files\n");
			write(0, buf, strlen(buf));
			exit(0);
		}

	}
	exit(0);
}	

/*Check if package exist*/
int findPackage(char* argv){
	struct stat statBuf;

	/*Is file accesible*/
	err = access(argv, F_OK | R_OK);
	if(err < 0){
		sprintf(buf, "%s\n", strerror(errno));
		write(0, buf, strlen(buf));
		return -1;
	}

	/*Check that file is regular*/
	err = stat(argv, &statBuf);
	if(err < 0){
		sprintf(buf, "%s\n", strerror(errno));
		write(0, buf, strlen(buf));
		return -1;
	}

	if(!S_ISREG(statBuf.st_mode)){
		sprintf(buf, "Package file is not a regular file\n");
		write(0, buf, strlen(buf));
		return -1;
	}

	return 1;
}
/*Checks that all specific files to be unpacked
*exist within the package.
Returns file descriptor of package or -1 on failure.
*/
int inPackage(int argc, char* argv[]){
	char tempBuf[257] = "";
	int fd;
	int i, j;
	int match = 0;
	off_t cursor;
	/*Check that all files can be written*/
	if(argc == 2){
		fd = openPackage(argv[argc - 1]);
		if(err < 0){
			return -1;
		}

		for(i = 1; i <= numFilesInPackage; i++){
			cursor = lseek(fd, 0, SEEK_CUR); /*Get current position*/
			if(cursor < 0){
				sprintf(buf, "%s\n", strerror(errno));
				write(0, buf, strlen(buf));
				return -1;
			}
			cursor = lseek(fd, -cursor + 8 + (structSize * i) - 256, SEEK_CUR); 
			if(cursor < 0){
				sprintf(buf, "%s\n", strerror(errno));
				write(0, buf, strlen(buf));
				return -1;
			}
			err = read(fd, tempBuf, 256);
			if(err < 0){
				sprintf(buf, "%s\n", strerror(errno));
				write(0, buf, strlen(buf));
				return -1;
			}
			/*Verify file can be opened and written*/
			err = isFileWritable(tempBuf);
			if(err < 0){
				sprintf(buf, "A file can not be unpackaged correctly\n");
				write(0, buf, strlen(buf));
				return -1;
			}
		}

		return fd;
	}
	/*Check that specified files can be written too*/
	else if(argc > 2){
		fd = openPackage(argv[argc - 1]);
		if(err < 0){
			return -1;
		}

		for(j = 1; j < argc - 1; j++){
			match = 0;
			for(i = 1; i <= numFilesInPackage; i++){

				cursor = lseek(fd, 0, SEEK_CUR); /*Get current position*/
				if(cursor < 0){
					sprintf(buf, "%s\n", strerror(errno));
					write(0, buf, strlen(buf));
					return -1;
				}
				cursor = lseek(fd, -cursor + 8 + (structSize * i) - 256, SEEK_CUR); 
				if(cursor < 0){
					sprintf(buf, "%s\n", strerror(errno));
					write(0, buf, strlen(buf));
					return -1;
				}
				err = read(fd, tempBuf, 256);
				if(err < 0){
					sprintf(buf, "%s\n", strerror(errno));
					write(0, buf, strlen(buf));
					return -1;
				}

				if(strncmp(tempBuf, argv[j], 256) == 0){
					match = 1;
					/*Verify file can be opened and written*/
					err = isFileWritable(tempBuf);
					if(err < 0){
						sprintf(buf, "A file can not be unpackaged correctly\n");
						write(0, buf, strlen(buf));
						return -1;
					}
				}
			}
			if(match == 0){
				sprintf(buf, "%s can not be found in package\n", argv[j]);
				write(0, buf, strlen(buf));
				return -1;
			}

		}

		return fd;
	}
	else{
		return -1;
	}
	return -1;

}

/*Open package file.
 Function assumes that package has already been verified as
 openable file.
 Return file descriptor if file is valid, else return -1*/
int openPackage(char* argv){
	int fd;
	int verifyMagic;
	fd = open(argv, O_RDONLY);
	if(fd < 0){
		sprintf(buf, "%s\n", strerror(errno));
		write(0, buf, strlen(buf));
		return -1;
	}
	else{
		/*Check for magic number*/
		err = read(fd, &verifyMagic, 4);
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		if(verifyMagic != magicNumber){
			sprintf(buf, "Package file is not formatted correctly\n");
			write(0, buf, strlen(buf));
			return -1;
		}
		else{

			/*Get number of files in package*/
			err = read(fd, &numFilesInPackage, 4);
	
			if(err < 0){
				sprintf(buf, "%s\n", strerror(errno));
				write(0, buf, strlen(buf));
				return -1;
			}
			return fd;
		}
	}
}

/*Test is file is writable or accesable*/
int isFileWritable(char* argv){
	struct stat statBuf;
	/*Check that file exist*/

	err = stat(argv, &statBuf);
	if(err < 0){
		if(errno == ENOENT){
			/*This error is OK, will just create file later*/
			return 1;
		}
		else if(errno == EACCES){
			/*Need a followup check*/
			/*Possibly a read permission error, only need write*/
		}
		else{
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
	}
	/*Check that file is a regular file.*/
	if(!S_ISREG(statBuf.st_mode)){
		sprintf(buf, "Package file is not a regular file\n");
		write(0, buf, strlen(buf));
		return -1;
	}

	/*Check for write permissions exclusivly*/
	err = access(argv, W_OK);
	if(err < 0){
		if(errno == EACCES){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		else if(errno == ENOENT){
			return 1;
		}
		else{
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
	}
	return 1;
}

/*Unpacks specific files.
 *
 *returns 1 on success and -1 on failure.
 */
int unpackFiles(int fd, int argc, char* argv[]){
	int i = 0; int j = 0; int x = 0;
	int unpackfd;
	int rd = 0;
	char ch = 0;

	FileData file_data;

	err = resetCursor(fd);
	if(err < 0){
		sprintf(buf, "Error reseting cursor to beggining of file\n");
		write(0, buf, strlen(buf));
		return -1;
	}

	for(i = 0; i < numFilesInPackage; i++){
		/*Set cursor to start of struct i*/
		err = lseek(fd, 8 + (sizeof(FileData) * i), SEEK_CUR);
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}

		err = read(fd, &file_data.starts_at, sizeof(off_t));
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		err = read(fd, &file_data.file_size, sizeof(off_t));
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		err = read(fd, &file_data.file_name, 256);
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		/*Search for correct file*/
		for(j = 1; j < argc - 1; j++){
			if(strncmp(file_data.file_name, argv[j], 256) == 0){
				err = resetCursor(fd);
				if(err < 0){
					sprintf(buf, "Error reseting cursor to beggining of file\n");
					write(0, buf, strlen(buf));
					return -1;
				}
				err = lseek(fd, (int)file_data.starts_at, SEEK_SET);
				if(err < 0){
					sprintf(buf, "%s\n", strerror(errno));
					write(0, buf, strlen(buf));
					return -1;
				}

				unpackfd = open(file_data.file_name, O_WRONLY | O_CREAT | O_TRUNC);
				if(unpackfd < 0){
					sprintf(buf, "%s\n", strerror(errno));
					write(0, buf, strlen(buf));
					return -1;
				}

				for(x = 0; x < file_data.file_size; x++){
					rd = read(fd, &ch, 1);
					if(rd < 0){
						sprintf(buf, "%s\n", strerror(errno));
						write(0, buf, strlen(buf));
						return -1;						
					}
					err = write(unpackfd, &ch, 1);
					if(err < 0){
						sprintf(buf, "%s\n", strerror(errno));
						write(0, buf, strlen(buf));
						return -1;						
					}
				}

			}
		}
		err = resetCursor(fd);
		if(err < 0){
			sprintf(buf, "Error reseting cursor to beggining of file\n");
			write(0, buf, strlen(buf));
			return -1;
		}
	}
	return 1;
}
/*Unpacks all files that are in the
 *package.
 *
 */
int unpackFilesAll(int fd){

	int i = 0; int j = 0;
	int unpackfd;
	char ch = 1;
	FileData file_data;

	for(i = 0; i < numFilesInPackage; i++){
		err =lseek(fd, 0, SEEK_SET);
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		err = lseek(fd, 8 + (sizeof(FileData) * i), SEEK_CUR);
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		err = read(fd, &file_data.starts_at, sizeof(off_t));
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		err = read(fd, &file_data.file_size, sizeof(off_t));
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}
		err = read(fd, &file_data.file_name, 256);
		if(err < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;
		}

		err = lseek(fd, (int)file_data.starts_at, SEEK_SET);
		unpackfd = open(file_data.file_name, O_WRONLY | O_CREAT | O_TRUNC);
		if(unpackfd < 0){
			sprintf(buf, "%s\n", strerror(errno));
			write(0, buf, strlen(buf));
			return -1;	
		}
		for(j = 0; j < file_data.file_size; j++){
			err = read(fd, &ch, 1);
			if(err < 0){
				sprintf(buf, "%s\n", strerror(errno));
				write(0, buf, strlen(buf));
			return -1;
		}
			err = write(unpackfd, &ch, 1);
		}

		close(unpackfd);

	}
	return 1;
}

/*Sets file descripter offset to start of file.
 *Only exist becuase it was quicker to fix this 
 *function from its original use 
 *then change every function call to
 *lseek(fd, 0, SEEK_SET)
 */
int resetCursor(int fd){
	int cursor = 0;
	cursor = lseek(fd, 0, SEEK_SET);
	if(cursor < 0){
		sprintf(buf, "%s\n", strerror(errno));
		write(0, buf, strlen(buf));
		return -1;
	}
	return cursor;
}