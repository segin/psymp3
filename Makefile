#
# PsyMP3 master Makefile
# Linux-specific.
#

FBC	= fbc
CXX	= g++
CC	= gcc

QT4_CFLAGS	= `pkg-config QtGui --cflags` 
QT4_LDFLAGS	= `pkg-config QtGui --libs`

psymp3: libmd5 libui psymp3.bas
	$(FBC) -g psymp3.bas psymp3_icon.xpm -Wl -rpath,.

libui: libui.so

libmd5: libmd5.so 

libmd5.so: libs/util/md5.c
	cd libs/util && exec make libmd5
	mv libs/util/libmd5.dll libmd5.so
	
libui.so: libui-qt4.cpp
	$(CXX) libui-qt4.cpp -o libui.so -shared -D_LIBUI_QT4 $(QT4_CFLAGS) $(QT4_LDFLAGS)
