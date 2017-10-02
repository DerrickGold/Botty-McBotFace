SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g -std=gnu99 -I/usr/local/opt/openssl/include
LDLIBS=-lm
LDFLAGS=-L/usr/local/opt/openssl/lib -lcrypto -lssl

#include libbotty into the project
BOTTYDIR=libbotty
CFLAGS+=-I $(BOTTYDIR)
LDLIBS+= $(BOTTYDIR)/botty.a

JSMNDIR=jsmn
CFLAGS+=-I $(BOTTYDIR)/$(JSMNDIR)
LDLIBS+= $(BOTTYDIR)/$(JSMNDIR)/libjsmn.a

CMDDIR=commands
CFLAGS+=-I $(CMDDIR)

.PHONY: all clean

all: samplebot

samplebot: samplebot.o commands/mailbox.o commands/links.o
mailbox.o: commands/mailbox.c commands/mailbox.h
links.o: commands/links.c commands/links.h

clean:
	$(RM) *.o samplebot multibot $(CMDDIR)/*.o
