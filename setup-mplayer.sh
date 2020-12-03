#!/bin/sh

[ -z "${OE_BASE}" ] && echo "Script require OE_BASE, DISTRO and TARGET configured by Yocto environment." && exit 1

curdir=$PWD

echo
echo "Preparing mplayer..."
echo

if [ ! -e mplayer/Makefile ]; then
	cd ${OE_BASE}/build-${DISTRO}-${TARGET} && \
		source env.source && \
		${OE_BASE}/bitbake/bin/bitbake mplayer-mini -cclean && \
		${OE_BASE}/bitbake/bin/bitbake mplayer-mini -cconfigure && {
			cd ${curdir}
			echo
			echo "Copying toolchain..."
			echo
			cp -R ${OE_BASE}/build-${DISTRO}-${TARGET}/tmp/work/*-linux-gnueabi/mplayer-mini/*/recipe-sysroot-native sysroot-native/
			cp -R ${OE_BASE}/build-${DISTRO}-${TARGET}/tmp/work/*-linux-gnueabi/mplayer-mini/*/recipe-sysroot sysroot/
			echo
			echo "--- Setup done ---"
			echo
		}
else
	echo
	echo "Nothing to do..."
	echo
fi
