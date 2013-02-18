#!/bin/zsh
#
# This script cross compiles the various dependencies for PsyMP3 2.x
# Currently assumes a Debian-like distro.
# 
# vim: encoding=utf-8 syntax=zsh ts=4 sw=4 ai
#

# First, configuration
BUILDDIR=${HOME}/build
SRCDIR=${BUILDDIR}/src
typeset -A DISTS

DISTS=(
	SDL 

# Magic!
mkdir -p ${BUILDDIR} ${SRCDIR}

