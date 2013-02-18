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
TUPLE=i686-w64-mingw32
BUILDDIR=${HOME}/build
DISTFILES=${BUILDDIR}/distfiles
typeset -A DISTS
typeset -A DISTVERS
typeset -A DISTMD5
typeset -A DISTSHA256

DISTS=(
	zlib		"http://zlib.net/zlib-1.2.7.tar.gz"
	SDL			"http://www.libsdl.org/release/SDL-1.2.15.tar.gz"
	SDL_ttf		"http://www.libsdl.org/projects/SDL_ttf/release/SDL_ttf-2.0.11.tar.gz"
	SDL_gfx		"http://www.ferzkopp.net/Software/SDL_gfx-2.0/SDL_gfx-2.0.24.tar.gz"
	freetype	"http://download.savannah.gnu.org/releases/freetype/freetype-2.4.11.tar.gz"
	taglib		"http://taglib.github.com/releases/taglib-1.8.tar.gz"
	libvisual	"http://ftp.freebsd.org/pub/FreeBSD/ports/distfiles/libvisual-0.4.0.tar.gz"
	mpg123		"http://mpg123.de/download/mpg123-1.14.4.tar.bz2"
	ogg			"http://downloads.xiph.org/releases/ogg/libogg-1.3.0.tar.gz"
	vorbis		"http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.3.tar.gz"
)

DISTVERS=(
	zlib		"1.2.7"
	SDL			"1.2.15"
	SDL_ttf		"2.0.11"
	SDL_gfx		"2.0.24"
	freetype	"2.4.11"
	taglib		"1.8"
	libvisual	"0.4.0"
	mpg123		"1.14.4"
	ogg			"1.3.0"
	vorbis		"1.3.3"
)

DISTMD5=(
		
)

DISTSHA256=(
)

PACKAGES=( zlib	SDL SDL_ttf SDL_gfx	freetype taglib libvisual mpg123 ogg vorbis )

# Initialize our build environment 
mkdir -p ${BUILDDIR} ${DISTFILES}

notice () { 
	echo "===> " $*
}

distfiles_sha256sum () { 
	
}

distfiles_md5sum () {

}

distfiles_fetch () { 
	package=$1
	if [ -z ${DISTS[$package]} ]; then
		notice "Invalid package " $package
		exit 1
	fi

	notice "Fetching for ${package}-${DISTVERS[$package]}"
	wget -O ${DISTFILES}/$(basename ${DISTS[$package]}) ${DISTS[$package]}
}


# XXX: better system for fetching distfiles needed!!

for i in ${PACKAGES}; do
	wget -O ${DISTFILES}/$(basename ${DISTS[$i]}) ${DISTS[$i]}
done


