/*
 * Library for the built-in
 * functions of smallsh.c
 */


/*
 * Kills all background processes
 * before exiting.
 */
void smallsh_exit (int numProcesses, pid_t background[]);


/*
 * Prints the exit status of the last
 * process, or the signal that terminated
 * the last process to be terminated.
 */
int smallsh_status(int status, int signalNum);


/*
 * Changes to the specified directory.
 */
int smallsh_cd (int numArgs, char *userArgs[]);

