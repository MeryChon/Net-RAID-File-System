#define FUSE_USE_VERSION 26

#include "net_raid_client.h"
#include <assert.h>
#include <string.h>
#include <time.h>
#include "raid1_fuse.h"





static void parse_client_info(char* line);
static char* substring(char* string, int start, int end);
static char* chop_whitespaces(char* string);

static void parse_client_info(char* line) {
	const char* delims = " =";
	// int is_value = FALSE;

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


static void parse_server_info(char* line, int disk_num) {	
	const char* delims = "=";
	// int is_value = FALSE;
	char* key = chop_whitespaces(strtok(line, delims));
	assert(key != NULL);
	char* value = chop_whitespaces(strtok(NULL, delims));
	assert(value!=NULL);
	
	if(strcmp(key, "diskname") == 0) {
		raids[disk_num].diskname = malloc(MAX_DIR_NAME_LENGTH);
		assert(raids[disk_num].diskname != NULL);
		strcpy(raids[disk_num].diskname, value);
	} else if(strcmp(key, "mountpoint") == 0){
		raids[disk_num].mountpoint = malloc(MAX_DIR_NAME_LENGTH * 8);
		assert(raids[disk_num].mountpoint != NULL);
		strcpy(raids[disk_num].mountpoint, value);
	} else if(strcmp(key, "raid") == 0){
		raids[disk_num].raid = atoi(value); //should check for number
	} else if(strcmp(key, "servers") == 0){
		int i = 0;
		raids[disk_num].servers = malloc(sizeof(char*) * MAX_NUM_SERVERS);
		assert(raids[disk_num].servers != NULL);
		char* server;
		while((server = strtok_r(value, ", ", &value))) {
			raids[disk_num].servers[i] = malloc(20);
			assert(raids[disk_num].servers[i] != NULL);
			strcpy(raids[disk_num].servers[i], server);
			i++;
		}

	} else if(strcmp(key, "hotswap") == 0){
		raids[disk_num].hotswap = value;
	}

}



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
			parse_client_info(chop_whitespaces(curr_line));
		}

		int server_count = 0;
		while(1) {
			ssize_t curr_bytes = getline(&curr_line, &n, config_file);
			if(curr_bytes < 1 || curr_line == NULL)
				break;
			if(strcmp(curr_line, "\n") == 0) {
				server_count++;
				continue;
			}
			bytes_read += curr_bytes;
			parse_server_info(chop_whitespaces(curr_line), server_count);
		}

		fclose(config_file);
	}
}



void log_to_file(const char* log_text) {
	if(log_text != NULL){
		printf("%s\n", client_info.errorlog_path);
		FILE* log_file = fopen(client_info.errorlog_path, "a");
		if(log_file) {
			printf("%s\n", "opened the log file");
			time_t t = time(0);
			struct tm* tm = localtime(&t);
			char time_string [26];
			strftime(time_string, 26, "%Y-%m-%d %H:%M:%S", tm);
			fprintf(log_file, "%s --- %s\n", time_string, log_text);
			fclose(log_file);
		}
	}	
}


int main(int argc, char const *argv[]) {
	const char* config_file_path = argv[1];
	raids = malloc(MAX_NUM_RAIDS * sizeof(struct disk_info));
	assert(raids != NULL); 
	parse_config_file(config_file_path); 
	//if (configfile)
	// char* fuse_args[2];
	// fuse_args[0] = strdup(argv[0]);
	// fuse_args[1] = strdup(raids[0].mountpoint);
	return raid1_fuse_main(argv[0], raids[0].mountpoint);
	// return fuse_main(2, fuse_args, &nrfs_operations, NULL);
}



