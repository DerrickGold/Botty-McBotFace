#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <math.h>
#include "hash.h"

//Limit how many times to rehash an entry before
//resizing the hash table
#define LOOKUP_ATTEMPTS 5

//how large to grow the table each time it is
//automatically resized. As a percentage of its
//current size.
#define GROWTH_RATE 0.21f

static size_t growSize(size_t oldSize) {

  return oldSize  + (size_t)floor(oldSize * GROWTH_RATE);
}


HashEntry *HashEntry_create(char *key, void *data) {
	if (!key) {
		syslog(LOG_WARNING, "Error creating hash entry: NULL key value");
		return NULL;
	}

  HashEntry *entry = calloc(1, sizeof(HashEntry));
  if (!entry) {
    syslog(LOG_CRIT, "Error creating hash data");
    return NULL;
  }

  //set the key to whatever the token was pointing at
  entry->key = key;
  entry->data = data;
  return entry;
}

void HashEntry_destroy(HashEntry *data) {

  if (!data)
    return;

  memset(data, 0, sizeof(HashEntry));
  free(data);
}

static int entryDestroyHelper(HashEntry *entry, void *data) {
  HashEntry_destroy(entry);
  return 0;
}

//djb2 algorithm
//http://www.cse.yorku.ca/~oz/hash.html
size_t hash1(char *key, size_t num, size_t last) {

  size_t hashVal = 5381 + last;
  int c;

  while ((c = *key++) != '\0')
    hashVal = ((hashVal << 5) + hashVal) ^ c; /* hash * 33 + c */


  return hashVal % num;
}


HashEntry **HashTable_getEntry(HashTable *table, char *key) {

  if (!table || !key || !*key)
    return NULL;

  size_t pos = hash1(key, table->size, 0);

  HashEntry **curPos = &table->entries[pos];

  //no table entries allocated?
  if (!curPos)
    return NULL;

  //if first attempt and nothing exists, return the free position
  if (!*curPos)
    return curPos;

  //otherwise, check that we've got the right one
  int attempt = 0;

  do {
    HashEntry *entry = *curPos;

    if (!strcmp(key, entry->key))
      return curPos;

    //pos = (pos + attempt) % table->size;
    pos = hash1(key, table->size, pos + (attempt<<1));
    curPos = &table->entries[pos];
  } while (*curPos != NULL && attempt++ < LOOKUP_ATTEMPTS);

  if (attempt >= LOOKUP_ATTEMPTS)
    return NULL;

  return curPos;
}


int HashTable_copy(HashTable *dest, HashTable *src) {

  //make sure the dest table is at least as large as the source
  if (dest->size < src->size)
    return -1;

  //rehash all entries and copy them over
  for (size_t e = 0; e < src->size; e++) {

    //skip null entries
    if (src->entries[e] == NULL)
      continue;

    //have a valid entry
    HashEntry **pos = HashTable_getEntry(dest, src->entries[e]->key);
    if (!pos) {
      return -1;
    }

    if (*pos == NULL)
      *pos = src->entries[e];
    else {
      return -1;
    }

  }

  return 0;
}

HashTable *HashTable_init(size_t size) {

  if (size <= 0)
    return NULL;

  HashTable *table = calloc(1, sizeof(HashTable));
  if (!table) {
  	syslog(LOG_CRIT, "HashTable_init: Error allocating symbol table");
    return NULL;
  }
  table->size = size;
  table->entries = calloc(size, sizeof(HashEntry*));
  if (!table->entries) {
    syslog(LOG_CRIT, "HashTable_init: Error allocating symtable entries");
    free(table);
    return NULL;
  }

  return table;
}


void HashTable_destroy(HashTable *table) {

  if (!table)
    return;

  if (table->entries) {
    HashTable_forEach(table, NULL, entryDestroyHelper);
    free(table->entries);
  }

  memset(table, 0, sizeof(HashTable));
  free(table);
}


int HashTable_resize(HashTable *table, size_t size) {

  HashTable *newTable = NULL;
  int status = 0;
  do {
    newTable = HashTable_init(size);
    if (!newTable) {
      syslog(LOG_CRIT, "Error resizing table, failed to allocate new table of size: %zu", size);
      return -1;
    }

    status = HashTable_copy(newTable, table);

    //error copying data, resize the table
    if (status) {
      size = growSize(size);
      free(newTable);
    }
  } while (status);

  //then free old table, and assign new table
  free(table->entries);

  //make original table point to new table stuff
  table->entries = newTable->entries;
  table->size = newTable->size;
  free(newTable);
  //and done
  return 0;
}

int HashTable_forEach(HashTable *table, void *data, int (*fn) (HashEntry *, void *)) {

  if (!table || !fn)
    return 0;

  //loop through all symbol table entries
  for (size_t i = 0; i < table->size; i++) {

    //skip entries where key may not be set (malformed entries)
    HashEntry *entry = table->entries[i];

    //skip empty entries
    if (!entry)
      continue;

    int status = fn(entry, data);
    if (status)
      return status;

  } //for each table element

  return 0;
}

HashEntry **HashTable_add(HashTable *table, HashEntry *data) {

  if (!table || !table->entries)
   return NULL;

  //if we somehow managed to fill the table, make it grow
  if (table->count >= table->size)
    HashTable_resize(table, growSize(table->size));


  HashEntry **position = HashTable_getEntry(table, data->key);

  if (!position) {
    HashTable_resize(table, growSize(table->size));
    //try adding the data again
    return HashTable_add(table, data);;

  }
  if (!*position) {
    (*position) = data;
    table->count++;
  }
  //if this statement is false, that means an entry already
  //exists for a given key, just return that data instead
  return position;
}

HashEntry *HashTable_rm(HashTable *table, HashEntry *data) {

  if (!table || !table->entries)
   return NULL;

  HashEntry **position = HashTable_getEntry(table, data->key);
  if (!position) {
    //entry does not exist in hash table
    return NULL;
  }

  HashEntry *toRemove = *position;
  *position = NULL;
  table->count--;

  return toRemove;
}

HashEntry *HashTable_find(HashTable *table, char *key) {

	if (!key) {
		syslog(LOG_WARNING, "Error finding hash entry, key is NULL");
		return NULL;
	}

  HashEntry **position = HashTable_getEntry(table, key);
  if (!position || !*position) {
    return NULL;
  }
  return *position;
}
