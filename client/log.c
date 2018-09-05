#include "log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include "structs.h"


void set_log_file(char* file_path) {
	log_file_path = malloc(strlen(file_path) + 1);
	assert(log_file_path != NULL);
	strcpy(log_file_path, file_path);
}


int log_msg(char* msg) {
	if(msg != NULL){
		FILE* log_file = fopen(log_file_path, "a");
		if(log_file) {
			time_t t = time(0);
			struct tm* tm = localtime(&t);
			char time_string [26];
			strftime(time_string, 26, "%Y-%m-%d %H:%M:%S", tm);
			fprintf(log_file, "%s --- %s\n", time_string, msg);
			fclose(log_file);
		}
	}	
	return 0;
}


int log_connection(char* address) {
	if(address != NULL){
		FILE* log_file = fopen(log_file_path, "a");
		if(log_file) {
			time_t t = time(0);
			struct tm* tm = localtime(&t);
			char time_string [26];
			strftime(time_string, 26, "%Y-%m-%d %H:%M:%S", tm);
			fprintf(log_file, "%s --- %s %s\n", time_string, "Connected to address: ", address);
			fclose(log_file);
		}
	}	
	return 0;
}


int log_error(char* error_msg, int error_num) {
	//TODO: get errno and so on ...
	if(error_msg != NULL){
		FILE* log_file = fopen(log_file_path, "a");
		if(log_file) {
			time_t t = time(0);
			struct tm* tm = localtime(&t);
			char time_string [26];
			strftime(time_string, 26, "%Y-%m-%d %H:%M:%S", tm);
			fprintf(log_file, "%s --- Error: %s %s \n", time_string, error_msg, strerror(error_num));
			fclose(log_file);
		}
	}	
	return 0;
}

void log_server_error(int disk_index, int server_index, int error_num, char* additional_message) {
	FILE* log_file = fopen(log_file_path, "a");
	if(log_file) {
		time_t t = time(0);
		struct tm* tm = localtime(&t);
		char time_string [26];
		char* str_error = malloc(600);
		assert(str_error != NULL);
		if(error_num != 0) {
			strcpy(str_error, strerror(error_num));
		} else {
			strcpy(str_error, "");
		}
		strftime(time_string, 26, "%Y-%m-%d %H:%M:%S", tm);
		fprintf(log_file, "[%s]--- %s %s  %s %s \n", time_string, raids[disk_index].diskname, 
						raids[disk_index].servers[server_index], additional_message, strerror(error_num));
		fclose(log_file);
		free (str_error);
	}
}