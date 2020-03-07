#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

#define READ_SIZE 21	// Represents size of the READ buffer that we're reading in
#define MAX_FORKS 5		// Max number of connections allowed

// Function prototypes
void getHeaderInfo(char*, int, int, int*, int*, char*);
void getText(int, char*, char*, int, int);
void encryptText(char*, char*, char*, int);
void checkForTerm();
void setupSignals();
void catchSIGCHLD(int);
void removePid(int);

// Global vars
int childPids[MAX_FORKS];
int numChildren = 0;

int main(int argc, char *argv[])
{
	int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
	socklen_t sizeOfClientInfo;
	struct sockaddr_in serverAddress, clientAddress;	
	pid_t returnPid = -5;

	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); 	// Clear out the address struct
	portNumber = atoi(argv[1]); 									// Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; 							// Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); 					// Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; 					// Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); 				// Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 5); 												// Flip the socket on - it can now receive up to 5 connections

	// Setup signals for SIGCHLD
	setupSignals();

	// Variables for encryption
	char readBuffer[READ_SIZE];
	char origin;
	int keySize, textSize;

	// Dynamic arrays
	char* plaintext;
	char* keytext;
	char* enctext;	

	// Run server forever
	while(1){
		if(numChildren < MAX_FORKS){
			// Accept a connection, blocking if one is not available until one connects	
			sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
			establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
			if (establishedConnectionFD < 0) error("ERROR on accept");		

				// Spawn child process and increase child count
				numChildren += 1;
				
				pid_t spawnPid = -5;
				spawnPid = fork();	
		
				switch(spawnPid){
					// Error
					case -1:
						perror("Spawning fork went wrong!\n");
						exit(1);
						break;
	
					// Child process
					case 0:	
						// Get plaintext size, key size, and origin from client				
						getHeaderInfo(readBuffer, establishedConnectionFD, charsRead, &textSize, &keySize, &origin);

						// Check if origin is from otp_enc
						if(origin == '!'){
							plaintext = (char*)calloc(textSize, sizeof(char));
							keytext = (char*)calloc(keySize, sizeof(char));
							getText(establishedConnectionFD, plaintext, keytext, textSize, keySize);
						
							// Encrypt plaintext with keytext
							enctext = (char*)calloc(textSize, sizeof(char));
							encryptText(enctext, plaintext, keytext, textSize);
							
							// Send a Success message back to the client
							charsRead = send(establishedConnectionFD, enctext, textSize, 0);
							if (charsRead < 0){ error("ERROR writing to socket"); }
					
							// Close the existing socket which is connected to the client
							close(establishedConnectionFD);
				
							// Free dynamic memory
							free(plaintext);
							free(keytext);
							free(enctext);
						}
						else{
							fprintf(stderr,"SERVER ERROR: Connection not from otp_enc.\n");
							charsRead = send(establishedConnectionFD, "ERROR: Connection not from otp_enc.", 35, 0);
							close(establishedConnectionFD);
						}
						// Exit child process
						exit(0);
						break;
					
					// Parent process
					default:
						childPids[numChildren-1] = spawnPid;		
						break;
				}	
		}
		else{
			// We have more than 5 children, wait for one to finish before continuing
			returnPid = wait(NULL);	
			removePid(returnPid);
		}
	}
	// Close the listening socket
	close(listenSocketFD);
	
	return 0; 
}

/***********************
 * Remove a single passed in pid from the global childPids array.
 * This is almost exclusively used after the wait() call
 ***********************/
void removePid(int pid){
	for(int i = 0; i < numChildren; i++){
		if(childPids[i] == pid){
			for(int j = i; j < numChildren-1; j++){
				childPids[j] = childPids[j+1];
			}
		}
	}
	numChildren -= 1;
}

/*******************
 * Setting up signals to catch SIGCHLD
 *******************/
void setupSignals(){
	struct sigaction sigchild_action = {0};
	sigchild_action.sa_handler = catchSIGCHLD;
	sigchild_action.sa_flags = SA_RESTART;

	sigaction(SIGCHLD, &sigchild_action, NULL);	// Register signal catcher
}

/*******************
 * Any time a child terminates, SIGCHLD will call checkForTerm
 *******************/
void catchSIGCHLD(int signo){
	checkForTerm();
}

/**********************
 * Function checks for termination of a child process. Given an
 * array of ints (childPids) and the number of childPids (count),
 * we'll loop through and check for any child processes that
 * have terminated.
 **********************/
void checkForTerm(){
	int exitStatus;
	int check;
	int tempCount = numChildren;	

	for(int i = 0; i < tempCount; i++){
		check = waitpid(childPids[i], &exitStatus, WNOHANG);

		// If check > 0, process has finished, get exitStatus and print
		if(check > 0){
			// Remove pid from the array, i.e., move down values one slot
			for(int j = i; j < tempCount-1; j++){
				childPids[j] = childPids[j+1];
			}

			numChildren -= 1;
		}
	}
}

void encryptText(char* enctext, char* plaintext, char* keytext, int size){
	int letter1, letter2;

	for(int i = 0; i < size; i++){	
		if(plaintext[i] == ' '){
			enctext[i] = ' ';
		}
		else{
			letter1 = plaintext[i];
			letter2 = keytext[i];
			enctext[i] = ((letter1 + letter2) % 26) + 65;	// +65 to bring the alphabet "up" to 'A' in the ascii table	
		}
	}
}

/*****************************
 * This function reads the message from the client
 * and splits up said message into the corresponding plain text
 * and key text
 *****************************/
void getText(int establishedConnectionFD, char* plainText, char* keyText, int tSize, int kSize){
	memset(plainText, '\0', tSize);
	memset(keyText, '\0', kSize);

	char tempBuffer[kSize + tSize];
	int charsRead;

	charsRead = recv(establishedConnectionFD, tempBuffer, kSize+tSize, 0);	// Read client's message from socket

	// Check for errors
	if(charsRead < 0){ error("ERROR reading from socket"); }
	if(charsRead == 0){ error("ERROR return chars == 0, maybe shutdown happened on client.\n"); };

	// Copy correct text from tempBuffer into plainText
	for(int i = 0; i < tSize; i++){
		plainText[i] = tempBuffer[i];
	}

	// Copy correct text from tempBuffer into keyText
	for(int i = 0; i < kSize; i++){
		keyText[i] = tempBuffer[i + tSize];
	}	
}

/*****************************
 * This function will read the header of the incoming message from the client. The header is formatted as follows:
 * message[0] = origin. '!' if from otp_enc, ' ' if from otp_dec
 * message[1 - 10] = text form of the number of characters in the plaintext file
 * message[11 - 20] = text form of the number of characters in the key file
 *****************************/
void getHeaderInfo(char* readBuffer, int establishedConnectionFD, int charsRead, int* textSize, int* keySize, char* origin){
	// Clear out buffers
	memset(readBuffer, '\0', READ_SIZE);

	charsRead = recv(establishedConnectionFD, readBuffer, READ_SIZE, 0); 	// Read the client's message from the socket	
	
	// Create tSize and kSize character arrays that represent the integer and origin
	char tSize[10];
	char kSize[10];

	// Clear out t and k size arrays
	memset(tSize, '\0', sizeof(tSize));
	memset(kSize, '\0', sizeof(kSize));	

	for(int i = 0; i < 10; i++){
		tSize[i] = readBuffer[i+1];
		kSize[i] = readBuffer[i+11];
	}
	
	*origin = readBuffer[0];
	*textSize = atoi(tSize);
	*keySize = atoi(kSize);
}
