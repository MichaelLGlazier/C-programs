#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

char error[256];
char buf[256];
int tieFlag = 0; 
int deckIndx = 0;
void createDeck(int* deck);
void shuffleDeck(int* deck);
int debugShuffle(int *deck);
void freePipes(int** pfd, int numPlayers);
int giveCards(int *resultsTable, int *playerMask, int numPlayers, int **pfd, int *deck, int *expectedValue);
int findLowestValue(int *playerMask, int *resultsTable, int mask, int numPlayers);
int recieveInput(int *playerMask, int *resultsTable, int mask, int numPlayers, int **pfd);
void removeTieMask(int *playerMask, int numPlayers);
void closeAllPipesExcept(int n, int numPlayers, int **pfd);
int main(int argc, char* argv[]){
	int deck[52];
	int err = 0;
	int numPlayers = 0;
	int **pfd = NULL;
	int i = 0, n = 0, j = 0;
	int remaining = 0;
	int antiCheat = 0;
	int *playerMask = NULL;
	int *resultsTable = NULL;
	int lowestValue = 0;
	int *expectedValue = NULL;
	int removeTieChild = 0;
	pid_t *child = NULL;

	if(argc < 2){
		exit(-1);
	}
	else{
		numPlayers = atoi(argv[1]);
		if(numPlayers < 1){
			exit(-1);
		}
	}
	/*Create deck*/
	createDeck(deck);

	/*Shuffle Deck*/
	shuffleDeck(deck);

	/*Create pipes for children, and pipes for parent*/
	pfd = malloc(sizeof(int *) * numPlayers * 2);
	if(pfd == NULL){
		exit(-1);
	}
	for(i = 0; i < numPlayers * 2; i++){
		pfd[i] = malloc(sizeof(int *) * 2);
		if(pfd == NULL){
			exit(-1);
		}
		err = pipe(pfd[i]);
		if(err < 0){
			exit(-1);
		}
	}

	/*Create array for pfd pids*/
	child = malloc(sizeof(pid_t) * numPlayers);
	if(child == NULL){
		freePipes(pfd, numPlayers);
		exit(-1);
	}

	expectedValue = malloc(numPlayers * sizeof(int));
	if(expectedValue == NULL){
		freePipes(pfd, numPlayers);
		exit(-1);
	}
	playerMask = calloc(numPlayers, sizeof(int));
	if(expectedValue == NULL){
		freePipes(pfd, numPlayers);
		free(expectedValue);
		exit(-1);
	}
	resultsTable = malloc(numPlayers * sizeof(int));
	if(expectedValue == NULL){
		free(playerMask);
		freePipes(pfd, numPlayers);
		free(expectedValue);
		exit(-1);
	}
	memset(resultsTable, -1, numPlayers * sizeof(int));
	if(expectedValue == NULL){
		free(resultsTable);
		free(playerMask);
		freePipes(pfd, numPlayers);
		free(expectedValue);
		exit(-1);
	}
	remaining = numPlayers;

	/*Create all children*/
	for(i = 0; i < remaining; i++){

		child[i] = fork();
		if(child[i] < 0){
			/*error*/
			free(child);
			freePipes(pfd, numPlayers);
			free(playerMask);
			exit(-1);
		}
		else if(child[i] == 0){
			/*Child case*/
			/*close stdin and replace with pipe*/
			dup2(pfd[i * 2][0], 0);
			close(pfd[i * 2][1]);
			/*close pipe2[1], replace stdout*/
			dup2(pfd[(i * 2) + 1][1], 1);
			close(pfd[(i * 2) + 1][0]);

			closeAllPipesExcept(i, numPlayers, pfd);
			err = execl("./player", "player", (char *)NULL);
			if(err < 0){
				sprintf(error, "%s\n", strerror(errno));
				write(1, error, strlen(error));	
			}
			free(child);
			freePipes(pfd, numPlayers);
			free(playerMask);
			free(expectedValue);
			exit(-1);
		}
		else{
			/*Parent*/
			/*Close parent's pipe ends*/
			close(pfd[(i * 2) + 1][1]);
			close(pfd[(i * 2)][0]);
		}
	}

	do{
		/*Give input to players*/
		giveCards(resultsTable, playerMask, numPlayers, pfd, deck, expectedValue);


		/*Recieve input from players*/
		recieveInput(playerMask, resultsTable, 0, numPlayers, pfd);
		for(i = 0; i < numPlayers; i++){
			/*Cheater Detected*/
			if(resultsTable[i] != expectedValue[i] && playerMask[i] >= 0){
				sprintf(buf, "Booting player %d for cheating\n", i);
				write(1, buf, strlen(buf));

				playerMask[i] = -1;

				remaining--;

				/*Cut child off*/
				close(pfd[i * 2][1]);
				close(pfd[(i * 2) + 1][0]);
			}
		}
		if(remaining == 1){
			break;
		}
		else if(remaining == 0){
			sprintf(buf, "There is no winners\n");
			write(1, buf, strlen(buf));

			free(child);
			free(resultsTable);
			free(playerMask);
			freePipes(pfd, numPlayers);
			free(expectedValue);
			exit(0);
		}

		/*Only one player, wins by default*/
		if(numPlayers == 1){
			free(child);
			free(resultsTable);
			free(playerMask);
			freePipes(pfd, numPlayers);
			free(expectedValue);

			sprintf(buf, "Player 0 has won the game\n");
			write(1, buf, strlen(buf));
			exit(0);
		}
		tieFlag = 0;
		lowestValue = 10000;
		/*Find player to knock out*/
		lowestValue = findLowestValue(playerMask, resultsTable, 0, numPlayers);

		/*Find ties and break*/
		while(tieFlag == 1){
			sprintf(buf, "Dealer breaking tie\n");
			err = write(1, buf, strlen(buf));
			if(err < 0){
				free(child);
				free(resultsTable);
				free(playerMask);
				freePipes(pfd, numPlayers);
				free(expectedValue);
				exit(-1);
			}

			removeTieMask(playerMask, numPlayers);

			/*deal to this player*/
			for(n = 0; n < numPlayers; n++){
				antiCheat = 0;
				/*if player mask equals 0 then player is still in the game*/

				if(playerMask[n] == 0 && resultsTable[n] == lowestValue){
					for(j = 0; j < 4; j++){
						if(deckIndx == 51){
							deckIndx = 0;

							sprintf(buf, "Dealer shuffling new deck\n");
							err = write(1, buf, strlen(buf));
							if(err < 0){
								free(child);
								free(resultsTable);
								free(playerMask);
								freePipes(pfd, numPlayers);
								free(expectedValue);
								exit(-1);
							}
							shuffleDeck(deck);
						}
						antiCheat = deck[deckIndx] + antiCheat;

						sprintf(buf, "Dealing %d to player %d\n", deck[deckIndx], n);
						err = write(1, buf, strlen(buf));
						if(err < 0){
							free(child);
							free(resultsTable);
							free(playerMask);
							freePipes(pfd, numPlayers);
							free(expectedValue);
							exit(-1);
						}

						err = write(pfd[n * 2][1], &deck[deckIndx], sizeof(int));
						if(err < 0){
							free(child);
							free(resultsTable);
							free(playerMask);
							freePipes(pfd, numPlayers);
							free(expectedValue);
							exit(-1);
						}
						deckIndx++;
						playerMask[n] = 1;
						removeTieChild = 1;
					}
				}
				expectedValue[n] = antiCheat;
			}
				
			
			/*Recieve input from players*/
			recieveInput(playerMask, resultsTable, 1, numPlayers, pfd);

			for(i = 0; i < numPlayers; i++){
				/*Cheater Detected*/
				if(resultsTable[i] != expectedValue[i] && playerMask[i] > 0){
					sprintf(buf, "Booting player %d for cheating\n", i);
					write(1, buf, strlen(buf));

					playerMask[i] = -1;

					remaining--;

					/*Cut child off*/
					close(pfd[i * 2][1]);
					close(pfd[(i * 2) + 1][0]);
				}
			}
			lowestValue = findLowestValue(playerMask, resultsTable, 1, numPlayers);
		}
		/*Player may have been booted by cheat detection*/
		if(remaining == 1){
			break;
		}
		else if(remaining == 0){
			sprintf(buf, "There is no winners\n");
			write(1, buf, strlen(buf));

			free(child);
			free(resultsTable);
			free(playerMask);
			freePipes(pfd, numPlayers);
			free(expectedValue);
			exit(0);
		}
		/*Remove lowest value, and adjust playerMask*/
		if(removeTieChild == 1){
			for(i = 0; i < numPlayers; i++){
				if(playerMask[i] == 1){
					if(resultsTable[i] == lowestValue){
						playerMask[i] = -1;

						sprintf(buf, "Player %d has been removed from the game\n", i);
						write(1, buf, strlen(buf));

						remaining--;
						/*Cut child off*/
						close(pfd[i * 2][1]);
						close(pfd[(i * 2) + 1][0]);
					}
				}
			}
		}
		else{
			for(i = 0; i < numPlayers; i++){
				if(playerMask[i] == 0){
					if(resultsTable[i] == lowestValue){
						playerMask[i] = -1;

						sprintf(buf, "Player %d has been removed from the game\n", i);
						write(1, buf, strlen(buf));

						remaining--;
						/*Cut child off*/
						close(pfd[i * 2][1]);
						close(pfd[(i * 2) + 1][0]);
					}
				}
			}
		}
		removeTieChild = 0;
		removeTieMask(playerMask, numPlayers);

	}while(remaining > 1);

	/*Find winner*/
	for(i = 0; i < numPlayers; i++){
		if(playerMask[i] == 0){
			sprintf(buf, "Player %d has won the game\n", i);
			write(1, buf, strlen(buf));

			close(pfd[i * 2][1]);
			close(pfd[(i * 2) + 1][0]);
		}
	}

	/*Close Pipes*/
	/*Redundency check*/
	for(i = 0; i < numPlayers; i++){
		close(pfd[i * 2][0]);
		close(pfd[i * 2][1]);
		close(pfd[(i * 2) + 1][0]);
		close(pfd[(i * 2) + 1][1]);
	}
	free(expectedValue);
	free(child);
	free(resultsTable);
	free(playerMask);
	freePipes(pfd, numPlayers);
	exit(0);
}
void createDeck(int *deck){
	int i = 0;
	int n = 0;
	for(i = 0; i < 52; i = i + 4){
		for(n = 0; n < 4; n++){
			deck[i + n] = (i / 4) + 1; 
		}
	}
}

void freePipes(int** pfd, int numPlayers){
	int i = 0;
	for(i = 0; i < numPlayers * 2; i++){
		free(pfd[i]);
	}
	free(pfd);
}
void shuffleDeck(int *deck){
	int i = 0;
	int r1 = 0, r2 = 0;
	int temp = 0;
	srand(time(0));

	for(i = 0; i < 500; i++){
		r1 = rand() % 52;
		r2 = rand() % 52;

		temp = deck[r1];
		deck[r1] = deck[r2];
		deck[r2] = temp;
	}
}

/*Test case code. Makes sure the shuffle creates
*redicted results.
*/
int debugShuffle(int *deck){
	int i = 0, n = 0;
	int c = 0;
	for(i = 1; i <= 13; i++ ){
		for(n = 0; n < 52; n++){
			if(deck[n] == i){
				c++;
			}
		}
		if(c != 4){
			return -1;
		}
		else{
			c = 0;
		}
	
	}
	return 0;
}

int giveCards(int *resultsTable, int *playerMask, int numPlayers, int **pfd, int *deck, int *expectedValue){
	int i = 0, n = 0;
	int antiCheat = 0;

	for(i = 0; i < numPlayers; i++){
		antiCheat = 0;
		/*if player mask equals 0 then player is still in the game*/
		if(playerMask[i] == 0){
			for(n = 0; n < 4; n++){
				if(deckIndx == 51){
					deckIndx = 0;

					sprintf(buf, "Dealer shuffling new deck\n");
					write(1, buf, strlen(buf));
					shuffleDeck(deck);
				}
				antiCheat = deck[deckIndx] + antiCheat;

				sprintf(buf, "Dealing %d to player %d\n", deck[deckIndx], i);
				write(1, buf, strlen(buf));

				write(pfd[i * 2][1], &deck[deckIndx], sizeof(int));

				deckIndx++;
			}
			expectedValue[i] = antiCheat;
		}
	}
	return 1;
}

int findLowestValue(int *playerMask, int *resultsTable, int mask, int numPlayers){
	int	lowestValue = 10000;
	int i = 0;
	/*Find player to knock out*/
	for(i = 0; i < numPlayers; i++){
		if(playerMask[i] == mask){
			if(resultsTable[i] != -1){
				if(resultsTable[i] < lowestValue){
					lowestValue = resultsTable[i];
					tieFlag = 0;
				}
				else if(resultsTable[i] == lowestValue){
					tieFlag = 1;
				}
			}
		}
	}

	return lowestValue;

}

int recieveInput(int *playerMask, int *resultsTable, int mask, int numPlayers, int **pfd){
	int i = 0;
	for(i = 0; i < numPlayers; i++){
		if(playerMask[i] == mask){
			read(pfd[(i * 2) + 1][0], &resultsTable[i], sizeof(int));
			sprintf(buf, "Dealer recieved value %d from player %d\n", resultsTable[i], i);
			write(1, buf, strlen(buf));
		}
	}
	return 1;
}

void removeTieMask(int *playerMask, int numPlayers){
	int i = 0;
	for(i = 0; i < numPlayers; i++){
		if(playerMask[i] == 1){
			playerMask[i] = 0;
		}
	}
}

void closeAllPipesExcept(int n, int numPlayers, int **pfd){
	int i = 0;
	for(i = 0; i < numPlayers; i++){
		if(i != n){
			close(pfd[i * 2][0]);
			close(pfd[i * 2 ][1]);
			close(pfd[(i * 2) + 1][0]);
			close(pfd[(i * 2) + 1][1]);
		}
	}
}