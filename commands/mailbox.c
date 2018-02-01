#include "mailbox.h"

HashTable *mailBoxes = NULL;

//mail command to read any inboxed messages
int botcmd_mail(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *responseTarget = botcmd_builtin_getTarget(data);
  readMail(data->bot, responseTarget, data->msg->nick);
  int left = numMsgs(data->msg->nick);
  botty_say(data->bot, responseTarget, "%s: You have %d message(s) remaining.", data->msg->nick, left);
  return 0;
}


void mailNotify(BotInfo *bot, char *channel, char *nick) {
  //mail notification
  int left = numMsgs(nick);
  if (left > 0) {
     char *notStatus = getNotified(nick);
     if (!notStatus) return;
     if (!*notStatus) {
         botty_say(bot, channel, "%s: You have %d unread message(s). Use '~mail' to view them.", nick, left);
         *notStatus = 1;
     }
  }
}


/*=====================================================
 * Mailbox implementation
 *
 *  HashTable functions are included in libbotty
 *===================================================*/


void cleanupMailBox(MailBox *box) {
  if (!box) return;

  Mail *cur = box->messages, *next = NULL;
  while (cur) {
    next = cur->next;
    free(cur);
    cur = next;
  }

  free(box);
}

static int cleanupHashedBox(HashEntry *entry, void *data) {
  //cleanup user name and stored messages
  if (entry->key) free(entry->key);
  cleanupMailBox((MailBox *)entry->data);
  entry->data = NULL;
  return 0;
}

void destroyAllMailBoxes(void) {
  HashTable_forEach(mailBoxes, NULL, &cleanupHashedBox);
  HashTable_destroy(mailBoxes);
}

int saveMail(char *to, char *from, char *message) {
  //make sure our mail boxes exist
  if (!mailBoxes) {
    mailBoxes = HashTable_init(MIN_MAIL_BOXES);
    if (!mailBoxes) {
      syslog(LOG_CRIT, "ERROR ALLOCATING MAILBOXES");
      return -1;
    }
  }

  //make sure the user has an inbox
  HashEntry *user = HashTable_find(mailBoxes, to);
  if (!user) {
    syslog(LOG_NOTICE, "'%s' does not have a box, creating one...", to);
    MailBox *newBox = calloc(1, sizeof(MailBox));
    if (!newBox) {
      syslog(LOG_CRIT, "error allocating message box for user %s", to);
      return -1;
    }

    char *nick = strdup(to);
    if (!nick) {
      syslog(LOG_CRIT, "error allocating nick for user's inbox");
      return -1;
    }

    HashEntry *newUser = HashEntry_create(nick, newBox);
    if (!newUser) {
      syslog(LOG_CRIT, "Error allocating mailbox for nick: %s", nick);
      free(nick);
      cleanupMailBox(newBox);
      return -1;
    }
    syslog(LOG_NOTICE, "Adding '%s' to mailbox hash", nick);
    if (!HashTable_add(mailBoxes, newUser)) {
      syslog(LOG_CRIT, "Error adding %s's mail box to the hash", nick);
      cleanupHashedBox(newUser, NULL);
      return -1;
    }
    syslog(LOG_DEBUG, "Searching hash for '%s''s box", nick);
    user = HashTable_find(mailBoxes, nick);
    if (!user) {
      syslog(LOG_CRIT, "Fatal error, could not retrieve created user (%s)", nick);
      return -1;
    }
    syslog(LOG_DEBUG, "'%s''s box was found", to);
  } else {
    syslog(LOG_INFO, "%s has a mail box already", to);
  }

  //make the new mail message to store
  Mail *newMail = calloc(1, sizeof(Mail));
  if (!newMail) {
    syslog(LOG_CRIT, "error allocating new mail for nick %s", to);
    return -1;
  }
  syslog(LOG_INFO, "copying message details");
  time(&newMail->sent);
  strncpy(newMail->from, from, MAX_NICK_LEN);
  strncpy(newMail->msg, message, MAX_MSG_LEN);
  newMail->next = NULL;

  //now add the new message to the user's inbox
  MailBox *inbox = (MailBox *)user->data;
  if (inbox->messages == NULL) {
    inbox->count = 1;
    inbox->messages = newMail;
    syslog(LOG_INFO, "First message added to mail box for %s", to);
  } else {
    Mail *curMail = inbox->messages;
    syslog(LOG_INFO, "Adding message to end of inbox...");
    while (curMail->next) curMail = curMail->next;
    curMail->next = newMail;
    inbox->count++;
  }

  syslog(LOG_INFO, "Successfully sent message to %s", to);
  return 0;
}

char *getNotified(char *nick) {
  HashEntry *user = HashTable_find(mailBoxes, nick);
  if (!user) return NULL;

  MailBox *box = (MailBox *)user->data;
  if (!box) return NULL;

  return &box->notified;
}

//get the number of messages available for a user
int numMsgs(char *nick) {
  if (!mailBoxes) {
    syslog(LOG_INFO, "numMsgs: no global mailbox allocated.");
    return 0;
  }

  HashEntry *user = HashTable_find(mailBoxes, nick);
  if (!user) {
    syslog(LOG_INFO, "numMsgs: no user: '%s'", nick);
    return 0;
  }

  MailBox *box = (MailBox *)user->data;
  if (box->count <= 0 || !box->messages) {
    syslog(LOG_INFO, "numMsgs: %s has no mail", nick);
    return 0;
  }

  return box->count;
}

void readMail(BotInfo *bot, char *respTarget, char *nick) {
  syslog(LOG_DEBUG, "readMail: getting message count first");
  int msgCount = numMsgs(nick);
  if (!msgCount) return;

  syslog(LOG_INFO, "readMail: Accessing last message for '%s' of %d", nick, msgCount);
  //if msg count is > 0, then the user exists and has a box with messages
  HashEntry *user = HashTable_find(mailBoxes, nick);
  MailBox *box = (MailBox *)user->data;
  Mail *message = box->messages;
  box->messages = message->next;

  //get sent time
  char buff[32];
  struct tm * timeinfo = localtime(&message->sent);
  strftime(buff, sizeof(buff), "%b %d @ %H:%M", timeinfo);

  syslog(LOG_INFO, "Retrieved stored message: %s: [%s <%s>] %s", nick, buff, message->from, message->msg);
  botty_say(bot, respTarget, "%s: [%s <%s>] %s", nick, buff, message->from, message->msg);

  free(message);
  box->count--;
}
