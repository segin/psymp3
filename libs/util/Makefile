#
# Makefile for PsyMP3 external libs
#
# Copyright © 2009 Kirn Gill <segin2005@gmail.com>
#

SOEXT=dll
#SOEXT=so.0
CC=gcc
RM=del /y
#RM=rm -f 


all: libdir libmd5

libmd5: libmd5.$(SOEXT)

libmd5.$(SOEXT):
	$(CC) md5.c -shared -o libmd5.$(SOEXT)

libdir: libdir.$(SOEXT)

libdir.$(SOEXT):
	$(CC) dirname.c -shared -o libdir.$(SOEXT)

clean:
	$(RM) libmd5.$(SOEXT)
	$(RM) libdir.$(SOEXT)
