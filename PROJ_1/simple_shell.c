#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80 /* The maximum length command */
#define READ_END 0
#define WRITE_END 1

int main(void)
{
	char *args[MAX_LINE/2 + 1]; // command line arguments
	char *args_pipe[MAX_LINE/2 + 1]; // arguments for pipe
	int should_run = 1; // flag to determine when to exit program
	char buffer[MAX_LINE]; // sub string to make args
	int num = 0, background = 0, status;
	int pipe_index = 0; // index of |
	pid_t pid;

	while (should_run) {
		printf("osh>");
		fflush(stdout);
		fflush(stdin);

		fgets(buffer, sizeof(buffer), stdin);

		// 28 - 40 : Make args using buffer and strtok
		if (buffer[0] == '\n')
			continue;
		else
			buffer[strlen(buffer) - 1] = '\0';

		args[0] = strtok(buffer, " ");
		if (strcmp(args[0], "quit") == 0)
			break;

		while (args[num] != NULL){
			args[++num] = strtok(NULL, " ");
		}

		if (strcmp(args[num-1], "&") == 0){ // If there are background sign &,
			background = 1;					 // Delete & sign and background = 1
			num--;
			args[num] = NULL;
		}

		for (int i=0; i < num; i++){
			if (strcmp(args[i], ">") == 0 | strcmp(args[i], "<") == 0){
				int fd;
				mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH;
				if (strcmp(args[i], ">") == 0){											// Case 1 : >
					if ((fd = open(args[i+1], O_CREAT | O_WRONLY | O_TRUNC, mode)) == -1){
						perror("OPEN ERROR AT >");
						break;
					}
					dup2(fd, STDOUT_FILENO);
					close(fd);
				}
				else{																	// Case 2 : <
					if ((fd = open(args[i+1], O_RDONLY, mode)) == -1){
						perror("OPEN ERROR AT <");
						break;
					}
					dup2(fd, STDIN_FILENO);
					close(fd);
				}
				args[i] = NULL;
				num -= 2;
				should_run = 0;					// To check the change, stop program. ( > or < )
			}
			else if (strcmp(args[i], "|") == 0){										// Case 3 : | (Pipe)
				pipe_index = i;
				args[i] = NULL;
				int k=0;
				while (k < num - i - 1){		// If command is A | B,	Make A -> args / B -> args_pipe
					args_pipe[k] = args[i + k + 1];
					args[i + k + 1] = NULL;
					k++;
				}
				if (pipe_index != i)
					num++;
				num = num - k - 1;
				args_pipe[k] = NULL;
				should_run = 0;				// To check the change, stop program. ( PIPE)
			}
		}

		pid = fork();

		if (pid < 0){							// Fork error
			perror("FORK FAILED");
			return -1;
		}
		else if (pid == 0){					// CHILD
			if (pipe_index != 0){			// IF PIPE
				int pipe_fd[2];
				pid_t pid_pipe;

				if (pipe(pipe_fd) == -1){
					perror("PIPE FAILED");
					return -1;
				}

				pid_pipe = fork();

				if (pid_pipe < 0){
					perror("PIPE FORK FAILED");
					return -1;
				}
				else if (pid_pipe == 0){		// PIPE_CHILD
					close(pipe_fd[WRITE_END]);
					dup2(pipe_fd[READ_END], STDIN_FILENO);
					close(pipe_fd[READ_END]);
					status = execvp(args_pipe[0], args_pipe);
					if (status == -1){
						perror("FAIL TO EXECUTE THE PIPE COMMAND");
						return -1;
					}
				}
				else {							// PIPE_PARENT
					close(pipe_fd[READ_END]);
					dup2(pipe_fd[WRITE_END], STDOUT_FILENO);
					close(pipe_fd[WRITE_END]);
				}
			}
			status = execvp(args[0], args);
			if (status == -1){
				perror("FAIL TO EXECUTE THE COMMAND");
				return -1;
			}
		}
		else{									// PARENT
			if (background){					// BACKGROUND, NO WAIT
				waitpid(pid, NULL, WNOHANG);
				printf("PID #%d IS WORKING IN BACKGROUND : %s\n", pid, buffer);
			} else{							// FOREGROUND, WAIT CHILD
				wait(&status);
			}
		}

		num = 0;
		background = 0;

		/**
		 * After reading user input, the steps are:
		 * (1) fork a child process using fork()
		 * (2) the child process will invoke execvp()
		 * (3) parent will invoke wait() unless command included &
		 **/
	}

	return 0;
}
