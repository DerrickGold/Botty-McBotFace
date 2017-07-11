SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g -std=gnu99 -I/usr/local/opt/openssl/include
LDLIBS=-lm
LDFLAGS=-L/usr/local/opt/openssl/lib -lcrypto -lssl

#include libbotty into the project
BOTTYDIR=libbotty
CFLAGS+=-I $(BOTTYDIR)
LDLIBS+= $(BOTTYDIR)/botty.a

.PHONY: all clean

all: samplebot multibot

samplebot: samplebot.o
multibot: multibot.o

clean:
	$(RM) *.o samplebot multibot
