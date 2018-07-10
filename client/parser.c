#include "parser.h"
#include <assert.h>
#include <string.h>


static char* substring(char* string, int start, int end);
static char* chop_whitespaces(char* string);


static char* chop_whitespaces(char* str) {
	if(str == NULL)
		return NULL;
	int start = 0;
	int end = strlen(str) - 1;
	while (str[start] == ' ') {
		start++;
	}

	while(str[end] == ' ' || str[end] == '\n') {
		end--;
	}
	char* res = substring(str, start, end+1);
	return res;
}


/* Includes string[start], excludes string[end] */
static char* substring(char* string, int start, int end) {
	char* res = malloc(strlen(string)); //free
	int i=0;
	end = (end < strlen(string)) ? end : (strlen(string));
	while(start + i < end) {
		res[i] = string[start+i];
		i++;
	}
	res[i] = '\0';
	return res;
}