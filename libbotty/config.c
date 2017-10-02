#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include "config.h"

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

static int json_getstr(jsmntok_t *tokens, char *filebuf, char *outbuf, size_t outsize) {
	if (!tokens) {
		syslog(LOG_CRIT, "json_getstr: NULL tokens buffer");
		return -1;
	}
	jsmntok_t *value = (tokens + 1);

	size_t tokSize = (value->end - value->start) + 1;
	if (tokSize > outsize) {
		syslog(LOG_ERR, "json_getstr: token size is larger than output buffer. Result will be trimmed.");

		tokSize = outsize - 1;
	}
	snprintf(outbuf, tokSize, "%s", filebuf + value->start);
	//syslog(LOG_INFO, "json_getstr: read value: %s", outbuf);
	return 0;
}


static char isArray(jsmntok_t *tok) {
	jsmntok_t *arrayTok = tok + 1;
	return arrayTok->type == JSMN_ARRAY;
}

static size_t arrayLen(jsmntok_t *tok) {
	jsmntok_t *arrayTok = tok + 1;
	return arrayTok->size;
}

static jsmntok_t *getArrayEntry(jsmntok_t *arrayTok, int arrayIndex) {
	jsmntok_t *array = arrayTok + 1;
	return &array[arrayIndex];
}

static char *getConfigBuffer(char *configPath, size_t *datalen) {
	FILE *f = NULL;
	char *jsonBuffer = NULL;
	struct stat st = {};

	if (stat(configPath, &st)) {
		syslog(LOG_CRIT, "botty_loadConfig: Error getting file stat: %s -> errno: %s", configPath, strerror(errno));
		return NULL;
	}
	*datalen = st.st_size;

	if (!(f = fopen(configPath, "r"))) {
		syslog(LOG_CRIT, "botty_loadConfig: Failed to open config file: %s: %s", configPath, strerror(errno));
		return NULL;
	}

	jsonBuffer = calloc(1, *datalen);
	if (!jsonBuffer) {
		syslog(LOG_CRIT, "botty_loadConfig: Error allocating memory for config file: %s", strerror(errno));
		fclose(f);
		return NULL;
	}

	fread(jsonBuffer, 1, *datalen, f);
	if (errno) {
		syslog(LOG_CRIT, "botty_loadConfig: Error reading config file: %s", strerror(errno));
		return NULL;
	}
	fclose(f);

	return jsonBuffer;
}

int botty_loadConfig(BotInfo *bot, char *configPath) {
	if (!bot) {
		syslog(LOG_CRIT, "botty_loadConfig: Missing BotInfo object.");
		return -1;
	}
	else if (!bot->info) {
		syslog(LOG_CRIT, "botty_loadConfig: BotInfo->info is NULL. Please assign/allocate IrcInfo struct.");
		return -1;
	}

	size_t jsonLen = 0;
	char *jsonBuffer = getConfigBuffer(configPath, &jsonLen);
	if (!jsonBuffer) return -1;


	int i = 0, r = 0;
	jsmn_parser p = {};
	jsmntok_t t[MAX_JSON_TOKENS]; /* We expect no more than 128 tokens */

	jsmn_init(&p);
	r = jsmn_parse(&p, jsonBuffer, jsonLen, t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		syslog(LOG_CRIT, "Failed to parse JSON: %d\n", r);
		goto _json_error;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		syslog(LOG_CRIT, "Error parsing json: expected object.");
		goto _json_error;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		jsmntok_t *jsonTok = &t[i];
		//PORT
		if (jsoneq(jsonBuffer, jsonTok, "port") == 0) {
			json_getstr(jsonTok, jsonBuffer, bot->info->port, MAX_PORT_LEN);
			syslog(LOG_INFO, "botty_loadConfig: PORT: %s", bot->info->port);
			i++;
		}
		//SERVER
		else if (jsoneq(jsonBuffer, jsonTok, "server") == 0) {
			json_getstr(jsonTok, jsonBuffer, bot->info->server, MAX_SERV_LEN);
			syslog(LOG_INFO, "botty_loadConfig: SERVER: %s", bot->info->server);
			i++;
		}
		//CHANNEL
		else if (jsoneq(jsonBuffer, jsonTok, "channel") == 0) {
			if (!isArray(jsonTok)) {
				syslog(LOG_CRIT, "botty_loadConfig: 'channel' property is not an array and should be an array.");
				goto _json_error;
			}

			size_t chanCount = arrayLen(jsonTok);
			for (int chan = 0; chan < MAX_CONNECTED_CHANS; chan++) {
				if (chan < chanCount) {
					jsmntok_t *channel = getArrayEntry(jsonTok, chan);
					json_getstr(channel, jsonBuffer, bot->info->channel[chan], MAX_CHAN_LEN);
					syslog(LOG_CRIT, "botty_loadConfig: CHANNEL: %s", bot->info->channel[chan]);
				}
				else
					snprintf(bot->info->channel[chan], MAX_CHAN_LEN, "");
			}
			i += chanCount + 1;
		}
		//HOST
		else if (jsoneq(jsonBuffer, jsonTok, "host") == 0) {
			json_getstr(jsonTok, jsonBuffer, bot->host, MAX_HOST_LEN);
			syslog(LOG_INFO, "botty_loadConfig: HOST: %s", bot->host);
			i++;
		}
		//NICK
		else if (jsoneq(jsonBuffer, jsonTok, "nick") == 0) {
			if (!isArray(jsonTok)) {
				syslog(LOG_CRIT, "botty_loadConfig: 'nick' property is not an array and should be an array.");
				goto _json_error;
			}

			size_t nickCount = arrayLen(jsonTok);
			for (int n = 0; n < nickCount && n < NICK_ATTEMPTS; n++) {
				jsmntok_t *nickEntry = getArrayEntry(jsonTok, n);
				json_getstr(nickEntry, jsonBuffer, bot->nick[n], MAX_CHAN_LEN);
				syslog(LOG_CRIT, "botty_loadConfig: NICK[%d]: %s", n, bot->nick[n]);
			}
			i += nickCount + 1;
		}
		//IDENT
		else if (jsoneq(jsonBuffer, jsonTok, "ident") == 0) {
			json_getstr(jsonTok, jsonBuffer, bot->ident, MAX_IDENT_LEN);
			syslog(LOG_INFO, "botty_loadConfig: IDENT: %s", bot->ident);
			i++;
		}
		//REALNAME
		else if (jsoneq(jsonBuffer, jsonTok, "realname") == 0) {
			json_getstr(jsonTok, jsonBuffer, bot->realname, MAX_REALNAME_LEN);
			syslog(LOG_INFO, "botty_loadConfig: REALNAME: %s", bot->realname);
			i++;
		}
		//master
		else if (jsoneq(jsonBuffer, jsonTok, "master") == 0) {
			json_getstr(jsonTok, jsonBuffer, bot->master, MAX_NICK_LEN);
			syslog(LOG_INFO, "botty_loadConfig: MASTER: %s", bot->master);
			i++;
		}
		else {
			printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
					jsonBuffer + t[i].start);
		}
	}

	free(jsonBuffer);
	return 0;

	_json_error:
		free(jsonBuffer);
		return -1;
}
