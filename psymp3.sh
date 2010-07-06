#!/bin/sh
# 
# Launcher for PsyMP3
#

if [ -e ./libfmod.so ]; then
	echo "FMOD's shared library (libfmod.so) is missing! Cannot continue."
	exit 1;
fi

LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:.

./psymp3 "$@" 
