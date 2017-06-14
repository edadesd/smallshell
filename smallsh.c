 /* Filename: smallsh.c
 * Created: 5/20/2016
 * Last updated: 5/23/2016
 * Description: smallsh is a shell program that
 * executes commands on a UNIX system. It prompts the
 * user to enter a command and arguments, then executes
 * the command either as a built-in command (status, cd,
 * and exit) or by following the PATH variable.
 *
 * smallsh runs a process in the foreground by default,
 * or in the background if the command line ends with the "&" operator.
 *
 * smallsh supports comment lines, which begin with a #. If the
 * line is a comment line, smallsh does not carry out any instructions
 * and instead returns control to the user for another line.
 *
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "smallshlib.h"

const int MAX_ARGUMENTS = 512;
const int MAX_COMMAND_LENGTH = 2048;
const int MAX_FORKS = 100;
const int SIGNAL_KILLED = 500;

const char *COMMENT = "#";

int main () {


	/*
	 * Signal handling. The default
	 * is restored for forked foreground
	 * processes.
	 */

	struct sigaction handling;

	sigemptyset(&(handling.sa_mask));
	sigaddset(&(handling.sa_mask), SIGINT);
	handling.sa_handler = SIG_IGN;
	sigaction(SIGINT, &handling, NULL);

	/*
	 * Track exit status
	 * and the signal number
	 * of the signal that
	 * killed the last foreground
	 * process.
	 */
	static int status = 0;
	static int signalNum = 0;


	static char *previousDirectory;
	char *currentDirectory;

	/* Extracting command from user input*/

	char commandInputBuffer[MAX_COMMAND_LENGTH];
	char command[MAX_COMMAND_LENGTH];
	int commandIsExit = 0;
	int isBlankOrComment = 0;

	/* Extracting arguments from user input*/

	char *userArgs[MAX_ARGUMENTS];
	char *execvLine[MAX_ARGUMENTS + 1];
	char *lastToken;
	int numArgs = 0;

	char spaceCheck;

	int i;					//Loop/array index


	char *pathStep;			//Captures a partial directory name

	int inputFile = 0;		//fd for input redirection
	int outputFile = 0;		//fd for output redirection
	int inputDup = 0;		//Duplicates of fd
	int outputDup = 0;

	/*
	 * Save stdout and stdin to be restored
	 * in case of a bad command.
	 */

	int savedIn = STDIN_FILENO;
	int savedOut = STDOUT_FILENO;

	/*
	 * These flags track whether the user
	 * entered an operator. They will be
	 * used to properly assign input
	 * and output, and control whether
	 * the shell should immediately wait
	 * on the forked process.
	 */
	int backgroundProcessFlag = 0;
	int redirectsInput = 0;
	int redirectsOutput = 0;

	/*
	 * These flags tell the shell
	 * that the next argument is a
	 * filename for redirection
	 */
	int getInput = 0;
	int getOutput = 0;
	char *inputTarget;
	char *outputTarget;

	/*
	 * Tracks whether the
	 * argument is an argument
	 * to send to the forked process. An operator,
	 * filename, or space (if erroneously
	 * taken as an argument) will clear this.
	 */
	int isArg = 1;

	/*
	 * Tracks whether the command
	 * is built-in or external. If
	 * command matches a built-in
	 * command, the flag will be cleared
	 * and the external command block
	 * will be skipped.
	 */
	int externalCommand = 1;

	/*
	 * PID and number of process
	 * tracking, used to ensure
	 * the shell waits on each
	 * background process (WNOHANG)
	 * during each pass through the
	 * loop.
	 */

	pid_t backgroundProcesses[MAX_FORKS];
	int numBGProcesses= 0;
	pid_t foregroundPID;
	pid_t forkedPID;
	int childStatus;
	int waitResult;
	int processesForked = 0; //Fork bomb prevention


	/*
	 * Entering exit will actually call smallsh_exit
	 * which itself will call exit for
	 * the shell process, but this
 	 * is a nice-sounding statement of the
	 * "true" exit condition.
	 */

	while(!commandIsExit) {


		/*
		 * Fork bomb prevention. Bail
		 * if the number of forks exceeds
		 * MAX_FORKS.
		 */
		if (processesForked >= MAX_FORKS) {

			printf("Too many forked processes, exiting shell.\n");
			fflush(stdout);
			exit(1);

		}

		/*
		 * Wait on each background process
		 * using NOHANG before giving control to the
		 * user.
		 */

		for (i = 0; i < numBGProcesses; i++) {

			waitResult = waitpid(backgroundProcesses[i], &childStatus, WNOHANG);

			//waitpid returns 0 if no children have changed status

			if(waitResult) {

				if(WIFEXITED(childStatus)) {

					status = WEXITSTATUS(childStatus);
					printf("Background PID %ld is done: exit value %d\n", (long)backgroundProcesses[i], childStatus);
					fflush(stdout);
					/*
					 * Remove process from array when it terminates
					 */
					backgroundProcesses[i] = backgroundProcesses[numBGProcesses];
					numBGProcesses--;
					processesForked--;
				}

				else if(WIFSIGNALED(childStatus)) {

					status = SIGNAL_KILLED;
					signalNum = WTERMSIG(childStatus);
					printf("Background PID %ld is done: terminated by signal %d\n", (long)backgroundProcesses[i], signalNum);
					fflush(stdout);

					backgroundProcesses[i] = backgroundProcesses[numBGProcesses];
					numBGProcesses--;
					processesForked--;
				}
			}
		}


		/*
		 * Before giving control to the user,
		 * clear out the previous command contents
		 */
		for (i = 0; i < numArgs; i++) {

			strcpy(userArgs[i], "");
		}
		numArgs = 0;
		i = 0;
		isBlankOrComment = 1;
		externalCommand = 1;

		/*
		 * Clear the operator flags. They will
		 * be set if the user requests a background process in the
		 * command line.
		 */

		backgroundProcessFlag = 0;
		redirectsInput = 0;
		redirectsOutput = 0;
		getInput = 0;
		getOutput = 0;
		inputFile = 0;
		outputFile = 0;
		waitResult = 0;

		memset(commandInputBuffer, 0, sizeof(commandInputBuffer));

		/*
		 * Print a colon as the prompt to the user to enter
		 * a command. Flush stdout and stdin to be safe.
		 */
		printf(":");
		fflush(stdout);
		fflush(stdin);
		fgets(commandInputBuffer, MAX_COMMAND_LENGTH, stdin);



		/*
		 * Only parse the input if it is neither blank
		 * nor a comment line (i.e. starts with #.)
		 *
		 *
		 * If the length is greater than
		 * just a newline,
		 * check for whether it contains
		 * nothing but whitespace.
		 * If so, it is blank,
		 * set the flag so parsing is skipped.
		 *
		 */

		if(strlen(commandInputBuffer) > 1) {


			for (i = 0; i < strlen(commandInputBuffer); i++) {

				if(!isspace(commandInputBuffer[i])) {

					/*
					 * This will clear even if
					 * the first character is a #,
					 * but the next block will
					 * re-set the flag.
					 */
					isBlankOrComment = 0;

				}
			}

			/*
			 * If the line starts with a #, it is
			 * a comment line, so set the flag
			 * to skip parsing.
			 */

			strcpy(command, strtok(commandInputBuffer, " "));
			if(!strncmp(command, COMMENT, 1)) {
				isBlankOrComment = 1;
			}

		}

		/*
		 * If the line has length <= 1, it is
		 * blank, so set the flag to skip
		 * parsing.
		 */

		else {
			isBlankOrComment = 1;
		}


		/*
		 * Blank lines and comment lines are ignored.
		 * Only proceed with parsing the line if it
		 * is neither.
		 */


		if(!isBlankOrComment) {

			/*
			 * Check for any more tokens. If there are none, the
			 * user has supplied no arguments.
			 */

			lastToken = strtok(NULL, " ");

			/* Really the first token, but also the last token if null*/
			if(lastToken == NULL) {

				numArgs = 0;

			}

			/*
			 * If there are any more tokens,
			 * copy the first to the
			 * array of arguments, then keep adding
			 * arguments until there are no more
			 * tokens.
			 */

			else {

				i = 0;


				while(lastToken) {

					/*
					 * Set the isArg flag, cleared
					 * if the item is an operator
					 * or filename
					 */

					isArg = 1;

					/*
					 * If the first character
					 * of the command is whitespace,
					 * discharge it by clearing isArg.
					 * When isArg is cleared, tokens
					 * will not be added to the list
					 * of arguments.
					 */

					spaceCheck = lastToken[0];
					if(isspace(spaceCheck)) {

						isArg = 0;

					}

					/*
					 * If the capture flags were set
					 * on a previous loop, get the filename
					 * and clear the flag.
					 */

					if(getInput) {

						inputTarget = lastToken;

						getInput = 0;
						isArg = 0;

					}

					if(getOutput) {

						outputTarget = lastToken;

						getOutput = 0;
						isArg = 0;
					}


					/*
					 * Determine whether the token is
					 * an argument or an operator. If
					 * it matches an operator, set the
					 * flag for what it redirects, and
					 * set the flag for the next token
					 * to be captured as the filename.
					 */


					fflush(stdout);
					if(!strncmp(lastToken, "<", 1)) {

						/*
						 * If the token matches
						 * the symbol but the
						 * getInput flag is already
						 * set, treat the operator
						 * as the filename,
						 * e.g. ~/"<"
						 *
						 * Handle this by letting the
						 * getInput flag block
						 * pick it up.
						 */
						if (!getInput) {
							redirectsInput = 1;
							getInput = 1;
						}

						isArg = 0;
					}


					else if(!strncmp(lastToken, ">", 1)) {


						if(!getOutput) {
							redirectsOutput = 1;
							getOutput = 1;
						}

						isArg = 0;
					}


					/*
					 * If no operator modifies
					 * the command, add it to the
					 * array of arguments and
					 * increment the number of arguments.
					 */

					if (isArg) {

							userArgs[i] = lastToken;
							i++;
							numArgs++;
					}


					/*
					 * Take another token from the string.
					 */

					lastToken = strtok(NULL, " ");
				}
			}

			/*
			 * Input or output redirection were
			 * specified, strip any newline
			 * characters from the filenames.
			 *
			 * Done here to avoid mixing strtok
			 * calls with the user input parse.
			 */

			if(redirectsInput) {

				strcpy(inputTarget, strtok(inputTarget, "\n"));

			}

			if(redirectsOutput) {

				strcpy(outputTarget, strtok(outputTarget, "\n"));
			}

			/*
			* Check the last "arg" for whether
			* it was the & operator. If it was,
			* remove it from the list of operators
			* and instead set the background flag.
			*/

			if (numArgs > 0) {

				/*
				 * Also strip the newline from the last
				 * argument. This causes a lot of
				 * trouble with other commands, so
				 * stripping it here will help.
				 */

				strcpy(userArgs[numArgs - 1], strtok(userArgs[numArgs - 1], "\n"));

				if(!strncmp(userArgs[numArgs - 1], "&", 1)) {

					backgroundProcessFlag = 1;
					userArgs[numArgs] = "";
					numArgs--;
				}
			}


			/*
			 * Test the command against each of the built-in commands.
			 *
			 *
			 * If the command is equal to exit, then
			 * begin the process of exiting the shell.
			 *
			 * Command has a newline, so use strtok to
			 * strip it before comparing to the built-in
			 * commands.
			 */


			if(!strcmp(strtok(command, "\n"), "exit")) {
				commandIsExit = 1;
				externalCommand = 0;

				/*
				 * Kill all processes started by
				 * the shell
				 */

				smallsh_exit(numBGProcesses, backgroundProcesses);
			}

			/*
			 * If the command is status, just print the status.
			 * The blocks that terminate processes will update
			 * the static variable status as appropriate.
			 */

			else if(!strcmp(strtok(command, "\n"), "status")) {


				status = smallsh_status(status, signalNum);
				externalCommand = 0;
				/*
				 * Successfully executing status means
				 * the last command was successfully
				 * executed. This is the same behavior
				 * as echo $? in bash.
				 */
				status = 0;
			}

			/*
			 * If the command is cd, it goes to Home with no argument
			 * or to the directory specified in the first argument.
			 *
			 * smallsh_cd returns a status, so the shell's const int
			 * will be updated with it.
			 */

			else if(!strcmp(strtok(command, "\n"), "cd")) {

				status = smallsh_cd(numArgs, userArgs);
				externalCommand = 0;

			}


			/*
		 	 * If the command is not listed here, attempt to start it as a process
		 	 * by forking this and passing the process to execvp.
		 	 * The shell will reach this point if none of the
		 	 * built-in commands clear the externalCommand flag.
		 	 *
		 	 *
		 	 * Build the execvp arguments by building an array
		 	 * that starts with a pointer to the command,
		 	 * then contains a pointer to each argument held
		 	 * in userArgs.
		 	 *
		 	 */

			if(externalCommand) {

				execvLine[0] = strdup(command);

				/*
				 * numArgs was collected above during
				 * argument parsing, and disregarded
				 * operators and filenames (i.e. tokens
				 * that came after an operator.)
				 */
				for (i = 0; i < numArgs; i++) {

					execvLine[i+1] = strdup(userArgs[i]);
				}
				/*
				 * Finish the array with NULL to indicate no more commands.
				 * The array was initialized with MAX_ARGS + 1
				 * positionsto ensure there would be room for this.
				 */
				execvLine[numArgs+1] = NULL;


				/*
				 * Open file descriptors for input and output,
				 * if redirection is specified.
				 *
				 * If the files are not available, print the
				 * error. Also set the status to 1 for failed.
				 */

				if(redirectsInput) {

					inputFile = open(inputTarget, O_RDONLY);
					if (inputFile == -1) {

						inputFile = close(inputFile);

						perror("open");
						fflush(stdout);
						status = 1;
						continue;
					}
				}

				if(redirectsOutput) {

					outputFile = open(outputTarget, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
					if (outputFile == -1) {
						perror("open");
						fflush(stdout);
						status = 1;
						continue;
					}
				}

				/*
				 * Fork only if any specified input or
				 * output files are valid. If not,
				 * skip the fork and everything after.
				 *
				 * The loop will complete and return control
				 * to the user, after the cleanup routine.
				 */

				if(inputFile != -1 && outputFile != -1) {


					forkedPID = fork();
					processesForked++;

					if(forkedPID == -1) {

						perror("fork");
						fflush(stdout);
						status = 1;

					}

					/*
					 * Child process I/O redirection
					 */

					/*
					 * fork() returns 0 in the
					 * child process, so if
					 * forkedPID == 0, the child
					 * process will carry out
					 * this block but not the
					 * parent.
					 */

					if(forkedPID == 0) {

						if(redirectsInput) {

							/*
							 * dup2 to replace stdin with
							 * the input file
							 */

							fflush(stdout);
							inputDup = dup2(inputFile, 0);

							/*
							 * If the dup2 fails,
							 * print the error
							 * and kill the forked process.
							 */
							if(inputDup == -1) {

								perror("dup2");
								fflush(stdout);
								kill(forkedPID, 2);
								status = SIGNAL_KILLED;
								signalNum = 2;
							}
						}

						/*
						 * Same if the dup2 fails
						 * for the output.
						 */

						if(redirectsOutput) {

							outputDup = dup2(outputFile, 1);
							if(outputDup == -1) {

								perror("dup2");
								fflush(stdout);
								kill(forkedPID, 2);
								status = SIGNAL_KILLED;
								signalNum = 2;

							}
						}

						/*
						 * If the backgroundProcessFlag was
						 * not set, this child will run in the
						 * foreground. Register it to handle
						 * SIGINT with the default action,
						 * undoing the SIG_IGN flag set
						 * at the top of the parent.
						 */

						if(!backgroundProcessFlag) {

						handling.sa_handler = SIG_DFL;
						sigaction(2, &handling, NULL);
						}

						/*
						 * If the process is a background
						 * process, its signal handling is unchanged
						 * from the parent, i.e. it ignores
						 * SIGINT.
						 *
						 * Redirect its input and
						 * output to /dev/null unless
						 * an input file was specified.
						 */

						if(backgroundProcessFlag) {

							/*
							 * If the flag is set, the
							 * input was already sent to its proper
							 * destination so skip this redirection.
							 */
							if(!redirectsInput) {

								inputFile = open("/dev/null", O_RDONLY);

								if (inputFile == -1) {
									perror("open");
									fflush(stdout);
									status = 1;
								}

								/*
								 * If opening /dev/null was successful,
								 * complete the redirection.
								 */

								else {

									inputDup = dup2(inputFile, 0);
									if(inputDup == -1) {

										perror("dup2");
										fflush(stdout);
										status = 1;
									}
								}
						}


							/*
							 * Same process for output:
							 * Leave any redirection already
							 * handled in place. Otherwise,
							 * attempt to open /dev/null and
							 * redirect.
							 */

							if(!redirectsOutput) {

								outputFile = open("/dev/null", O_RDONLY);

								if(outputFile == -1) {

									perror("open");
									status = 1;
								}

								else {

									outputDup = dup2(outputFile, 1);
									if(outputDup == -1) {

										perror("dup2");
										fflush(stdout);
										status = 1;
									}
								}
							}
						}

						/*
						 * If status is 0 at this point,
						 * everything should be ready to hand
						 * off to execvp
						 *
						 */

						execvp(execvLine[0], execvLine);

						/*
						 * Ensure execvp occurred.
						 * If line is reached, the
						 * execvp did not replace
						 * the fork of the shell.
						 */

						perror("");
						status = 1;
						/*
						 * Close the input and
						 * output files.
						 */
						close(inputFile);
						close(outputFile);
						close(inputDup);
						close(outputDup);
						exit(1);
					}


					/*
					* If launched as a background process,
					* add the child's PID to the
					* array of background PIDs.
					*
					* The shell will wait for background
					* processes before asking for input
					* each loop.
					*/


					if(backgroundProcessFlag) {

						backgroundProcesses[numBGProcesses] = forkedPID;

					}

					/*
					 * If launched as a foreground process,
					 * the shell will wait for the child.
					 */


					if(!backgroundProcessFlag) {

						waitResult = waitpid(forkedPID, &childStatus, 0);

						if(waitResult == -1) {

							perror("wait failed");
							fflush(stdout);
						}


						/*
						 * If the foreground child was killed by a
						 * signal record that signal in signalNum
						 * and set status to the SIGNAL_KILLED
						 * sentinel so smallsh_status will know it
						 * was terminated via signal.
						 */

						if(WIFSIGNALED(childStatus)) {
							signalNum = WTERMSIG(childStatus);
							status = SIGNAL_KILLED;
							printf("Terminated by signal %d\n", signalNum);
							processesForked--;

						}

						/*
						 * If the child reached an exit, record the
						 * exit status for smallsh_status.
						 */
						else {

							status = (WEXITSTATUS(childStatus));
							processesForked--;
						}

					}
					if(backgroundProcessFlag) {

						printf("Background PID is %ld\n", (long)forkedPID);
						fflush(stdout);
						backgroundProcesses[numBGProcesses] = forkedPID;
						numBGProcesses++;

					}
				}
			}//Skips to here if not external command.
		} //Skips to here if blank or comment.
	}
	/*
	 * Return to top of loop where flags will be reset.
	 */
}
