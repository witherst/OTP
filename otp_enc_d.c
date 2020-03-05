#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

#define READ_SIZE 21	// Represents size of the READ buffer that we're reading in

// Function prototypes
void getHeaderInfo(char*, int, int, int*, int*, char*);
void getText(int, char*, char*, int, int);
void encryptText(char*, char*, char*, int);

int main(int argc, char *argv[])
{
	int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
	socklen_t sizeOfClientInfo;
	struct sockaddr_in serverAddress, clientAddress;

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

	// Accept a connection, blocking if one is not available until one connects
	sizeOfClientInfo = sizeof(clientAddress); 								// Get the size of the address for the client that will connect
	establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
	if (establishedConnectionFD < 0) error("ERROR on accept");

	// Get plaintext size, key size, and origin from client
	char readBuffer[READ_SIZE];
	char origin;
	int keySize, textSize;

	getHeaderInfo(readBuffer, establishedConnectionFD, charsRead, &textSize, &keySize, &origin);

	// Get the plaintext and key from the client and display it
	char plaintext[textSize];
	char keytext[keySize];

	getText(establishedConnectionFD, plaintext, keytext, textSize, keySize);

	// Encrypt plaintext with keytext
	char enctext[textSize];
	encryptText(enctext, plaintext, keytext, textSize);
	
	// Send a Success message back to the client
	//charsRead = send(establishedConnectionFD, "I am the server, and I got your message", 39, 0); // Send success back
	charsRead = send(establishedConnectionFD, enctext, textSize, 0);
	if (charsRead < 0) error("ERROR writing to socket");
	close(establishedConnectionFD); 															// Close the existing socket which is connected to the client
	close(listenSocketFD); 																		// Close the listening socket
	
	return 0; 
}

void encryptText(char* enctext, char* plaintext, char* keytext, int size){
	int letter1, letter2;

	for(int i = 0; i < size; i++){
		letter1 = plaintext[i];
		letter2 = keytext[i];
		enctext[i] = ((letter1 + letter2) % 27) + 65;	// +65 to bring the alphabet "up" to 'A' in the ascii table
		//printf("%c + %c = %c\n", plaintext[i], keytext[i], enctext[i]);
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
	
//	printf("sizeof readbuffer: %d\n", sizeof(readBuffer)/sizeof(readBuffer[0]));
//	printf("printing readBuffer:\n");
//	for(int i = 0; i < READ_SIZE; i++){
//		printf("%d ", readBuffer[i]);
//	}
//	printf("\n");
	
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

//	while(strstr(fullBuffer, "@@") == NULL){
//		memset(readBuffer, '\0', sizeof(readBuffer));
//		charsRead = recv(establishedConnectionFD, readBuffer, sizeof(readBuffer) - 1, 0); 	// Read the client's message from the socket
//
//		printf("printing readBuffer:\n");
//		for(int i = 0; i < READ_SIZE; i++){
//			printf("%d ", readBuffer[i]);
//		}
//		printf("\n");
//
//		// Add chunk of readBuffer to fullBuffer
//		strcat(fullBuffer, readBuffer);
//
//		// Check for errors
//		if(charsRead < 0){ error("ERROR reading from socket"); }
//		if(charsRead == 0){
//			printf("Return chars == 0, shutdown happened.\n");
//			break;
//		}
//
//		// Find where to replace '\0'
//		for(int i = 0; i < ARR_SIZE; i++){
//			printf("%d ", fullBuffer[i]);
//		}
//		printf("\n");
//		int index = strstr(fullBuffer, "@@") - fullBuffer;
//		
//		printf("index: %d\n", index);
//		fullBuffer[index] = '\0';
//
//
//	}

}
