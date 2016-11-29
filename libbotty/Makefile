SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g -std=gnu99
LDLIBS=-lm

.PHONY: all clean

all: botty.a

#the botty library
botty.a: commands.o callback.o ircmsg.o connection.o hash.o irc.o builtin.o botapi.o
	ar rcs $@ $^

commands.o: commands.c commands.h globals.h hash.h
callback.o: callback.c callback.h ircmsg.h globals.h ircmsg.h
ircmsg.o: ircmsg.c ircmsg.h globals.h commands.h hash.h
connection.o: connection.c connection.h
irc.o: irc.c irc.h ircmsg.h commands.h callback.h connection.h hash.h globals.h cmddata.h builtin.h
hash.o: hash.c hash.h
builtin.o: builtin.c builtin.h globals.h hash.h irc.h cmddata.h
botapi.o: botapi.c botapi.h globals.h hash.h callback.h ircmsg.h commands.h irc.h cmddata.h

clean:
	$(RM) *.o *.a
