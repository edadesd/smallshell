# smallshell

A simple UNIX shell written in C. Executes programs entered via command line.

<h2>Usage</h2>
Compile according to the instructions in readme.txt, execute the compiled program in a command line to start the shell.

Enter the name of the program you wish to execute once smallshell is running, along with any arguments to that program after the name. You can use < or > to redirect input or output, or & to make the program execute in the background. Blank lines and lines beginning with a # are treated as comment lines and ignored. 

<h3>Built-in Commands</h3>
<b>Status</b>
Prints the last returned status, or the signal number of the last signal that terminated a program.

<b>cd</b><br>
Changes working directory to the specified directory using either relative or absolute path. Allows use of ~(HOME) "/"(root) and ".." (up one level) shortcuts. If no target specified, changes working directory to home.

<b>exit</b><br>
Kills all processes launched by smallshell and then exits smallshell, returning 0.
