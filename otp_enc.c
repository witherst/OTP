#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define h_addr h_addr_list[0] /* for backward compatibility */
#define ORIGIN '!'		// If ORIGIN = '!', we are coming from otp_enc -- if ORIGIN = ' ', we are coming from otp_dec

void error(const char *msg) { perror(msg); exit(0); } // Error function used for reporting issues

/* argv[0] = otp_enc
 * argv[1] = plaintext
 * argv[2] = key file
 * argv[3] = port
 */

/* Function prototypes */
int checkPlaintext(FILE*, int*);
int getKeySize(FILE*);
int checkSize(int, int);
void populateBuffer(FILE*, int, FILE*, int, char*);

int main(int argc, char *argv[])
{
	int socketFD, portNumber, charsWritten, charsRead;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;	
    
	if (argc < 4) { fprintf(stderr, "USAGE: %s plaintext hostname port\n", argv[0]); exit(0); } // Check usage & args

	// plaintext file vars
	int textSize = 0;
	FILE* plainFP;
	plainFP = fopen(argv[1], "r");

	if(plainFP == NULL){
		fprintf(stderr, "ERROR: plaintext file does not exist or is null.\n");
		fclose(plainFP);
		exit(1);
	}

	// Get keygen file
	FILE* keyFP;
	keyFP = fopen(argv[2], "r");

	if(keyFP == NULL){
		fprintf(stderr, "ERROR: keyfile does not exist or is null.\n");
		fclose(keyFP);
		exit(1);
	}

	// Check validity of plaintext file
	int textResult = checkPlaintext(plainFP, &textSize);
	
	// Check size of key vs. size of plaintext
	int keySize = getKeySize(keyFP);

	if(textResult != 0 || checkSize(textSize, keySize) != 0){ 
		fclose(plainFP);
		fclose(keyFP);
		exit(1);
	}

	// Populate buffer
	char buffer[textSize + keySize + 21 + 1];	// +21 will be: origin number(1) +  plainfile size(char array size of 10) + keyfile size(char array size of 10) + 1 null character
	memset(buffer, '-', sizeof(buffer));
	populateBuffer(keyFP, keySize, plainFP, textSize, buffer);

	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); 								// Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; 						// Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); 				// Store the port number
	serverHostInfo = gethostbyname("localhost"); 				// Convert the machine name into a special form of address

	if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); 				// Create the socket
	if (socketFD < 0) error("CLIENT: ERROR opening socket");
	
	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
		error("CLIENT: ERROR connecting");

	// Send message to server
	charsWritten = send(socketFD, buffer, strlen(buffer), 0);  	// Write to the server 
	if (charsWritten < 0) error("CLIENT: ERROR writing to socket");
	if (charsWritten < strlen(buffer)) printf("CLIENT: WARNING: Not all data written to socket!\n");

	// Get return message from server
	memset(buffer, '\0', sizeof(buffer)); 						// Clear out the buffer again for reuse
	charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0); 	// Read data from the socket, leaving \0 at end
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");
	printf("CLIENT: I received this from the server: \"%s\"\n", buffer);

	close(socketFD); // Close the socket
	return 0;
}

/*********************
 * This function will populate the buffer array with the contents of the plainfile
 * and the key file. Buffer will be a concatenated version of all the information
 * needed to send to the server. The beginning of buffer will also contain information
 * for the server about plainfile size, keyfile size, and the origin location. Value of 0
 * will represent origin from otp_enc, value of 1 will be an origin from otp_dec. 
 * Buffer = origin number + plainfile Size + keyfile size + plainfile + keyfile
 *********************/
void populateBuffer(FILE* keyFP, int keySize, FILE* plainFP, int textSize, char* buffer){

	// Start at beginning of both files again
	rewind(plainFP);
	rewind(keyFP);

	int bufferPos = 0;

	char textSizeC[10];		// Character "string" version of text size (to be copied into buffer)
	char keySizeC[10];		// Character "string" version of text size (to be copied into buffer)

	// Clear textSizeC and keySizeC
	memset(textSizeC, '-', sizeof(textSizeC));
	memset(keySizeC, '-', sizeof(keySizeC));

	// Convert int textSize and keySize into character array
	sprintf(textSizeC, "%d", textSize);
	sprintf(keySizeC, "%d", keySize);	

	// sprintf adds null terminating character at the end, replace with '-'
	textSizeC[strcspn(textSizeC, "\0")] = '-';
	keySizeC[strcspn(keySizeC, "\0")] = '-';

	// Print origin into buffer
	buffer[bufferPos] = ORIGIN;	
	bufferPos += 1;

	// Print number of chars in plainfile
	for(int i = 0; i < 10; i++){
		buffer[bufferPos + i] = textSizeC[i];
	}
	bufferPos += 10;

	// Print number of chars in keyfile
	for(int i = 0; i < 10; i++){
		buffer[bufferPos + i] = keySizeC[i];
	}
	bufferPos += 10;

	// Put contents of plainfile into buffer
	char c = fgetc(plainFP);
	while(c != '\n'){	
		buffer[bufferPos] = c;
		bufferPos += 1;	
		c = fgetc(plainFP);
	}

	// Put contents of keyfile into buffer	
	c = fgetc(keyFP);
	while(c != '\n'){	
		buffer[bufferPos] = c;
		bufferPos += 1;	
		c = fgetc(keyFP);
	}

	// Add null terminator at end of string
	buffer[bufferPos] = '\0';
}

/*********************
 * This function checks text size against the key size. If text size
 * is greater than key size, return an error (1)
 *********************/
int checkSize(int textSize, int keySize){
	if(textSize > keySize){
		fprintf(stderr, "ERROR: plaintext size is greater than keysize.\n");
		return 1;
	}
	return 0;
}

/*********************
 * This function just gets the number of characters
 * in the key file.
 *********************/
int getKeySize(FILE* fp){
	int result = 0;
	char c = fgetc(fp);

	while(c != '\n'){	
		result += 1;
		c = fgetc(fp);
	}
	return result;
}

/*********************
 * This function checks to make sure all the letters
 * in the plaintext file (fp) are A - Z or a ' ' character.
 * Returns 1 if failed, returns 0 if everything is fine.
 *********************/
int checkPlaintext(FILE* fp, int* size){
	char c = fgetc(fp);

	while(c != '\n'){
		if(!(c >= 'A' && c <= 'Z' || c == ' ')){
			fprintf(stderr, "ERROR: bad characters found in plaintext file.\n");
			return 1;
		}	
		*size += 1;
		c = fgetc(fp);
	}
	
	return 0;
}


















