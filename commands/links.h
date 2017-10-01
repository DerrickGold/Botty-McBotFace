#ifndef __BOT_LINKHISTORY_H__
#define __BOT_LINKHISTORY_H__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "botapi.h"

#define LINKS_STORE_MAX 5
#define URL_IDENTIFIER_HTTP "http:"
#define URL_IDENTIFIER_HTTPS "https:"
#define URL_IDENTIFIER_WWW "www."


typedef struct LinkNode {
  char url[MAX_MSG_LEN];
  struct LinkNode *next;
} LinkNode;

typedef struct LinksHead {
  LinkNode *head;
  int count;
  LinkNode *lastPos;
} LinksHead;


char *links_msgContainsLink(char *input);
char links_store(char *input);
int links_print(CmdData *data, char *args[MAX_BOT_ARGS]);
void links_purge(void);

#endif //__BOT_LINKHISTORY_H__
