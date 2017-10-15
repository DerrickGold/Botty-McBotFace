#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "globals.h"
#include "whitelist.h"

#define STUB_IDENTITY ""

static int cleanWhitelistEntry(HashEntry *entry, void *d) {
	free(entry->key);
	HashEntry_destroy(entry);
	return 0;
}

static int validateArgs(HashTable *whitelist, char *identity, const char *fn) {
		if (!whitelist) {
		syslog(LOG_ERR, "%s: Failed to whitelist user, whitelist pointer is null.", fn);
		return -1;
	}
	else if (!identity) {
		syslog(LOG_ERR, "%s: Failed to whitelist user, identity pointer is null.", fn);
		return -1;
	}

	return 0;
}

static HashEntry *getWhitelistEntry(HashTable *whitelist, char *identity, const char *fn) {
	HashEntry *entry = HashTable_find(whitelist, identity);
	if (!entry) {
		syslog(LOG_INFO, "%s: user '%s' is not whitelisted.", fn, identity);
	} else {
		syslog(LOG_INFO, "%s: user '%s' is whitelisted.", fn, identity);
	}

	return entry;
}

int whitelist_init(HashTable **whitelist) {
	if (!whitelist) {
		syslog(LOG_CRIT, "%s: Null hashtable pointer provided.", __FUNCTION__);
		return -1;
	}

	*whitelist = HashTable_init(WHITELIST_HASH_SIZE);
  if (!*whitelist) {
  	syslog(LOG_CRIT, "%s: Error initializing hash table for channel nick lists", __FUNCTION__);
  	return -1;
  }

  return 0;
}

void whitelist_cleanup(HashTable **whitelist) {
	if (!whitelist) {
		syslog(LOG_WARNING, "%s: Pointer to whitelist pointer is null", __FUNCTION__);
		return;
	}
	else if (validateArgs(*whitelist, STUB_IDENTITY, __FUNCTION__)) return;

	HashTable_forEach(*whitelist, NULL, &cleanWhitelistEntry);
	HashTable_destroy(*whitelist);
	*whitelist = NULL;
}

void whitelist_add(HashTable *whitelist, char *identity) {

	if (validateArgs(whitelist, identity, __FUNCTION__))
		return;

	size_t nickLen = strlen(identity);

	char *key = calloc(1, nickLen + 1);
	if (!key) {
		syslog(LOG_CRIT, "%s: Failed to whitelist user '%s': %s", __FUNCTION__, identity, strerror(errno));
		return;
	}

	HashEntry *whitelistEntry = HashEntry_create(key, NULL);
	if (!whitelistEntry) {
		free(key);
		return;
	}

	if (!HashTable_add(whitelist, whitelistEntry)) {
		syslog(LOG_CRIT, "%s: Failed to whitelist user '%s': Error adding to whitelist hash.", __FUNCTION__, identity);
		cleanWhitelistEntry(whitelistEntry, NULL);
		return;
	}
	syslog(LOG_INFO, "%s: Successfully whitelisted user '%s'.", __FUNCTION__, identity);
}

char whitelist_isAllowed(HashTable *whitelist, char *identity) {
	if (validateArgs(whitelist, identity, __FUNCTION__))
		return 0;

	return (getWhitelistEntry(whitelist, identity, __FUNCTION__) != NULL);
}

void whitelist_remove(HashTable *whitelist, char *identity) {
	if (validateArgs(whitelist, identity, __FUNCTION__))
		return;

	HashEntry *entry = getWhitelistEntry(whitelist, identity, __FUNCTION__);
	if (!entry) {
		syslog(LOG_ERR, "%s: Could not remove '%s' from the whitelist, they didn't exist!", __FUNCTION__, identity);
		return;
	}

	HashEntry *removed = HashTable_rm(whitelist, entry);
	if (!removed) {
		syslog(LOG_CRIT, "%s: Failed to remove '%s' from whitelist.", __FUNCTION__, identity);
		return;
	}
	cleanWhitelistEntry(removed, NULL);
}
