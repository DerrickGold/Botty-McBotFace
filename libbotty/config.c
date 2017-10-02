#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "jsmn/jsmn.h"
#include "config.h"

/*
 * A small example of jsmn parsing when JSON structure is known and number of
 * tokens is predictable.
 */

static const char *JSON_STRING =
	"{\"user\": \"johndoe\", \"admin\": false, \"uid\": 1000,\n  "
	"\"groups\": [\"users\", \"wheel\", \"audio\", \"video\"]}";

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

static int json_getstr(jsmntok_t *tokens, int tok, char *filebuf, char *outbuf, size_t outsize) {
	if (!tokens) {
		syslog(LOG_CRIT, "json_getstr: NULL tokens buffer");
		return -1;
	}
	jsmntok_t *value = &tokens[tok + 1];

	size_t tokSize = value->end - value->start;
	if (tokSize > outsize) {
		syslog(LOG_ERR, "json_getstr: token size is larger than output buffer. Result will be trimmed.");

		tokSize = outsize - 1;
	}
	snprintf(outbuf, tokSize, "%s", filebuf + value->start);
	syslog(LOG_INFO, "json_getstr: read value: %s", outbuf);
	return 0;
}

int test_json() {
	int i;
	int r;
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */
	char outbuf[512];

	jsmn_init(&p);
	r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return 1;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return 1;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(JSON_STRING, &t[i], "user") == 0) {
			/* We may use strndup() to fetch string value */
			//printf("- User: %.*s\n", t[i+1].end-t[i+1].start,
			//		JSON_STRING + t[i+1].start);
			json_getstr(t, i, JSON_STRING, outbuf, 512);
			printf("- User: %s\n", outbuf);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], "admin") == 0) {
			/* We may additionally check if the value is either "true" or "false" */
			printf("- Admin: %.*s\n", t[i+1].end-t[i+1].start,
					JSON_STRING + t[i+1].start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], "uid") == 0) {
			/* We may want to do strtol() here to get numeric value */
			printf("- UID: %.*s\n", t[i+1].end-t[i+1].start,
					JSON_STRING + t[i+1].start);
			i++;
		} else if (jsoneq(JSON_STRING, &t[i], "groups") == 0) {
			int j;
			printf("- Groups:\n");
			if (t[i+1].type != JSMN_ARRAY) {
				continue; /* We expect groups to be an array of strings */
			}
			for (j = 0; j < t[i+1].size; j++) {
				jsmntok_t *g = &t[i+j+2];
				printf("  * %.*s\n", g->end - g->start, JSON_STRING + g->start);
			}
			i += t[i+1].size + 1;
		} else {
			printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
					JSON_STRING + t[i].start);
		}
	}
	return EXIT_SUCCESS;
}


int botty_loadConfig(BotInfo *bot, char *configPath) {
/*
BotInfo botInfo = {
  .info     = &(IrcInfo) {
    .port     = "6697",
    .server   = "irc.CHANGEME.net",
    .channel  = {"#CHANGEME", "\0", "\0", "\0", "\0"}
  },
  .host     = "CIRCBotHost",
  .nick     = {"DiceBot", "DrawBot", "CIrcBot3"},
  .ident    = "CIrcBot",
  .realname = "Botty McBotFace",
  .master   = "Derrick",
  .useSSL   = 1
};
*/
	return 0;
}
