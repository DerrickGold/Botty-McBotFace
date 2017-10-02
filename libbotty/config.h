#ifndef __CONFIG_READING_H__
#define __CONFIG_READING_H__
#include "jsmn/jsmn.h"
#include "botapi.h"

#define MAX_JSON_TOKENS 128

int test_json();

int botty_loadConfig(BotInfo *bot, char *configPath);

#endif //__CONFIG_READING_H__
