SHELL=/bin/bash
CC=gcc
LDFLAGS=-L/usr/local/opt/openssl/lib -lcrypto -lssl
CFLAGS=-Wall -g -std=gnu99 -I/usr/local/opt/openssl/include -DUSE_OPENSSL
LDLIBS=-lm

JSMNDIR=jsmn
CFLAGS+=-I $(JSMNDIR)

.PHONY: all clean

all: botty.a

#the botty library
botty.a: ircmsg.o commands.o callback.o hash.o irc.o builtin.o botapi.o connection.o botmsgqueues.o \
	botprocqueue.o botinputqueue.o config.o whitelist.o nicklist.o
	ar rcs $@ $^

commands.o: commands.c commands.h globals.h hash.h ircmsg.h cmddata.h
callback.o: callback.c callback.h ircmsg.h globals.h ircmsg.h
ircmsg.o: ircmsg.c ircmsg.h globals.h hash.h
connection.o: connection.c connection.h
irc.o: irc.c irc.h ircmsg.h commands.h callback.h connection.h hash.h globals.h cmddata.h builtin.h \
	botmsgqueues.h botprocqueue.h botinputqueue.h whitelist.h nicklist.h
hash.o: hash.c hash.h
builtin.o: builtin.c builtin.h globals.h hash.h irc.h cmddata.h botprocqueue.h botmsgqueues.h botinputqueue.h
botapi.o: botapi.c botapi.h globals.h hash.h callback.h ircmsg.h commands.h irc.h cmddata.h connection.h
botmsgqueues.o: botmsgqueues.c botmsgqueues.h hash.h connection.h globals.h
botprocqueue.o: botprocqueue.c botprocqueue.h globals.h
botinputqueue.o: botinputqueue.c botinputqueue.h globals.h
config.o: config.c irc.h
whitelist.o: whitelist.c whitelist.h hash.h globals.h
nicklist.o: nicklist.c nicklist.h globals.h hash.h

clean:
	$(RM) *.o *.a
