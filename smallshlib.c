#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include "smallshlib.h"


const int KILLED_BY_SIGNAL = 500;
const int MAX_PATH_LENGTH = 128;

/*
 * Assign a different value to use
 * another signal.
 */
const int EXIT_SIGNAL = SIGTERM;


int smallsh_status (int status, int signalNum) {

		if(status == KILLED_BY_SIGNAL) {

			printf("Terminated by signal %d\n", signalNum);
			fflush(stdout);

		}
		else {

			printf("%d\n", status);
			fflush(stdout);
		}

	return 0;
}


int smallsh_cd (int numArgs, char *userArgs[]) {

	char *pathStep;
	char *argTest;
	char *previousDirectory;
	int status = 0;

			/*
			 * If no argument given, cd moves working
			 * directory to HOME.
			 */
			getcwd(previousDirectory, MAX_PATH_LENGTH);

			if(!numArgs) {

				pathStep = getenv("HOME");
				status = chdir(pathStep);

			}


			/*
			 * If an argument is given, determine whether
			 * it is a shortcut or a path that can be followed.
			 */

			else if(numArgs == 1) {

				/*If the length of the argument is greater than 1, then
				* it is a path to attempt to follow.
				*
				* Tokenize the string with /, then change the directory
				* to each token as it is reached.
				*
				* If the first character of the string is /, the path is
				* absolute so start in root.
				*
				*/

				if(!strncmp(userArgs[0], "/", 1)) {

					chdir("/");

					/* Strip the leading slash by advancing
					 * in the array, since the directory
					 * has already been changed to root */
					userArgs[0]++;

					pathStep = strtok(userArgs[0], "/");

					while(pathStep != NULL) {

						status = chdir(pathStep);

						/*
						 * If the chdir fails, report
						 * the error and undo the attempt.
						 */

						if(status != 0) {
							perror(pathStep);
							pathStep = NULL;
							/* Reverse the partial directory change */
							chdir(previousDirectory);
							/* Standardize error exit code as 1 */
							status = 1;
							}

						else {
							pathStep = strtok(NULL, "/");
						}

					}
				}

				/*
				* If the first character is not /, the path
				* is relative to the current working directory.
				*
				* "." and ".." already work with chdir
				*/

				else {

					getcwd(previousDirectory, MAX_PATH_LENGTH);

					pathStep = strtok(userArgs[0], "/");

					while(pathStep != NULL) {

						status = chdir(pathStep);
						if(status != 0) {

							perror(pathStep);
							chdir(previousDirectory);
							pathStep = NULL;
							status = 1;
						}

						else {
							pathStep = strtok(NULL, "/");
						}
					}
				}


				if(numArgs > 1) {

					printf("Usage: cd [absolute or relative path\n");
					fflush(stdout);
					status = 1;

				}
			}

	/* Report status to the shell */

	return status;
}

void smallsh_exit (int numProcesses, pid_t background[]) {

	int i;

	/*
	 * Kill all processes, then
	 * the shell will exit.
	 */

	for(i = 0; i < numProcesses; i++) {

		printf("Killing %ld\n", (long)background[i]);
		kill(background[i], SIGTERM);

		/* Casting a pid_t as a long works according to
		 *http://stackoverflow.com/questions/20533606/what-is-the-correct-printf-specifier-for-printing-pid-t
		 */
		printf("%ld terminated by signal %d\n", (long)(background[i]), SIGTERM);
		fflush(stdout);

	}
	exit(0);
}



