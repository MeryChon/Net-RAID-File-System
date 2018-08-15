#define FUSE_USE_VERSION 26

#include "net_raid_client.h"
#include "raid1_fuse.h"

#include <assert.h>
#include <string.h>
#include <errno.h>
#include "log.h"

#include<unistd.h>

static void parse_client_info(char* line);
static char* substring(char* string, int start, int end);
static char* chop_whitespaces(char* string);

static void parse_client_info(char* line) {
	const char* delims = " =";

	char* key = strtok(line, delims);

	assert(key != NULL);
	char* value = strtok(NULL, delims);
	assert(value!=NULL);

	if(strcmp(key, "errorlog") == 0) {
		
		char* final_value = strtok(value, "\"");  //remove quotes
		client_info.errorlog_path = strdup(final_value);

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


static void parse_storage_info(char* line, int disk_num) {	
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
		raids[disk_num].num_servers = i;

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

	while(str[end] == ' ' || str[end] == '\n' || str[end] == '\r' || str[end] == '\t') {
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



static int parse_config_file(const char* config_file_path){
	FILE* config_file = fopen(config_file_path, "r");
	if(config_file) {
		char* curr_line = malloc(sizeof(char)*MAX_LINE_LENGTH);  //free
		assert(curr_line != NULL);

		size_t n = (size_t)MAX_LINE_LENGTH;
		ssize_t bytes_read = 0;
		while(1) {
			ssize_t curr_bytes = getline(&curr_line, &n, config_file);
			if(curr_bytes < 1 || strcmp(curr_line, "\r\n") == 0 || strcmp(curr_line, "\n") == 0)
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
			parse_storage_info(chop_whitespaces(curr_line), server_count);
			num_storages++;
		}

		fclose(config_file);
		return 0;
	} else {
		return -1;
	}
}


static int start(const char* program_name) {
	int i = 0;
	for(; i<num_storages; i++) {
		struct disk_info storage_info = raids[i];
		printf("Next disk %d is RAID%d\n", i, storage_info.raid);
		if(storage_info.raid == RAID_1) {
			if(raid1_fuse_main(program_name, client_info, storage_info) < 0) {
				return -1;
			}
		} else {
			printf("%s\n", "RAID5 Not yet implemented, sorry...");
		}
		// switch(fork()) {
  //           case -1:
  //               exit(100);
  //           case 0:
  //           	if(storage_info.raid == RAID_1) {
		// 			return raid1_fuse_main(program_name, client_info, storage_info);
		// 		} else {
		// 			printf("%s\n", "RAID5 Not yet implemented, sorry...");
		// 			exit(0);
		// 		}
  //           default:
  //               continue;	
  //       }	
	}
	return 0;
}


int main(int argc, char const *argv[]) {
	const char* config_file_path = argv[1];
	raids = malloc(MAX_NUM_RAIDS * sizeof(struct disk_info));
	assert(raids != NULL); 
	if(parse_config_file(config_file_path) < 0) {
		printf("%s\n", strerror(errno));
		exit(-1);
	} 
	//if (configfile)
	set_log_file(client_info.errorlog_path);
	return start(argv[0]);	
	// return raid1_fuse_main(argv[0], raids[0].mountpoint);
}



