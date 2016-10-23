SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g -std=gnu99

.PHONY: all clean

all: botty

botty: botty.o commands.o callback.o ircmsg.o connection.o hash.o irc.o 

botty.o: botty.c hash.h globals.h commands.h callback.h cmddata.h ircmsg.h connection.h irc.h
commands.o: commands.c commands.h globals.h
callback.o: callback.c callback.h ircmsg.h globals.h ircmsg.h
ircmsg.o: ircmsg.c ircmsg.h globals.h commands.h
connection.o: connection.c connection.h
irc.o: irc.c irc.h ircmsg.h commands.h callback.h connection.h hash.h globals.h cmddata.h 
hash.o: hash.c hash.h

clean:
	$(RM) *.o botty
