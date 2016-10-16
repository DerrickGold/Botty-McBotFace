SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g -std=c99

.PHONY: all clean

all: botty

botty: botty.o commands.o callback.o ircmsg.o connection.o irc.o

botty.o: botty.c globals.h commands.h
commands.o: commands.c commands.h globals.h
callback.o: callback.c callback.h globals.h ircmsg.h
ircmsg.o: ircmsg.c ircmsg.h globals.h commands.h
connection.o: connection.c connection.h
irc.o: irc.c irc.h ircmsg.h commands.h callback.h connection.h globals.h

clean:
	$(RM) *.o botty
