#include "links.h"

LinksHead ListOfLinks = {};

char *links_msgContainsLink(char *input) {

  char *url = strstr(input, URL_IDENTIFIER_HTTP);
  if (url) return url;

  url = strstr(input, URL_IDENTIFIER_HTTPS);
  if (url) return url;

  url = strstr(input, URL_IDENTIFIER_WWW);
  return url;
}

char links_store(char *input) {
  LinksHead *head = &ListOfLinks;

  char *start = links_msgContainsLink(input);
  if (!head || !start) return 0;

  char *end = start;
  while (*end != ' ' && *end != '\0' && *end != '\n' && *end != '\r') end++;

  LinkNode *newNode = NULL;
  if (head->count < LINKS_STORE_MAX) {
    newNode = calloc(1, sizeof(LinkNode));
  } else {
    newNode = head->head;
    LinkNode *prevNode = newNode;
    while (newNode->next){
      prevNode = newNode;
      newNode = newNode->next;
    }
    prevNode->next = NULL;
  }
  memset(newNode->url, 0, MAX_MSG_LEN);
  strncpy(newNode->url, start, (end - start));
  if (!head->head && head->count == 0) {
    head->head = newNode;
    head->count++;
  } else {
    newNode->next = head->head;
    head->head = newNode;
    head->count += (head->count < LINKS_STORE_MAX);
  }

  return 0;
}

int links_print_process(void *b, char *procOwner, BotProcessArgs *args) {
  BotInfo *bot = (BotInfo *)b;
  LinksHead *listData = (LinksHead *)args->data;
  char *responseTarget = args->target;

  if (!listData->lastPos)
    goto _fin;

  if (botty_say(bot, responseTarget, ". %s", listData->lastPos->url) < 0)
    goto _fin;

  listData->lastPos = listData->lastPos->next;
  //return 1 to keep the process going
  return 1;

  _fin:
  return -1;
}

int links_print(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *caller = data->msg->nick;
  char *responseTarget = botty_respondDest(data);
  char *script = "links";

  if (ListOfLinks.count == 0) {
    botty_say(data->bot, responseTarget, "%s: There is no link history to post.", caller);
    return 0;
  }

  BotProcessArgs *sArgs = botty_makeProcessArgs((void*)&ListOfLinks, responseTarget, NULL);
  if (!sArgs) {
    botty_say(data->bot, responseTarget, "There was an error allocating memory to execute command: %s", script);
    return 0;
  }

  ListOfLinks.lastPos = ListOfLinks.head;
  botty_say(data->bot, responseTarget, "Printing the last %d available chat link(s) in history.", ListOfLinks.count);
  botty_runProcess(data->bot, &links_print_process, sArgs, script, caller);
  return 0;
}

void links_purge(void) {
  LinksHead *list = &ListOfLinks;
  if (!list || !list->head) return;
  LinkNode *current = list->head;
  while (current->next) {
    LinkNode *next = current->next;
    free(current);
    current = next;
  }
  memset(list, 0, sizeof(LinksHead));
}
