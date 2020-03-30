# Server Client File Transfer Protocol System
* This miniature ftp system consists of two program exectuables, **mftpserve** and **mftp**, the server and client respectively.
* Both programs are written in C, and are designed to operate on a Linux OS environment.
* The programs utilize the Unix TCP/IP sockets interface library to communicate with each other across a TCP/IP network. Only IPv4 addressing is supported by these programs.
# User Interface
## Command Line
* The user initiates the client program using its name (mftp) with one required command line argument (in addition to the name of the program). This argument will be interpreted by the client program as the name of the host on which the mftpserve process is running and to which the client will connect. The hostname may be either a symbolic host name or IPv4 dotted-decimal notation. A DEBUG flag is included in mftp.h, and can be turned on to see extra output from the client/server commands. The client will use the server port 49999 to make its connection.
## Login
* The server process will not require a login dialog from the client. The server process will retain the identity and privileges of the user who initiated it and these privileges, identity and initial working directory are the context in which client commands are executed by the server.
## User Commands
* Following a successful connection to the server, the client program will prompt the user for commands from standard input. Commands are regarded as a series of tokens separated by one or more white-space characters and terminated by a new line character. The first token is the name of the command. Successive tokens are command arguments, if any. Note: the first token may be preceded by spaces. White-space characters are defined to be any character for which isspace() returns true. Commands and their arguments will be case-sensitive.
* The user commands are:
1. exit: terminate the client program after instructing the server’s child process to do the same.
2. cd _pathname_: change the current working directory of the client process to _pathname_. It is an error if _pathname_ does not exist, or is not a readable directory.
3. rcd _pathname_: change the current working directory of the server child process to _pathname_. It is an error if _pathname_ does not exist, or is not a readable directory.
4. ls: execute the “ls –l” command locally and display the output to standard output, 20 lines at a time, waiting for a space character on standard input before displaying the next 20 lines. See “rls” and “show”.
5. rls: execute the “ls –l” command on the remote server and display the output to the client’s standard output, 20 lines at a time in the same manner as “ls” above.
6. get _pathname_: retrieve _pathname_ from the server and store it locally in the client’s current working directory using the last component of _pathname_ as the file name. It is an error if _pathname_ does not exist, or is anything other than a regular file, or is not readable. It is also an error if the file cannot be opened/created in the client’s current working directory.
7. show _pathname_: retrieve the contents of the indicated remote _pathname_ and write it to the client’s standard output, 20 lines at a time. Wait for the user to type a space character before displaying the next 20 lines. Note: the “more” command is used to implement this feature. It is an error if _pathname_ does not exist, or is anything except a regular file, or is not readable.
8. put _pathname_: transmit the contents of the local file _pathname_ to the server and store the contents in the server process’ current working directory using the last component of _pathname_ as the file name. It is an error if _pathname_ does not exist locally or is anything other than a regular file. It is also an error if the file cannot be opened/created in the server’s child process’ current working directory.
## Notes
* The client program reports command errors and operational errors in a manner that is clear and understandable to the user. If the error is recoverable, the command is aborted and a new command prompt is issued. If the error is not recoverable, the client terminates.
* The get and put commands function equally well with both text and binary file data.
# Server
* The server process, mftpserve, is initiated with no command line arguments. The server will log its activity to stdout and its errors to stderr.
# Server/Client Interface Protocol
1. The mftpserve process listens on ephemeral TCP port number 49999 for client connections. It permits a queue of 4 connection requests simultaneously (see listen()).
2. The client program (mftp), prior to prompting the user for commands establishes a connection to the server process on TCP port 49999. If the connection fails, the client will issue an appropriate error message and abort.
3. When a connection is established, an appropriate success message is displayed to the user by the client and logged to stdout by the server, displaying the client’s name. This connection is regarded as the control connection. A second connection, the data connection, is required by certain other commands. The data connection is established when needed and will be closed at the end of the associated data transfer for each command that establishes it.
4. Once the control connection is established, the server process (child) will attempt to read commands from the control connection, execute those commands as described below, and then read another command, until a “Q” command is received, or until an EOF is received, in which case the server child process will terminate.
5. Server commands have the following syntax:
* No leading spaces.
* A single character command specifier.
* An optional parameter beginning with the second character of the command (the character following the single character command specifier) and ending with the character before the terminator.
* The command terminator is either a new line character or EOF.
6. Server response syntax:
* The server responds exactly once to each command it reads from the control connection.
* The response is either an error response or an acknowledgement.
* All responses are terminated by a new line character.
* An error response begins with the character “E”. The intervening characters between the “E” and the new line character comprise a text error message for display to the client’s user (to be written to standard output by the client).
* An acknowledgement begins with the character “A”. It may optionally contain an ASCII-coded decimal integer between the “A” and the new line character, if required by the server command (only the “D” command below requires such an integer in the acknowledgement).
7. The server control connection commands are:
* “D” Establish the data connection. The server acknowledges the command on the control connection by sending an ASCII coded decimal integer, representing the port number it will listen on, followed by a new line character.
* “C<i>pathname</i>” Change directory. The server responds by executing a chdir() system call with the specified path and transmitting an acknowledgement to the client. If an error occurs, an error message is transmitted to the client.
* “L” List directory. The server child process responds by initiating a child process to execute the “ls - l” command. It is an error for the client to issue this command unless a data connection has been previously established. Any output associated with this command is transmitted along the data connection to the client. The server child process waits for its child (ls) process to exit, at which point the data connection is closed. Following the termination of the child (ls) process, the server reads the control connection for additional commands.
* “G<i>pathname</i>” Get a file. It is an error to for the client to issue this command unless a data connection has been previously established. If the file cannot be opened or read by the server, an error response is generated. If there is no error, the server transmits the contents of _pathname_ to the client over the data connection and closes the data connection when the last byte has been transmitted.
* “P<i>pathname</i>” Put a file. It is an error for the client to issue this command unless a data connection has been previously established. The server attempts to open the last component of _pathname_ for writing. It is an error if the file already exists or cannot be opened. The server then reads data from the data connection and writes it to the opened file. The file is closed and the command is complete when the server reads an EOF from the data connection, implying that the client has completed the transfer and has closed the data connection.
* “Q” Quit. This command causes the server (child) process to exit normally. Before exiting, the server will acknowledge the command to the client.
8. Each command is logged by the server, along with the arguments and its success or failure, to standard output.
# Screenshots
## Getting Server IP
![Alt text](/screenshots/server_1.png?raw=true "Server 1")
## Client Side Example
![Alt text](/screenshots/client_1.png?raw=true "Client 1")
![Alt text](/screenshots/client_2.png?raw=true "Client 2")
![Alt text](/screenshots/client_3.png?raw=true "Client 3")
![Alt text](/screenshots/client_4.png?raw=true "Client 4")
![Alt text](/screenshots/client_5.png?raw=true "Client 5")
![Alt text](/screenshots/client_6.png?raw=true "Client 6")
![Alt text](/screenshots/client_7.png?raw=true "Client 7")
## Server Side Example
![Alt text](/screenshots/server_2.png?raw=true "Server 2")
![Alt text](/screenshots/server_3.png?raw=true "Server 3")
![Alt text](/screenshots/server_4.png?raw=true "Server 4")