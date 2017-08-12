#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include <stdbool.h>

typedef struct HashEntry {
  char *key;
  void *data;
} HashEntry;


typedef struct HashTable {
  size_t count, size;
  HashEntry **entries;
} HashTable;


/*
 * HashEntry_create:
 *  Create an entry for insertion into a hash table.
 *
 * Arguments:
 *  key*: string to use for lookup.
 *  data*: pointer to data to associate with key
 *
 *  *: Only pointer copies of these fields are made, must exist
 *    for the life time of the table. That being said, these
 *    values are also not free'd by HashTable_destroy.
 *
 * Returns:
 *  Pointer to new populated entry instance. NULL if entry
 *  creation fails in any way.
 */
HashEntry *HashEntry_create(char *key, void *data);

void HashEntry_destroy(HashEntry *data);
/*
 * HashTable_init
 *  Initialize a hash table instance.
 *
 * Arguments:
 *  size: initial number of elements to allocate in hash table.
 *
 * Returns:
 *  A pointer to a HashEntry table instance. NULL if any error occured
 *  in initialization.
 */
HashTable *HashTable_init(size_t size);

/*
 * HashTable_destroy:
 *  Free all memory allocated by a hash table instance.
 *  Does not free key pointers nor pointers used in entry
 *  data.
 *
 * Arguments:
 *  table: HashEntry table to destroy
 *
 */
void HashTable_destroy(HashTable *table);

/*
 * HashEntry_getEntry:
 *  Get a pointer to the entrys position in the hash table.
 *
 * Arguments:
 *  table: HashEntry table instance to search in
 *  key: Key to look up entry with
 *
 * Returns:
 *  A pointer to a location in the hash table where the
 *  entry should reside. Does not return the entry itself.
 *  To get the entry from this location, one just needs to
 *  dereference the return value, or use HashTable_find instead.
 */
HashEntry **HashTable_getEntry(HashTable *table, char *key);

/*
 * HashTable_add:
 *  Add entry to hash table.
 *
 * Arguments:
 *  table: HashEntry table to add entry into
 *  data: entry to add into hash table.
 *
 * Returns:
 *  Location in HashEntry Table in which the entry was added to.
 *  NULL if no table or entry arguments given.
 */
HashEntry **HashTable_add(HashTable *table, HashEntry *data);


/*
 * HashTable_find:
 *  A wrapper for HashTable_getEntry. Retreive a entry
 *  from a hash table.
 *
 * Arguments:
 *  table: HashEntry table to search for entry in
 *  key: Key to look up entry with
 *
 * Returns:
 *  A pointer to a entry instance.
 */
HashEntry *HashTable_find(HashTable *table, char *key);

HashEntry *HashTable_rm(HashTable *table, HashEntry *data);


int HashTable_forEach(HashTable *table, void *data, int (*fn) (HashEntry *, void *));

#endif

