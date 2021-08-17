#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_BACKGROUND_PROCESSES 64

/* Splits the string by space and returns the array of tokens
*
*/
char **tokenize(char *line, int *size, int *has_and, int *has_more_and)
{
	*has_and = 0;
	*has_more_and = 0;
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  int i, tokenIndex = 0, tokenNo = 0;

  for(i =0; i < strlen(line); i++){

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
      token[tokenIndex] = '\0';
      if (tokenIndex != 0){
	tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
	strcpy(tokens[tokenNo++], token);
	if(strcmp(token, "&&") == 0) *has_and = 1;
	if(strcmp(token, "&&&") == 0) *has_more_and = 1;
	tokenIndex = 0; 
      }
    } else {
      token[tokenIndex++] = readChar;
    }
  }
 
  free(token);
  tokens[tokenNo] = NULL ;
  *size = tokenNo;
  return tokens;
}

void initialize_background_pids(int *background_pids) {
	for(int i = 0; i < MAX_BACKGROUND_PROCESSES; i++) {
		background_pids[i] = 0;
	}
}

void add_background_pid(int *background_pids, int pid) {
	for(int i = 0; i < MAX_BACKGROUND_PROCESSES; i++) {
		if(background_pids[i] == 0) {
			background_pids[i] = pid;
			break;
		}
	}
}

void remove_background_pid(int *background_pids, int pid) {
	for(int i = 0; i < MAX_BACKGROUND_PROCESSES; i++) {
		if(background_pids[i] == pid) {
			background_pids[i] = 0;
			break;
		}
	}
}

void kill_background_pids(int *background_pids) {
	for(int i = 0; i < MAX_BACKGROUND_PROCESSES; i++) {
		if(background_pids[i] != 0) {
			kill(background_pids[i], 9);
		}
	}
}
int sig_int = 0;
void handle_sigint(int sig)
{
    killpg(1, sig);
    sig_int = 1;
}

int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;              
	int i, size;
	int has_and, has_more_and;
	int background_pids[MAX_BACKGROUND_PROCESSES];
	initialize_background_pids(background_pids);
	signal(SIGINT, handle_sigint);

	while(1) {			
		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		printf("$ ");
		scanf("%[^\n]", line);
		getchar();
		sig_int = 0;

		// printf("Command entered: %s (remove this debug output later)\n", line);
		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line, &size, &has_and, &has_more_and);
		int pid;
		while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
    		remove_background_pid(background_pids, pid);
        printf("Shell: Background process finished\n");
    }
    if(strcmp(tokens[0], "exit") == 0) {
    	kill_background_pids(background_pids);
    	for(i=0;tokens[i]!=NULL;i++){
				free(tokens[i]);
			}
			free(tokens);
			exit(0);
    }
    else if(strcmp(tokens[0], "cd") == 0) {
    	if(tokens[2] != NULL) {
    		fprintf(stderr, "Shell: Incorrect command\n");
    	}
    	else if (chdir(tokens[1]) < 0) {
    		fprintf(stderr, "Cannot cd to %s\n", tokens[1]);
    	}
    }
    else {
    	if(strcmp(tokens[size - 1], "&") == 0) {
    		free(tokens[size - 1]);
    		tokens[size - 1] = NULL;
	    	int rc = fork();

				if (rc < 0) {
					fprintf(stderr, "fork failed\n");
				}
				else if (rc == 0) {
					setpgid(getpid(), 2);
					if (execvp(tokens[0], tokens) < 0) {
						fprintf(stderr, "Unable to execute this command\n");
					}
				}
				else {
					add_background_pid(background_pids, rc);
				}
			}
			else if(has_and) {
				char **temp = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
				int j = 1;
				temp[0] = tokens[0];
				for(int i = 1; i <= size; i++) {
					if(sig_int == 1) break;
					if(i==size || strcmp(tokens[i], "&&") == 0) {
						temp[j] = NULL;
						int rc = fork();
						if (rc < 0) {
							fprintf(stderr, "fork failed\n");
						}
						else if (rc == 0) {
							setpgid(getpid(), 1);
							if (execvp(temp[0], temp) < 0) {
								fprintf(stderr, "Unable to execute this command\n");
							}
						}
						else {
							int rc_wait = waitpid(rc, NULL, 0);
						}
						j = 0;
					}
					else {
						temp[j++] = tokens[i];
					}
				}
				free(temp);
			}
			else if(has_more_and) {
				char **temp = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
				int j = 1;
				temp[0] = tokens[0];
				int num_pids = 0;
				int *pids = (int *)malloc(MAX_NUM_TOKENS * sizeof(int));
				for(int i = 1; i <= size; i++) {
					if(i==size || strcmp(tokens[i], "&&&") == 0) {
						setpgid(getpid(), 1);
						temp[j] = NULL;
						int rc = fork();
						if (rc < 0) {
							fprintf(stderr, "fork failed\n");
						}
						else if (rc == 0) {
							if (execvp(temp[0], temp) < 0) {
								fprintf(stderr, "Unable to execute this command\n");
							}
						}
						else {
							pids[num_pids++] = rc;
						}
						j = 0;
					}
					else {
						temp[j++] = tokens[i];
					}
				}
				for(int i = 0; i < num_pids; i++) {
					waitpid(pids[i], NULL, 0);
				}
				free(temp);
			}
			else {
				int rc = fork();
				if (rc < 0) {
					fprintf(stderr, "fork failed\n");
				}
				else if (rc == 0) {
					if (execvp(tokens[0], tokens) < 0) {
						fprintf(stderr, "Unable to execute this command\n");
					}
				}
				else {
					int rc_wait = wait(NULL);
				}
			}
    }
    
		// Freeing the allocated memory	
		for(i=0;tokens[i]!=NULL;i++){
			free(tokens[i]);
		}
		free(tokens);

	}
	return 0;
}
