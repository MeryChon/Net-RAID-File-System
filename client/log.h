#ifndef _LOG_
#define _LOG_


char* log_file;

int log_msg(char* msg);
int log_error(char* error_msg);
void set_log_file(char* file_path);


#endif