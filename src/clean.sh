#!/bin/sh

. _env.sh

SYSROOT_NATIVE=$(greadlink -f "../sysroot-native")
export SYSROOT=$(greadlink -f "../sysroot")
export PATH=${SYSROOT_NATIVE}/usr/bin/:${SYSROOT_NATIVE}/usr/bin/arm-mobiaqua-linux-gnueabi:${PATH}

make clean
