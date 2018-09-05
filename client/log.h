#ifndef _LOG_
#define _LOG_


char* log_file_path;

int log_msg(char* msg);
int log_connection(char* address);
int log_error(char* error_msg, int error_code);
void set_log_file(char* file_path);
void log_server_error(int disk_index, int server_index, int error_num, char* additional_message);


#endif