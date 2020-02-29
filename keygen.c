#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#define NUM_KEY_CHARS 27

int main(int argc, char** argv){

	// Init random seed
	srand(time(0));

	int randNum;
	int lowerLimit = 65;	// 65 == 'A' in ASCII
	int chars[NUM_KEY_CHARS];
	int keyLen;

	// Not enough arguments
	if(argc < 2){
		fprintf(stderr, "ERROR: Not enough arguments\n");
		return 1;
	}	

	// Convert argument one (the length of the key) to an int
	keyLen = atoi(argv[1]);

	if(keyLen < 1){
		fprintf(stderr, "ERROR: Key length less than 1.\n");
		return 1;
	}

	// Populate chars array with the characters we want in the keygen: A-Z, and ' '
	for(int i = 0; i < NUM_KEY_CHARS-1; i++){	// -1 so we save a space for ' '
		chars[i] = lowerLimit + i;
	}
	chars[NUM_KEY_CHARS-1] = ' ';	

	// Generate random string of letters with specified length FROM our chars array
	for(int i = 0; i < keyLen; i++){
		randNum = (rand() % (NUM_KEY_CHARS));
		printf("%c", chars[randNum]);
	}
	printf("\n");

	return 0;
}
