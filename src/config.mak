
export LC_ALL = C

CHARSET = UTF-8

AR      = arm-linux-gnueabi-ar
AS      = arm-linux-gnueabi-gcc -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb-interwork -marm
CC      = arm-linux-gnueabi-gcc -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb-interwork -marm

SYSROOT = $(OE_BASE)/build-$(DISTRO)/tmp/sysroots/armv7a-hf-linux-gnueabi

DCE_INCLUDES = $(SYSROOT)/usr/include/dce
DRM_INCLUDES = $(SYSROOT)/usr/include/libdrm
KMS_INCLUDES = $(SYSROOT)/usr/include/libkms
GBM_INCLUDES = $(SYSROOT)/usr/include/gbm
OMAP_INCLUDES = $(SYSROOT)/usr/include/omap
FREETYPE_INCLUDES = $(SYSROOT)/usr/include/freetype2

CFLAGS   = -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wdisabled-optimization -Wno-pointer-sign -Wdeclaration-after-statement -std=gnu99  -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600 -D_ISOC99_SOURCE -I. -fno-tree-vectorize -fomit-frame-pointer -O4 -frename-registers -ffast-math -fno-tree-vectorize -fno-asynchronous-unwind-tables -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_REENTRANT -I$(DCE_INCLUDES) -I$(DRM_INCLUDES) -I$(KMS_INCLUDES) -I$(GBM_INCLUDES) -I$(OMAP_INCLUDES) -I$(FREETYPE_INCLUDES) -g3 -O0
CC_DEPFLAGS = -MMD -MP
EXTRALIBS =  -Wl,-z,noexecstack -Wl,-O1 -lncurses -lrt -lasound -lfreetype -lz -lfontconfig -lpthread -ldl -lm -lmpg123 -ldce -ldrm -ldrm_omap -lgbm -lEGL -lGLESv2 -lavfilter -lavformat -lavcodec -lswscale -lswresample -lavutil

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
