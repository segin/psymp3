#!/bin/zsh
#
# This script cross compiles the various dependencies for PsyMP3 2.x
# Currently assumes a Debian-like distro.
# 
# vim: encoding=utf-8 syntax=zsh ts=4 sw=4 ai
#

# Are we really running on zsh? 
if [ -z ${ZSH_VERSION} ]; then
	echo "This script requires zsh!"
	exit 1
fi

# First, configuration
BUILDDIR=${HOME}/build
DISTFILES=${BUILDDIR}/distfiles
typeset -A DISTS
typeset -A DISTVERS

DISTS=(
	zlib		"http://zlib.net/zlib-1.2.7.tar.gz"
	SDL			"http://www.libsdl.org/release/SDL-1.2.15.tar.gz"
	SDL_ttf		"http://www.libsdl.org/projects/SDL_ttf/release/SDL_ttf-2.0.11.tar.gz"
	SDL_gfx		"http://www.ferzkopp.net/Software/SDL_gfx-2.0/SDL_gfx-2.0.24.tar.gz"
	freetype	"http://download.savannah.gnu.org/releases/freetype/freetype-2.4.11.tar.gz"
)

DISTVERS=(
	zlib		"1.2.7"
	SDL			"1.2.15"
	SDL_ttf		"2.0.11"
	SDL_gfx		"2.0.24"
	

# Magic!
mkdir -p ${BUILDDIR} ${SRCDIR}

