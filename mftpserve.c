/*  Erik Safford
 *  IPv4 Server Client Socket Connection Ftp System
 *  Spring 2019  */

#include "mftp.h"
//To include DEBUG output, set DEBUG to 1 in mftp.h

int isDir(const char *path);  //Prototype

int isDir(const char *path) {  //Check to see if <pathname> is a directory
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int main(int argc, char **argv) {
	int listenfd;  //fd to set to set up the server socket

	if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { //Create socket using IPv4 internet protocol (TCP)
		perror("socket");
		exit(1);
	}
	if(DEBUG) { printf("Server socket created through file descriptor %d\n", listenfd); }

	struct sockaddr_in servAddr;  //Set up server socket structure

	memset(&servAddr, 0,sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(MY_PORT_NUMBER);  //Port 49999
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if( bind(listenfd, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0 ) {  //Bind socket to a port
		perror("bind");
		exit(1);
	}
	if(DEBUG) { printf("Socket bound to port\n"); }

	if( (listen(listenfd, 4)) < 0 ) {  //Sets up a connection queue for up to 4 connections
		perror("listen");
		exit(1);
	}
	if(DEBUG) { printf("Listening for connections...\n\n"); }

	int connectfd;   //fd to accept client connection
	int datafd, clientdfd, Dflag = 0;  //fd's for client data connection, flag for data connection 
	int status;  //For waitpid

	int length = sizeof(struct sockaddr_in);  //Socket structure setup for accept client connection
	struct sockaddr_in clientAddr;

	while(1) {
		//Blocks until a new connection is established by a client
		if( (connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length)) < 0 ) {
                                perror("accept");
                                exit(1);
                }
		if(DEBUG) { printf("Client connection found, using file descriptor %d\n",connectfd); }

		if(fork()) {  //Parent process closes connection, and waits for child process to finish
			close(connectfd);  //Close client file descriptor
			int changedPID = waitpid(-1, &status, 0);  //Same as wait(NULL)
			if(DEBUG) { printf("Forking process %d to deal with new client connection\n",changedPID); }
		}	
		else {  //Double fork to prevent zombie process, client interaction is taken care of by grandchild process
			//grandchild parent exits immediately so grandchild will be taken care of by init process
			//grandchild becomes a daemon process and is cleaned up by OS once it terminates
			//since grandchild parent exits immediately, wait(NULL) in original parent returns and new
			//connections are allowed
                	if(fork()) {
				exit(0);
			}
			//Child process gets name of connection, then writes processes commands from/to client through socket connection
			else {
				struct hostent* hostEntry;
                		char* hostName;

				//Convert a numerical Internet address to a host name
                		if( (hostEntry = gethostbyaddr(&(clientAddr.sin_addr),sizeof(struct in_addr), AF_INET)) == NULL) {
					herror("gethostbyaddr");
					exit(1);
				}
                		hostName = hostEntry->h_name;
                		printf("Control connection established with: %s\n",hostName); //Print client name

				char input[1];      //For reading 1 char at a time
                		char buffer[4096];  //For holding the read in commands, terminated with \n
				int bytesRead, breakFlag = 0;
				
				while(1) {
					int i = 0;
					while( (bytesRead = read(connectfd,input,1)) != EOF ) {  //Read from client into buffer until \n
						if(bytesRead == 0) {  //When 0 bytes is read, client has unexpectedly exited
							breakFlag = 1;  //Set flag to exit client connection
							break;
						}
						if(input[0] != '\n') {  //Otherwise read in commands normally
							buffer[i] = input[0];  
							i++;
						}
						else {
							break;
						}
					}
					if(breakFlag == 1) {  //If client exited randomly, break and exit
						printf("Client unexpectedly exited\n");
						break;
					}
					buffer[i] = '\0';   //Else terminate the read in command so can be processed
					printf("Server reads: %s\n",buffer);
					
					//“Q” Quit. This command causes the server (child) process to exit normally.
					//Before exiting, the server will acknowledge the command to the client
					if(buffer[0] == 'Q') {
						if(DEBUG) { printf("'Q' exit command recieved, acknowledging client\n\n"); }
						write(connectfd,"A\n",2);
						break;
					}

					//“C<pathname>” Change directory. The server responds by executing a chdir()
					//system call with the specified path and transmitting an acknowledgement to the 
					//client. If an error occurs, an error message is transmitted to the client
					if(buffer[0] == 'C') {
						if(DEBUG) { printf("'C' change directory command recieved\n"); }
                                        	char filePath[100];
                                        	int i = 1;
                                        	while(buffer[i] != '\0') {  //Read in the \n terminated filepath from the C client command
                                                	filePath[i-1] = buffer[i];
                                                	i++;
                                        	}
                                        	filePath[i-1] = '\0';

                                        	char directory[100];
                                        	int errFlag = 0;
                                        	if( getcwd(directory,sizeof(directory)) == NULL ) {  //Store cwd in directory[]
                                                	perror("getcwd");
                                        	}
                                        	if(DEBUG) { printf("Server current working directory is %s\n",directory);
                                        	            printf("Attempting to change directory to '%s'\n",filePath); }

                                        	if(access(filePath,F_OK) != 0) { //Check access to see if the directory exists (F_OK)
                                                	perror("access exist");  //0 = exists, 1 = doesnt exist
                                                	errFlag = 1;
                                        	}
                                        	if(access(filePath,R_OK) != 0) { //Check access to see if the directory is readable (R_OK)
                                                	perror("access readable"); //0 = readable, 1 = not readable
                                                	errFlag = 1;
                                        	}
                                        	if(chdir(filePath) == -1) { //Attempt to change to specified directory
                                                	perror("chdir");
                                                	errFlag = 1;
                                        	}

                                        	if(errFlag == 1) {  //If any errors were encountered, print and send error
                                                	fprintf(stderr,"Error encountered while attempting to change directories, acknowledging client\n");
                                                	char errStr[] = "ENo such file or directory\n";
                                                	write(connectfd,errStr,strlen(errStr));
                                        	}
                                        	else {  //Else write success
                                                	printf("Directory successfully changed to %s, acknowledging client\n",filePath);
                                                	write(connectfd,"A\n",2);
                                        	}
                                        	fflush(stdout);
					}
					//“D” Establish the data connection.  The server acknowledges the command on the control connection by sending
                                	//an ASCII coded decimal integer, representing the port number it will listen on, followed by a new line character.
                                	if(buffer[0] == 'D') {
                                        	if(DEBUG) { printf("'D' establish data connection command recieved\n"); }
                                        	int errFlag = 0;

                                        	//Make a socket
                                        	if( (datafd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { //Create socket using IPv4 internet protocol (TCP)
                                                	perror("data socket");
                                                	errFlag = 1;
                                        	}
                                        	if(DEBUG) { printf("Data socket created through file descriptor %d\n", datafd); }

                                        	//Bind the socket to port 0 - wildcard for “pick any available ephemeral port”
                                        	struct sockaddr_in dataAddr;
                                        	memset(&dataAddr, 0,sizeof(dataAddr));
						dataAddr.sin_family = AF_INET;
                                                dataAddr.sin_port = htons(0);  //Port value 0 defaults to available port
                                                dataAddr.sin_addr.s_addr = htonl(INADDR_ANY);

                                        	if( bind(datafd, (struct sockaddr*) &dataAddr, sizeof(dataAddr)) < 0 ) {  //Bind socket to a port
                                                	perror("bind");
                                                	errFlag = 1;
                                        	}
                                        	if(DEBUG) { printf("Socket bound to port\n"); }

                                        	//Get the ephemeral port that socket binded to
                                        	struct sockaddr_in sin;
                                        	memset(&sin, 0,sizeof(sin));
                                        	socklen_t len = sizeof(sin);
                                        	if(getsockname(datafd, (struct sockaddr *)&sin, &len) == -1) {
                                                	perror("getsockname");
                                                	errFlag = 1;
                                        	}
                                        	if(DEBUG) { printf("port number %d\n", ntohs(sin.sin_port)); }
                                        	int socketNum = ntohs(sin.sin_port);
					
                                        	//Acknowledge client with port number to connect to
                                        	if(errFlag != 1) {  //If no errors encountered, write success
                                                	char ackPort[10];
                                                	sprintf(ackPort,"A%d\n",socketNum);
							printf("Writing: '%s' to client\n",ackPort);
                                                	write(connectfd,ackPort,strlen(ackPort)); 
                                        	}
                                        	else {  //Else print and write error
							fprintf(stderr,"Error encountered while attempting to create data socket connection, acknowledging client\n");
							char errStr[] = "ECouldn't create data socket connection\n";
                                                	write(connectfd,errStr,strlen(errStr));
                                        	}

                                        	//Listen for and accept data connection from client, 0 connection queue only listens for single connection
                                        	if( (listen(datafd, 0)) < 0 ) {
                                                	perror("listen");
                                        	}

						if(DEBUG) { printf("Data socket listening for connections...\n"); }

                                        	int length2 = sizeof(struct sockaddr_in);
                                        	struct sockaddr_in clientAddr2;

                                        	//Accept client data connection
						if( (clientdfd = accept(datafd, (struct sockaddr *) &clientAddr2, &length2)) < 0 ) {
                                                	perror("accept");
                                        	}
                                        	if(DEBUG) { printf("Client connection found, using file descriptor %d\n",clientdfd); }
						Dflag = 1;  //Set flag so other server commands know new data connection is established
                                	}
					//“L”  List  directory. The server child process responds by initiating a child process to execute the “ls -la”
                                	//command. It is an error to for the client to issue this command unless a data connection has been previously
                                	//established. Any output associated with this command is transmitted along the data connection to the client.
                                	//The server child process waits for its child (ls) process to exit, at which point the data connection is closed.
                                	//Following the termination of the child (ls) process, the server reads the control connection for additional commands.
					if(buffer[0] == 'L') {
						if(DEBUG) { printf("'L' list directory command recieved\n"); }
						if(Dflag == 0) {   //If no data connection, print and write error
							char errStr[] = "ENo data connection created\n";
							write(connectfd,errStr,strlen(errStr));
							fprintf(stderr,"No data connection created : %d, %s\n",errno,strerror(errno));
						}
						else {  //Else list the directory by exec'ing ls and write through client data connection
							write(connectfd,"A\n",2);
							if(DEBUG) { printf("Forking child process to exec 'ls -l'\n"); }
							if(fork()) {
								close(clientdfd);
								wait(NULL);
							}
							else {
								close(1); //Close stdout
								dup2(clientdfd,1);  //Redirect stdout to be the data connection
								execlp("ls","ls","-la",NULL);  //Exec ls -la command
								perror("execlp ls failed");
							}
							close(clientdfd);  //Close data connection
							close(datafd);  
							Dflag = 0;  //Reset data connection flag
						}
					}
					//“G<pathname>” Get a file. It is an error to for the client to issue this command unless a data connection has
                                	//been previously established. If the file cannot be opened or read by the server, an error response is generated.
                                	//If there is no error, the server transmits the contents of <pathname> to the client over the data connection
                                	//and closes the data connection when the last byte has been transmitted.
					if(buffer[0] == 'G') {
						if(DEBUG) { printf("'G' get a file command recieved\n"); }
						if(Dflag == 0) {  //If no data connection, print and write error
							printf("Sending E acknowledgement\n");
							char errStr[] = "ENo data connection created\n";
							write(connectfd,errStr,strlen(errStr));
							fprintf(stderr,"No data connection created : %d, %s\n",errno,strerror(errno));
						}
						else {  //Else get the server file by opening for reading on server and writing to client 
							//through the data connection
							char filePath[1000];  //Get filepath from read in command, remove G and \n
							int i = 1;
							while(1) {
								if(buffer[i] != '\n') {
									filePath[i-1] = buffer[i];
									i++;	
								}
								else {
									break;
								}
							}
							filePath[i-1] = '\0';
							printf("filepath read from client is '%s'\n",filePath);

							int fp;
							if(DEBUG) { printf("isDir = %d\n",isDir(filePath)); }		
							if(isDir(filePath) == 1) {  //Check if given path is a directory, write error if it is
								printf("Sending E acknowledgement\n");
								char errStr[] = "EFile is a directory\n";
                                                        	write(connectfd,errStr,strlen(errStr));
								fprintf(stderr,"File is a directory : %d, %s\n",errno,strerror(errno));
							}
							else {  //Else try to open the path for reading
								if( (fp = open(filePath,O_RDONLY)) < 0) {  //Write error if cant be opened
									fprintf(stderr,"open error : %d %s\n",errno,strerror(errno));
									printf("Sending E acknowledgement\n");
									char errStr[] = "EFile doesn't exist or isn't readable\n";
									write(connectfd,errStr,strlen(errStr));
								}
								else {  //Else write success, and begin reading from file/writing file to client
									printf("Sending A acknowledgement\n");
									write(connectfd,"A\n",2);
									if(DEBUG) { printf("Writing file to client\n"); }
									char buf[100];
	                                                		int n;
        	                                        		while(1) {
                	                                        		n = read(fp,buf,100);
                        	                                		if(n <= 0) {  //If file has been read completely/error, break
                                	                                		break;
                                        	                		}
                                                	        		else {
                                                        	        		if(write(clientdfd,buf,n) < 0) {  //If client presses Q before writing is finished
												                          //Print error and break out of write loop
												fprintf(stderr,"write error : %d %s\n",errno,strerror(errno));
												break;
											}
											if(DEBUG) { printf("wrote %d bytes to client\n",n); }
                                                        			}
                                                			}
									if(DEBUG) { printf("Finished writing file to client\n"); }
								}
							}
							close(fp);  //Close opened file
							close(clientdfd);  //Close data connection
                                                	close(datafd); 
							Dflag = 0;
						}
					}
					//“P <pathname>” Put a file. It is an error to for the client to issue this command unless a data connection has
					//been previously established. The server attempts to open the last component of <pathname> for writing. It is
                                        //an error if the file already exists or cannot be opened. The server then reads data from the data connection and
                                        //writes it to the opened file. The file is closed and the command is complete when the server reads an EOF from
                                        //the data connection, implying that the client has completed the transfer and has closed the data connection.
					if(buffer[0] == 'P') {
						if(DEBUG) { printf("'P' put a file command recieved\n"); }
                                                if(Dflag == 0) {  //If no data connection, print and write error
                                                        printf("Sending E acknowledgement\n\n");
                                                        char errStr[] = "ENo data connection created\n";
                                                        write(connectfd,errStr,strlen(errStr));
                                                        fprintf(stderr,"No data connection created : %d, %s\n",errno,strerror(errno));
                                                }
                                                else {  //Else try to create file on server side, and read in file contents from client
							printf("'%s' read from client\n",buffer);
							char filePath[1000];  //Get filepath from client, remove P and \n
							int i = 1;
                                                        while(1) {
                                                                if(buffer[i] != '\n') {
                                                                	filePath[i-1] = buffer[i];
									i++;
								}
                                                                else {
                                                                        break;
                                                                }
                                                        }
                                                        filePath[i] = '\0';
                                                        if(DEBUG) { printf("filepath from client is '%s'\n",filePath); }

							char pfilePath[1000]; //Get last component of filepath (filename), remove /'s if any
							int j = 0;
							i = 0;
                                                        while(1) {
                                                                if(filePath[j] != '\0') {
									if(filePath[j] == '/') {
										i = 0;
										j++;
										memset(pfilePath,0,1000);
									}
									else {
                                                                        	pfilePath[i] = filePath[j];
                                                                        	i++;
										j++;
									}
                                                                }
                                                                else {
                                                                        break;
                                                                }
                                                        }
                                                        pfilePath[i] = '\0';
							printf("parsed filename is '%s'\n",pfilePath);
							
							if(access(filePath,F_OK) == 0) { //Check access to see if the file already exists (F_OK)
								                         //if it does, print/write error
                        	                                perror("access exist");  //0 = exists, 1 = doesnt exist
                	                                        char errStr[] = "Efile already exists\n";
								write(connectfd,errStr,strlen(errStr));
        	                                        }
							else {  //Else if the file doesn't already exist, create file and begin reading/writing from client
								int fp;
								//If file can't be created for some reason, print/write error
   					                	if( (fp = open(pfilePath,O_WRONLY | O_CREAT | O_TRUNC,0755)) < 0 ) { 
									fprintf(stderr,"Couldnt open %s to be created : %d, %s\n",pfilePath,errno,strerror(errno));
									char errStr[] = "Efile could not be opened\n";
                                                                	write(connectfd,errStr,strlen(errStr));
                                				}
								else {  //Else write success and begin reading from client and writing into file
									write(connectfd,"A\n",2);
                                					i = 0;
                                					char buf[100];
                                					while(1) {
                                        					i = read(clientdfd,buffer,100);
                                        					if(i <= 0) {
                                                					break;
                                        					}
                                        					else {
                                                					if( write(fp,buffer,i) < 0 ) {  //If error writing to file, break write loop
												fprintf(stderr,"write error : %d %s\n",errno,strerror(errno));
                                                                                                break;
											}
											if(DEBUG) { printf("writing %d bytes to server\n",i); }
                                        					}
                                					}
                                					close(fp);  //Close created file
                                					if(DEBUG) { printf("Finished writing to file %s\n",pfilePath); }
								}
							}
							close(clientdfd);  //Close data connection
                                                        close(datafd);   
                                                        Dflag = 0;	
						}
					}
				}	
				printf("Exiting server child (grandchild) process...\n\n");
				close(connectfd);  //Close client file descriptor
				exit(0);  //Exit child process
			}
		}
	}
}
