#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char* argv[]){
	int err = 0;
	int num = 0;
	int total = 0;
	int i = 0;
	do{
		total = 0;

		for(i = 0; i < 4; i++){
			err = read(0, &num, 4);
			if(err == 0){
				/*EOF*/
				break;
			}
			total = total + num;
		}
		
		write(1, &total, sizeof(int));
	}while(err != 0);

	exit(1);
}