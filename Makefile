SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g -std=gnu99
LDLIBS=-lm

#include libbotty into the project
BOTTYDIR=libbotty
CFLAGS+=-I $(BOTTYDIR)
LDLIBS+= $(BOTTYDIR)/botty.a

.PHONY: all clean

all: samplebot
samplebot: samplebot.o 
samplebot.o: samplebot.c
clean:
	$(RM) *.o samplebot
