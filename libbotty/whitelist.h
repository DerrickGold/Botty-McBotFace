#ifndef __WHITELIST_H__
#define __WHITELIST_H__

#include "hash.h"

int whitelist_init(HashTable **whitelist);
void whitelist_cleanup(HashTable **whitelist);
void whitelist_add(HashTable *whitelist, char *identity);
char whitelist_isAllowed(HashTable *whitelist, char *identity);
void whitelist_remove(HashTable *whitelist, char *identity);


#endif //__WHITELIST_H__
