#ifndef __BOT_MAILBOXES_H__
#define __BOT_MAILBOXES_H__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "botapi.h"

#define MIN_MAIL_BOXES 13

typedef struct Mail {
  time_t sent;
  char from[MAX_NICK_LEN];
  char msg[MAX_MSG_LEN];
  struct Mail *next;
} Mail;

typedef struct MailBox {
  char notified;
  int count;
  Mail *messages;
} MailBox;

void MailBox_readMsg(BotInfo *bot, char *respTarget, char *nick);
int MailBox_saveMsg(char *to, char *from, char *message);
void MailBox_destroyAll(void);
void MailBox_notifyUser(BotInfo *bot, char *channel, char *nick);
void MailBox_resetUserNotification(char *nick);

int botcmd_mail(CmdData *data, char *args[MAX_BOT_ARGS]);
int botcmd_msg(CmdData *data, char *args[MAX_BOT_ARGS]);

#endif //__BOT_MAILBOXES_H__
