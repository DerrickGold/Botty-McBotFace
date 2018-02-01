#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "nicklist.h"
#include "hash.h"


static char *sanitizeNick(char *nick) {
	size_t diff = strcspn(nick, ILLEGAL_NICK_CHARS);
  nick += (diff == 0);
  return nick;
}

static HashEntry *getNicksForChannel(ChannelNickLists *allNickLists, char *channel) {
	syslog(LOG_DEBUG, "%s: searching nick lists for channel: %s", __FUNCTION__, channel);
	return HashTable_find(allNickLists->channelHash, channel);
}

static NickListEntry *makeNickListEntry(char *nick) {
	NickListEntry *newNick = calloc(1, sizeof(NickListEntry));
  if (!newNick) {
    syslog(LOG_ERR, "%s: Error creating NickListEntry for nick %s: %s",
    	__FUNCTION__, nick, strerror(errno));
    return NULL;
  }

  nick = sanitizeNick(nick);
  strncpy(newNick->nick, nick, MAX_NICK_LEN);
  syslog(LOG_DEBUG, "%s: New entry: %s", __FUNCTION__, nick);

  return newNick;
}

static int createListForChannel(ChannelNickLists *allNickLists, char *channel) {

	if (getNicksForChannel(allNickLists, channel)) {
		syslog(LOG_WARNING, "%s: attempted to create a nick list that already exists: %s",
		 __FUNCTION__, channel);
		return -1;
	}

	char *hashKey = strdup(channel);
	if (!hashKey) {
		syslog(LOG_CRIT, "Error allocating channel as nick list hash key.");
		return -1;
	}

	HashEntry *channelList = HashEntry_create(hashKey, NULL);
	HashEntry **inserted = HashTable_add(allNickLists->channelHash, channelList);

	syslog(LOG_INFO, "%s: Inserted %s into channel nick lists: status: %s",
		__FUNCTION__, hashKey, (inserted != NULL) ? "true" : "false");

	return !(inserted != NULL);
}

static int addNickToChannelHash(HashEntry *channelHash, char *nick) {

	if (!channelHash) {
		syslog(LOG_CRIT, "%s: ChannelHash is null.", __FUNCTION__);
		return -1;
	}

	NickListEntry	*newNick = makeNickListEntry(nick);
	if (!newNick)
		return -1;

	syslog(LOG_DEBUG, "%s: Adding nick (%s) to channel...", __FUNCTION__, nick);
	NickListEntry *curNick = (NickListEntry *)channelHash->data;
  if (!curNick) {
    //first name
    syslog(LOG_DEBUG, "%s: Adding nick as first entry in channel hash", __FUNCTION__);
    channelHash->data = (void *)newNick;
    return -1;
  }

  syslog(LOG_DEBUG, "%s: iterating til end of nick list...", __FUNCTION__);
  while (curNick->next) curNick = curNick->next;
  curNick->next = newNick;
  syslog(LOG_DEBUG, "%s: inserted %s into channel nick hash", __FUNCTION__, nick);
  return 0;
}

static int rmNickFromChannelHash(HashEntry *channelHash, char *nick) {

	if (!channelHash) {
		syslog(LOG_CRIT, "%s: ChannelHash is null.", __FUNCTION__);
		return -1;
	}

	NickListEntry *curNick = (NickListEntry *)channelHash->data;
  NickListEntry *lastNick = curNick;

  nick = sanitizeNick(nick);

  while (curNick && strncmp(curNick->nick, nick, MAX_NICK_LEN)) {
    lastNick = curNick;
    curNick = curNick->next;
  }

  //make sure the node we stopped on is the right one
  if (curNick && !strncmp(curNick->nick, nick, MAX_NICK_LEN)) {
    if ((NickListEntry *)channelHash->data == curNick) channelHash->data = (void *)curNick->next;
    else lastNick->next = curNick->next;
    free(curNick);
  } else {
    syslog(LOG_WARNING, "%s: Failed to remove \'%s\' from nick list, does not exist",
    __FUNCTION__, nick);
    return -1;
  }

  return 0;
}

int NickLists_addNickToChannel(ChannelNickLists *allNickLists, char *channel, char *nick) {

	HashEntry *channelHash = getNicksForChannel(allNickLists, channel);
	if (!channelHash) {
		syslog(LOG_DEBUG, "%s: Channel %s does not exist, creating hash.", __FUNCTION__, channel);
		if (!createListForChannel(allNickLists, channel)) {
			syslog(LOG_DEBUG, "%s: created hash for channel: %s", __FUNCTION__, channel);
			channelHash = getNicksForChannel(allNickLists, channel);
			if (!channelHash) {
				syslog(LOG_CRIT, "%s: Failed to create a channel that did not previously exist!: %s",
					__FUNCTION__, channel);

				return -1;
			}
		}
	}

	return addNickToChannelHash(channelHash, nick);
}

static int rmNickFromAllChannels(HashEntry *entry, void *data) {
	char *name = (char *)data;
	syslog(LOG_DEBUG, "Checking channel hash: %s for %s", entry->key, name);
	rmNickFromChannelHash(entry, name);
	return 0;
}

void NickLists_rmNickFromAll(ChannelNickLists *allNickLists, char *nick) {
	syslog(LOG_INFO, "%s: Purging nick from all channels, they  have disconnected", __FUNCTION__);
	HashTable_forEach(allNickLists->channelHash, (void *)nick, &rmNickFromAllChannels);
	syslog(LOG_INFO, "%s: Finished purging nick from all channels: %s", __FUNCTION__, nick);
}

void NickLists_rmNickFromChannel(ChannelNickLists *allNickLists, char *channel, char *nick) {
	HashEntry *channelHash = getNicksForChannel(allNickLists, channel);
	if (!channelHash) {
		syslog(LOG_WARNING, "%s: Failed to remove nick from channel list. Channel %s does not exist.",
			__FUNCTION__, channel);
		return;
	}

	rmNickFromChannelHash(channelHash, nick);
}

int NickLists_init(ChannelNickLists *allNickLists) {
	syslog(LOG_DEBUG, "%s: initializing nick list hashes...", __FUNCTION__);
	if (!allNickLists) {
		syslog(LOG_ERR, "%s: Failed to initialize NickLists. Null ptr.", __FUNCTION__);
		return -1;
	}
	allNickLists->channelHash = HashTable_init(CHANNICKS_HASH_SIZE);
  if (!allNickLists->channelHash) {
  	syslog(LOG_CRIT, "%s: Error initializing hash table for channel nick lists", __FUNCTION__);
  	return -1;
  }

  return 0;
}


static int purgeNameList(NickListEntry *list) {
	NickListEntry *curNick = list, *next;
  while (curNick) {
    next = curNick->next;
    free(curNick);
    curNick = next;
  }
  return 0;
}

static int clearHashedNickList(HashEntry *entry, void *data) {
	purgeNameList(entry->data);
	free(entry->key);
	entry->key = NULL;
	return 0;
}

void NickList_cleanupAllNickLists(ChannelNickLists *allNickLists) {
	HashTable_forEach(allNickLists->channelHash, NULL, clearHashedNickList);
	HashTable_destroy(allNickLists->channelHash);
	allNickLists->channelHash = NULL;
}


void NickList_forEachNickInChannel(ChannelNickLists *allNickLists, char *channel,
	void *d, void (*fn) (NickListEntry *nick, void *data))
{
	HashEntry *channelHash = getNicksForChannel(allNickLists, channel);
	if (!channelHash) {
		syslog(LOG_CRIT, "%s: Error iterating through nicks in channel %s, no nicks registered here.",
			__FUNCTION__, channel);
		return;
	}

  NickListEntry *curNick = channelHash->data;
  while (curNick) {
    if (fn) fn(curNick, d);
    curNick = curNick->next;
  }
}
