#ifndef _FILE_HASH_
#define _FILE_HASH_

#include <openssl/sha.h>

int SHA1_wrapper(const char* string, unsigned char result[]);

#endif