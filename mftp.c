/*  Erik Safford
 *  IPv4 Server Client Socket Connection Ftp System
 *  Spring 2019  */

#include "mftp.h"
//To include DEBUG output, set DEBUG to 1 in mftp.h

void parseFilepath(char *filePath, char *file);  //Function Prototypes
void readServAck(char *buffer, int socketfd); 
int isReg(const char *path);
int getDataConnection(char *buffer, char *argv[], int socketfd, int socketfd2);

void parseFilepath(char *filePath, char *file) {  //Parse the last component of <pathname>
	int i = 0;
        int mark = 0;  //Mark is spot in secondArg (filepath) that filename
                       //to be created should be read from
        while(filePath[i] != '\0') {
        	if(filePath[i] == '/') {
                	mark = i+1;
                	if(DEBUG) { printf("mark at position %d\n",mark); }
                }
                i++;
        }
        i = 0;
        while(filePath[mark] != '\0') {  //Read the filename w/out /'s into file buffer
        	if(DEBUG) { printf("Copying %c to file\n",filePath[mark]); }
                file[i] = filePath[mark];
                if(DEBUG) { printf("File is '%s'\n",file); }
                i++;
                mark++;
        }
        file[i] = '\0';
}

void readServAck(char *buffer, int socketfd) {  //Read acknowledgement from server
	memset(buffer, 0, 4096);  
        int n = 0;
        char input[1];
        while(1) {
        	read(socketfd,input,1);  //Read 1 byte at a time so no overflow
                if(DEBUG) { printf("Read %c from socket\n",input[0]); }
                if(input[0] != '\n') {
                	buffer[n] = input[0];
                        n++;
                }
                else {
                	break;
                }
        }
        buffer[n] = '\0';
}

int isReg(const char *path) {  //Check if a file is a regular file, 1 = regular, 0 = non-regular
    struct stat pathStat;  //Make stat stucture to check file information
    stat(path, &pathStat); //Call stat w/ filepath of file to check, location of stat structure
    return S_ISREG(pathStat.st_mode);  //Check st_mode field of stat struct for protection
                                       //POSIX macro S_ISREG : is it a regular file?
}

int getDataConnection(char *buffer, char *argv[], int socketfd, int socketfd2) {  //Establish a data connection with server
	if(DEBUG) { printf("<MFTP> sending to server: D\n"); }
        write(socketfd,"D\n",2);  //write D get a data connection command to server

	readServAck(buffer, socketfd);  //Read acknowledgement
        if(DEBUG) { printf("read %s from server\n", buffer); }
	
	if(buffer[0] == 'E') {  //If error is read from server, print error
		fprintf(stderr,"Error getting port number : %d, %s\n",errno,strerror(errno));
		exit(1);
	}
	else {  //Else if no error recieved, make data connection through specified port
        	//Parse port number
        	char pn[10];   
        	int i = 1, portNum; //Port number
        	while(buffer[i] != '\0') {  
        		pn[i-1] = buffer[i];
                	i++;
        	}
        	pn[i-1] = '\0';
        	if(DEBUG) { printf("Port for data connection is : %s\n",pn); }
        	portNum = atoi(pn);
        	if(DEBUG) { printf("Creating socket with port '%d'\n",portNum); }

        	//Make data connection socket
        	if( (socketfd2 = socket( AF_INET, SOCK_STREAM, 0)) < 0) {  //Make socket using TCP protocol
        		perror("data socket");
                	exit(1);
        	}
        	if(DEBUG) { printf("Data connection socket created\n"); }

		struct sockaddr_in servAddr2;  //Socket structure for data connection
		struct hostent *hostEntry2;
		struct in_addr **pptr2;

        	//Set up the address of the server
		memset(&servAddr2, 0,sizeof(servAddr2));
        	servAddr2.sin_family = AF_INET;
        	servAddr2.sin_port = htons(portNum);  //Connect to specified port number

        	if( (hostEntry2 = gethostbyname(argv[1])) == NULL ) {
        		herror("gethostbyname");
                	exit(1);
        	}

        	//Structure pointer magic
        	pptr2 = (struct in_addr**) hostEntry2->h_addr_list;
        	memcpy(&servAddr2.sin_addr, *pptr2,sizeof(struct in_addr));

        	//Connect and read from server
        	if( (connect(socketfd2, (struct sockaddr *) &servAddr2,sizeof(servAddr2))) < 0 ) {
        		perror("connect");
                	exit(1);
        	}
        	if(DEBUG) { printf("Connected through socket file descriptor %d\n", socketfd2); }
		return(socketfd2);  //Return new data connection fd
	}
}

int main(int argc, char **argv) {
	if(argc < 2) {  //If no server connection (ip address) entered, error and exit
		printf("Invalid connection format, format is: <executable> <hostname/address>\n");
		exit(1);
	}
	
	int socketfd, socketfd2; //Socket file descriptors, first is for server connection
	                         //Second is for data connection, to be used when commands require it

	if( (socketfd = socket( AF_INET, SOCK_STREAM, 0)) < 0) {  //Make socket using TCP protocol
		perror("socket");
		exit(1);
	}

	struct sockaddr_in servAddr;  //Socket structure for server connection
	struct hostent *hostEntry;
	struct in_addr **pptr;

	memset(&servAddr, 0,sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(MY_PORT_NUMBER);

	if( (hostEntry = gethostbyname(argv[1])) == NULL ) { //If invalid server connection entered, error and exit
		herror("gethostbyname");
		exit(1);
	}

	//Structure pointer magic 
	pptr = (struct in_addr**) hostEntry->h_addr_list;
	memcpy(&servAddr.sin_addr, *pptr,sizeof(struct in_addr));

	//Connect to server
	if( (connect(socketfd, (struct sockaddr *) &servAddr,sizeof(servAddr))) < 0 ) {
		perror("connect");
		exit(1);
	}
	if(DEBUG) { printf("Connected through socket file descriptor %d\n", socketfd); }

	char buffer[4096]; //buffer holds read in commands
	int bytesRead, tFlag = 0;  //tFlag keeps track of number of tokens read by strtok()
        char *token, firstArg[10], secondArg[1000];  //buffers for first/second token
	
	write(1, "<MFTP> ", 7);
	while( (bytesRead = read(0, buffer, 100)) > 0 ) {  //Read commands from stdin
		buffer[bytesRead-1] = '\0';  //-1 to remove \n
		if(DEBUG) { printf("User entered: %s\n", buffer); }

   		token = strtok(buffer, " \t\r\n\v\f");  //Break tokens on whitespace
   		while( token != NULL && tFlag != 2) { //Walk through the other tokens, 2 max
      			if(tFlag == 0) {  //Get first command line argument
				strcpy(firstArg, token);
				if(DEBUG) { printf("Command: <%s> Arguments: ", token); }
				tFlag++;
			}
			else {  //Get second command line argument
				strcpy(secondArg, token);
				if(DEBUG) { printf("<%s> ", token); }
				tFlag++;
			}
      			token = strtok(NULL, " ");
   		}
		tFlag = 0;  //Reset flag
		fflush(stdout);

		//exit: terminate the client program after instructing the server’s child process to do the same
		if(firstArg[0] == 'e' && firstArg[1] == 'x' && firstArg[2] == 'i' && firstArg[3] == 't') {
			if(DEBUG) { printf("'exit' command recieved, signaling server to exit...\n"); }
			write(socketfd,"Q\n",2);  //Write Q quit command to server

			readServAck(buffer, socketfd);  //Read acknowledgement
        		printf("read %s from server\n",buffer);
		        
			if(buffer[0] == 'E') {
				fprintf(stderr,"Server exit error\n");
			}	
			fflush(stdout);
			break;  //breaking out of while loop closes server connection and exits
		}
		//cd <pathname>: change the current working directory of the client process to <pathname>. 
		//It is an error if <pathname> does not exist, or is not a readable directory
		else if(firstArg[0] == 'c' && firstArg[1] == 'd') {
			char directory[100];
			int errFlag = 0;
                	if( getcwd(directory,sizeof(directory)) == NULL ) {  //Store cwd in directory[]
                        	perror("getcwd");
                	}
			if(DEBUG) { printf("cd command typed, current working directory is %s\n",directory);
			            printf("attempting to change cwd to %s\n",secondArg); }
			if(access(secondArg,F_OK) != 0) { //Check access to see if the directory/file exists (F_OK)
                                perror("access exist");  //0 = exists, 1 = doesnt exist
				errFlag = 1;
        		}
        		if(access(secondArg,R_OK) != 0) { //Check access to see if the directory/file is readable (R_OK)
                                perror("access readable"); //0 = readable, 1 = not readable
				errFlag = 1;
			}
			if(chdir(secondArg) == -1) { //Attempt to change to specified directory
                        	perror("chdir");
				errFlag = 1;
	                }
			if(errFlag == 1) {  //If any errors encountered, print error
				if(DEBUG) { printf("error encountered while attempting to change directories\n"); }
				fprintf(stderr,"Error changing directories : %d, %s\n",errno,strerror(errno));
			}
			else {
				if(DEBUG) { printf("directory changed to %s\n",secondArg); }
			}
			printf("<MFTP> ");
			fflush(stdout);
		}
		//rcd <pathname>: change the current working directory of the server child process to pathname.
		//It is an error if pathname does not exist, or is not a readable directory
		else if(firstArg[0] == 'r' && firstArg[1] == 'c' && firstArg[2] == 'd') {
			if(DEBUG) { printf("rcd command typed, attempting to change server cwd to %s\n",secondArg); }
			char filePath[1024];
			sprintf(filePath,"C%s\n",secondArg);
			if(DEBUG) { printf("sending to server: %s",filePath);
			            printf("Writing %ld bytes to server\n",strlen(filePath)); }
			write(socketfd,filePath,strlen(filePath));  //Write C change directory command to server with filepath

			readServAck(buffer, socketfd);  //Read acknowledgement
			printf("read %s from server\n",buffer);

			printf("<MFTP> ");
			fflush(stdout);
		}
		//ls: execute the “ls –la | more -d -20” command locally and display the output to standard output, 
		//20 lines at a time, waiting for a space character on standard input before displaying 
		//the next 20 lines
		else if(firstArg[0] == 'l' && firstArg[1] == 's') {
			if(DEBUG) { printf("ls command typed, printing current directory\n"); }

			int fd[2];    //Pipe file descriptors, 0 = stdin, 1 = stdout
	                int rdr,wtr;  //Reader and writer streams, initialize after pipe()

			if(pipe(fd) == -1) { //Setup pipe and file descriptors
        	                perror("pipe");
	                }
			rdr = fd[0]; //Assign file descriptor 0 to reader (i.e. stdin)
                	wtr = fd[1]; //Assign file descriptor 1 to writer (i.e. stdout)
			
			if(fork() == 0) {  //Child process runs exec commands
				if(fork() == 0) {
					if(DEBUG) { printf("execlp'ing ls -la\n"); }
					close(1);  //Close stdout
	                                close(rdr); //close fd[0] reader
        	                        if(dup2(wtr,1) == -1) { //Redirect stdout to writer fd[1] (pipe[1])
                	                        perror("dup2");
                        	        }
                                	execlp("ls","ls","-la",NULL); //Exec wont return if successful
                                	perror("execlp failed");      //print error if it does
                                	exit(1);
				}
				else {
					if(DEBUG) { printf("execlp'ing more -d -20\n"); }
					close(0); //close stdin
                       		        close(wtr); //close fd[1] writer
                                	if(dup2(rdr,0) == -1) { //Redirect stdin to reader fd[0] (pipe[0])
                                        	                //Output will be via stdout
                                        	perror("dup2");
                                	}
                                	execlp("more","more","-d","-20",NULL); //Exec wont return if successful
                                	perror("execlp failed");               //print error if it does
                                	exit(1);
				}
			}
			else {  //Parent waits for child exec processes to complete
				close(rdr);
				close(wtr);
				wait(NULL);
			}

			printf("<MFTP> ");
			fflush(stdout);
		}
		//rls: execute the “ls –l” command on the remote server and display the output to the client’s
		//standard output, 20 lines at a time in the same manner as “ls” above.
		else if(firstArg[0] == 'r' && firstArg[1] == 'l' && firstArg[2] == 's') {
			if(DEBUG) { printf("rls command typed, attempting to establish data connection\n"); }
			socketfd2 = getDataConnection(buffer, argv, socketfd, socketfd2); //Get a data connection
			
			write(socketfd,"L\n",2);  //Write L list directory command to server
			if(DEBUG) { printf("Waiting for server acknowledgement\n"); }
			
			readServAck(buffer, socketfd);  //Read acknowledgement
			printf("read %s from server\n", buffer);
			
			if(buffer[0] != 'E') {  //If no error recieved from server
				//Read in ls -la stuff from server
				if(fork()) {
					wait(NULL);
				}
				else {
					close(0);  //close stdin
					dup2(socketfd2,0); //redirect data connection to stdin
					execlp("more","more","-d","-20",NULL);  //Exec wont return if successful
                                	perror("execlp more failed");           //print error if it does
                                	exit(1);
				}
			}
			if(DEBUG) { printf("Closing data connection\n"); }
			close(socketfd2);  //close data connection
                        printf("<MFTP> ");
                        fflush(stdout);
		}
		//show <pathname> : retrieve the contents of the indicated remote <pathname> and write it to the
		//client’s standard output, 20 lines at a time. Wait for the user to type a space character 
		//before displaying the next 20 lines. Note: the “more” command can be used to implement this feature.
		//It is an error if <pathname> does not exist, or is anything except a regular file, or is not readable.
		else if(firstArg[0] == 's' && firstArg[1] == 'h' && firstArg[2] == 'o' && firstArg[3] == 'w') {
			if(DEBUG) { printf("show command typed, attempting to show %s\n",secondArg);
			            printf("Attempting to establish data connection\n"); }
			socketfd2 = getDataConnection(buffer, argv, socketfd, socketfd2); //Get data connection

                        char filePath[1024];
                        sprintf(filePath,"G%s\n",secondArg);  //Write G get a file command to server
                        if(DEBUG) { printf("sending to server: %s",filePath);
                                    printf("Writing %ld bytes to server\n",strlen(filePath)); }
                        write(socketfd,filePath,strlen(filePath));  //Write G<filepath>\n to server
			if(DEBUG) { printf("Waiting for server acknowledgement\n"); }
			
			readServAck(buffer, socketfd);  //Read acknowledgement
			printf("read %s from server\n", buffer);
			
			if(buffer[0] != 'E') { //If no error recieved from server
				//Read in file contents from server
				if(fork()) {
                                	wait(NULL);
                        	}
                        	else {
                                	close(0);  //close stdin
                                	dup2(socketfd2,0); //redirect data connection to stdin
                                	execlp("more","more","-d","-20",NULL);  //Exec wont return if successful
                                	perror("execlp more failed");           //print error if it does
                                	exit(1);
                        	}
			}
			else {  //Else print error recieved from server
				fprintf(stderr,"Error showing pathname\n");
			}

                        if(DEBUG) { printf("Closing data connection\n"); }
                        close(socketfd2);  //close data connection
                        printf("<MFTP> ");
                        fflush(stdout);
		}
		//get <pathname> : retrieve <pathname> from the server and store it locally in the client’s current working
                //directory using the last component of <pathname> as the file name. It is an error if <pathname> does not exist,
                //or is anything other than a regular file, or is not readable. It is also an error if the file cannot be 
		//opened/created in the client’s current working directory.
		else if(firstArg[0] == 'g' && firstArg[1] == 'e' && firstArg[2] == 't') {
			if(DEBUG) { printf("get command typed, attempting to get %s\n",secondArg);
			            printf("Attempting to establish data connection\n"); }
                        socketfd2 = getDataConnection(buffer, argv, socketfd, socketfd2);  //Get data connection

                        char filePath[1024];
                        sprintf(filePath,"G%s\n",secondArg);  //Write G get a file commands to server
                        if(DEBUG) { printf("sending to server: %s",filePath);
                                    printf("Writing %ld bytes to server\n",strlen(filePath)); }
                        write(socketfd,filePath,strlen(filePath));  //Write G<filepath>\n to server
                        if(DEBUG) { printf("Waiting for server acknowledgement\n"); }
                        
			readServAck(buffer, socketfd);   //Read acknowledgement
                        printf("read %s from server\n", buffer);

                        if(buffer[0] != 'E') {  //If no error recieved from server
                                //Read in file contents from server into new file
				if(DEBUG) { printf("Filepath is '%s'\n",secondArg); }
				char file[1000];

				parseFilepath(secondArg,file);  //Get the last component of filepath

				if(DEBUG) { printf("File to be created is '%s'\n",file); }
				
				int fp, i = 0;
				//Attempt to open last component of filepath for writing, print error if cant
				if( (fp = open(file,O_WRONLY | O_CREAT | O_TRUNC,0755)) < 0 ) {
					fprintf(stderr,"Error opening file to be created : %d, %s\n",errno,strerror(errno));
				}
				else {  //Else start reading from server/writing into created file
					i = 0;
					memset(buffer, 0, 4096);
					while(1) {
						i = read(socketfd2,buffer,1); //read from data connection
						if(i <= 0) {
							break;
						}	
						else {
							if(write(fp,buffer,1) < 0) {  //Write into file, print error if cant
								fprintf(stderr,"Write Error : %d, %s\n",errno,strerror(errno));
								break;
							}
						}
					}
					close(fp);  //Close created file
					if(DEBUG) { printf("Finished writing to file %s\n",file); }
				}
                        }
			else {  //Else print error recieved from server
				fprintf(stderr,"Error getting pathname\n");
			}
			if(DEBUG) { printf("Closing data connection\n"); }
                        close(socketfd2);  //close data connection
                        printf("<MFTP> ");
                        fflush(stdout);

		}
		//put <pathname> : transmit the contents of the local file <pathname> to the server
		//and store the contents in the server process’ current working directory using the
		//last component of <pathname> as the file name. It is an error if <pathname> does
		//not exist locally or is anything other than a regular file. It is also an error if
		//the file cannot be opened/created in the server’s child process’ current working
		//directory.
		else if(firstArg[0] == 'p' && firstArg[1] == 'u' && firstArg[2] == 't') {
			if(DEBUG) { printf("put command typed, attempting to open and send %s to server\n",secondArg); }
			
			if(isReg(secondArg) != 1) {  //Make sure pathname is a regular file, print error if not
				fprintf(stderr,"Error filepath is not a regular file or doesn't exist locally\n");
			}
			else {  //Else attempt to open pathname for reading
				int fp;
				if( (fp = open(secondArg,O_RDONLY)) < 0) {  //If cant open pathname, print error
                        		fprintf(stderr,"Error opening filepath : %d, %s\n",errno,strerror(errno));
                        	}  
                        	else {  //Else start reading from file/writing to server
					if(DEBUG) { printf("Attempting to establish data connection\n"); }
                                        socketfd2 = getDataConnection(buffer, argv, socketfd, socketfd2); //Get data connection

					char filePath[1024];
					char file[1024];
					if(DEBUG) { printf("filePath is %s\n",secondArg); }
					parseFilepath(secondArg,file);  //get last component of pathname

 	                        	sprintf(filePath,"P%s\n",file); //Send P put a file command to server w/ last component
         	                	if(DEBUG) { printf("sending to server: %s",filePath);
                        		            printf("Writing %ld bytes to server\n",strlen(filePath)); }
                        		write(socketfd,filePath,strlen(filePath));  //Write P<filepath>\n to server
                        		if(DEBUG) { printf("Waiting for server acknowledgement\n"); }
                        		
					readServAck(buffer, socketfd);  //Read acknowledgement
                        		printf("read %s from server\n", buffer); 

					if(buffer[0] != 'E') {  //If no error recieved from server
                                		if(DEBUG) { printf("Writing file to server\n"); }
                                		char buf[100];
                                		int n;
                                		while(1) {
                                			n = read(fp,buf,100);  //read from opened file
                                        		if(n <= 0) {
                                        			break;
                                        		}
                                        		else {
                                        			if( write(socketfd2,buf,n) < 0) {  //write file contents to server through data connection
									fprintf(stderr,"Write Error : %d, %s\n",errno,strerror(errno));
                                                                	break;
								}
								if(DEBUG) { printf("wrote %d bytes to server\n",n); }
                                        		}
                                		}
                                		if(DEBUG) { printf("Finished writing file to server\n"); }
					}
					else {  //Else print error recieved from server
						fprintf(stderr,"Error putting pathname, could not be opened/created\n");
					}
					close(fp);  //close opened file
					if(DEBUG) { printf("Closing data connection\n"); }
                                        close(socketfd2);  //close data connection
                        	}
			}
                        printf("<MFTP> ");
                        fflush(stdout);
		}
		//If client user enters incorrect input
		else {
			printf("Incorrect input\n");
			printf("Valid commands are: exit, cd <filepath>, rcd <filepath>, ls, rls,\n show <filepath>, get <filepath>, put <filepath>\n");
			printf("<MFTP> ");
			fflush(stdout);
		}
	}
	
	printf("<MFTP> Client exiting...\n");
	close(socketfd);  //Close server connection
	exit(0);
}
