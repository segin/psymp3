#!/bin/zsh
# 
# vim: set encoding=utf-8 syntax=zsh ts=4 sw=4 ai
#
# This script cross compiles the various dependencies for PsyMP3 2.x
# Currently assumes a Debian-like distro.
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
	zlib		60df6a37c56e7c1366cca812414f7b85
	SDL			9d96df8417572a2afb781a7c4c811a85
	SDL_ttf		61e29bd9da8d245bc2471d1b2ce591aa
	SDL_gfx		838514185ff9a3b6377760aaa52fef8a
	freetype	5af8234cf36f64dc2b97f44f89325117
	taglib		dcb8bd1b756f2843e18b1fdf3aaeee15
	libvisual	f4e78547c79ea8a8ad111cf8b85011bb
	mpg123		a72d0c60a1d7dbec7cfe966bc11672bf
	ogg			0a7eb40b86ac050db3a789ab65fe21c2
	vorbis		6b1a36f0d72332fae5130688e65efe1f
)

DISTSHA256=(
	zlib		fa9c9c8638efb8cb8ef5e4dd5453e455751e1c530b1595eed466e1be9b7e26c5
	SDL			d6d316a793e5e348155f0dd93b979798933fb98aa1edebcc108829d6474aad00
	SDL_ttf		724cd895ecf4da319a3ef164892b72078bd92632a5d812111261cde248ebcdb7
	SDL_gfx		30ad38c3e17586e5212ce4a43955adf26463e69a24bb241f152493da28d59118
	freetype	29a70e55863e4b697f6d9f3ddc405a88b83a317e3c8fd9c09dc4e4c8b5f9ec3e
	taglib		66d33481703c90236a0a9d1c38fd81b584ca7109ded049225f5463dcaffc209a
	libvisual	0b4dfdb87125e129567752089e3c8b54cefed601eef169d2533d8659da8dc1d7
	mpg123		9ca189f24eb4ec6b5046b64d72c3c8439fd9ea300ce1b8b91a05cd6a9d3e5c12
	ogg			a8de807631014615549d2356fd36641833b8288221cea214f8a72750efe93780
	vorbis		6d747efe7ac4ad249bf711527882cef79fb61d9194c45b5ca5498aa60f290762
)

PACKAGES=( zlib	SDL SDL_ttf SDL_gfx	freetype taglib libvisual mpg123 ogg vorbis )

# Initialize our build environment 
mkdir -p ${BUILDDIR} ${DISTFILES}

notice () { 
	echo "===> " $*
}

distnotice () { 
	echo "=>" $*
}

package_check () { 
	package=$1
	if [ -z ${DISTS[$package]} ]; then
		notice "Invalid package " $package
		exit 1
	fi
}

distfiles_sum () { 
	package=$2
	package_check ${package}
	distfile="${DISTFILES}/$(basename ${DISTS[$package]})"
	case $1 in
		SHA256)
			distsum=${DISTSHA256[$package]}
			filesum=$(sha256sum ${distfile})
			;;
		MD5)
			distsum=${DISTMD5[$package]}
			filesum=$(md5sum ${distfile})
			;;
		*)
			notice "Invalid checksum algorithm tried! Bailing!"
			exit 1
			;;
	esac
	if [ ${filesum} = "${distsum}  ${distfile}" ]; then
		distnotice "$1 Checksum OK for $(basename ${distfile})."
		return 0;
	else
		distnotice "$1 Checksum mismatch for $(basename ${distfile})."
		return 1;
	fi
}

distfiles_fetch () { 
	package=$1
	package_check ${package}
	notice "Fetching for ${package}-${DISTVERS[$package]}"
	wget -O ${DISTFILES}/$(basename ${DISTS[$package]}) ${DISTS[$package]}
	distfiles_sum MD5 ${package}
	distfiles_sum SHA256 ${package}
}


# XXX: better system for fetching distfiles needed!!

for i in ${PACKAGES}; do
	# wget -O ${DISTFILES}/$(basename ${DISTS[$i]}) ${DISTS[$i]}
	distfiles_fetch ${i}
done


