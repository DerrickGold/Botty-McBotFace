#ifndef __CHANNEL_NICK_LISTS__
#define __CHANNEL_NICK_LISTS__

#include "globals.h"

typedef struct NickListEntry {
  char nick[MAX_NICK_LEN];
  struct NickListEntry *next;
} NickListEntry;

typedef struct ChannelNickLists {
	HashTable *channelHash;
} ChannelNickLists;

typedef void (*NickListIterator)(NickListEntry *nick, void *data);

int NickLists_addNickToChannel(ChannelNickLists *allNickLists, char *channel, char *nick);
void NickLists_rmNickFromChannel(ChannelNickLists *allNickLists, char *channel, char *nick);
int NickLists_init(ChannelNickLists *allNickLists);
void NickList_cleanupAllNickLists(ChannelNickLists *allNickLists);
void NickList_forEachNickInChannel(ChannelNickLists *allNickLists, char *channel,
	void *d, NickListIterator iterator);
void NickLists_rmNickFromAll(ChannelNickLists *allNickLists, char *nick);
#endif //__CHANNEL_NICK_LISTS__
