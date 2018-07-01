#include "net_raid_client.h"




static void parse_config_file(const char* config_file_path){
	FILE* config_file = fopen(config_file_path, "r");
	if(config_file) {
		

		fclose(config_file);
	}
}



int main(int argc, char const *argv[]) {
	const char* config_file_path = argv[1];
	parse_config_file(config_file_path);
	printf("%s\n", config_file_path);
	return 0;
}