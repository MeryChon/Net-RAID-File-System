#include "net_raid_client.h"
#include <assert.h>
#include <string.h>

static void parse_client_info(char* line);
static char* substring(char* string, int start, int end);
static char* chop_newline(char* string);

static void parse_client_info(char* line) {
	const char* delims = " =";
	int is_value = FALSE;

	char* key = strtok(line, delims);
	assert(key != NULL);
	char* value = strtok(NULL, delims);
	assert(value!=NULL);
	if(strcmp(key, "errorlog") == 0) {

		client_info.errorlog_path = malloc((strlen(value) + 1)*sizeof(char));
		assert(client_info.errorlog_path!=NULL);
		char* final_value = strtok(value, "\"");  //remove quotes
		strcpy(client_info.errorlog_path, final_value);

	} else if(strcmp(key, "cache_size") == 0) {

		client_info.cache_size = atoi(value); //need to check if value was a number

	} else if(strcmp(key, "cache_replacement") == 0) {

		client_info.cache_replacement_algorithm = malloc((strlen(value) + 1)*sizeof(char));
		assert(client_info.cache_replacement_algorithm!=NULL);
		strcpy(client_info.cache_replacement_algorithm,value);

	} else if(strcmp(key, "timeout") == 0) {

		client_info.timeout = atoi(value); //need to check if value was a number
	}
}



static char* chop_newline(char* string) {
	char last = string[strlen(string) - 1];
	char* res = malloc(strlen(string)); //free
	assert(res != NULL);
	if(last == '\n') {
		strncpy(res, string, strlen(string) - 1);
	}
	// printf("%d,  %d\n", strlen(string), strlen(res));
	return res;
}



/* Includes both string[start] and string[end] */
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



static void parse_config_file(const char* config_file_path){
	FILE* config_file = fopen(config_file_path, "r");
	if(config_file) {
		char* curr_line = malloc(sizeof(char)*MAX_LINE_LENGTH);  //free
		assert(curr_line != NULL);

		size_t n = (size_t)MAX_LINE_LENGTH;
		ssize_t bytes_read = 0;
		while(1) {
			ssize_t curr_bytes = getline(&curr_line, &n, config_file);
			if(curr_bytes < 1 || strcmp(curr_line, "\n") == 0)
				break;
			bytes_read += curr_bytes;
			parse_client_info(chop_newline(curr_line));

		}

		// fseek();
		printf("%s  %d  %s  %d\n", client_info.errorlog_path, client_info.cache_size, client_info.cache_replacement_algorithm, client_info.timeout);

		fclose(config_file);
	}
}



int main(int argc, char const *argv[]) {
	const char* config_file_path = argv[1];
	parse_config_file(config_file_path);
	return 0;
}