#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"


int SHA1_wrapper(const char* string, unsigned char result[]) {
	

	SHA1(string, strlen(string), result);	

	return EXIT_SUCCESS;
}

