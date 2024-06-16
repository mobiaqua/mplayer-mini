
export LC_ALL = C

CHARSET = UTF-8

AR      = $(CROSS_COMPILE)ar
AS      = $(CROSS_COMPILE)gcc -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard --sysroot=$(SYSROOT)
CC      = $(CROSS_COMPILE)gcc -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard --sysroot=$(SYSROOT)

DCE_INCLUDES = $(SYSROOT)/usr/include/dce
DRM_INCLUDES = $(SYSROOT)/usr/include/libdrm
GBM_INCLUDES = $(SYSROOT)/usr/include/gbm
FREETYPE_INCLUDES = $(SYSROOT)/usr/include/freetype2

CFLAGS   = -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wdisabled-optimization -Wno-pointer-sign -Wdeclaration-after-statement -std=gnu99  -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600 -D_ISOC99_SOURCE -I. -fno-tree-vectorize -fomit-frame-pointer -O4 -frename-registers -ffast-math -fno-tree-vectorize -fno-asynchronous-unwind-tables -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_REENTRANT -I$(DCE_INCLUDES) -I$(DRM_INCLUDES) -I$(GBM_INCLUDES) -I$(FREETYPE_INCLUDES) -g3 -O0
CC_DEPFLAGS = -MMD -MP
EXTRALIBS =  -Wl,-z,noexecstack -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed -lncurses -lrt -lasound -lfreetype -lz -lfontconfig -lpthread -ldl -lm -lmpg123 -ldce -ldrm -lgbm -lEGL -lGLESv2 -lavfilter -lavformat -lavcodec -lswscale -lswresample -lavutil

ASFLAGS    = $(CFLAGS)
AS_DEPFLAGS= -MMD -MP
HOSTCC     = cc
HOSTCFLAGS = -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600 -D_ISOC99_SOURCE -I. -O3
HOSTLIBS   = -lm
AS_O       = -o $@
CC_O       = -o $@
CXX_O      = -o $@
AS_C       = -c
CC_C       = -c
CXX_C      = -c
LD         = gcc
RANLIB     = true
STRIP      = strip
