# MPlayer Makefile
#
# copyright (c) 2008 Diego Biurrun
# Rewritten entirely from a set of Makefiles written by Arpi and many others.
#
# This file is part of MPlayer.
#
# MPlayer is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# MPlayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with MPlayer; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

include config.mak

SRCS_COMMON = bstr.c                            \
              av_helpers.c                      \
              av_opts.c                         \
              codec-cfg.c                       \
              command.c                         \
              fmt-conversion.c                  \
              m_config.c                        \
              m_option.c                        \
              m_struct.c                        \
              m_property.c                      \
              mixer.c                           \
              mp_fifo.c                         \
              mp_msg.c                          \
              mp_strings.c                      \
              mplayer.c                         \
              mpcommon.c                        \
              parser-cfg.c                      \
              parser-mpcmd.c                    \
              path.c                            \
              playtree.c                        \
              playtreeparser.c                  \
              subopt-helper.c                   \
              libaf/af.c                        \
              libaf/af_dummy.c                  \
              libaf/af_format.c                 \
              libaf/af_pan.c                    \
              libaf/af_sub.c                    \
              libaf/af_surround.c               \
              libaf/af_tools.c                  \
              libaf/af_volnorm.c                \
              libaf/af_volume.c                 \
              libaf/af_lavcresample.c           \
              libaf/filter.c                    \
              libaf/format.c                    \
              libaf/reorder_ch.c                \
              libaf/window.c                    \
              libao2/ao_null.c                  \
              libao2/audio_out.c                \
              libao2/ao_alsa.c                  \
              libvo/aspect.c                    \
              libvo/geometry.c                  \
              libvo/video_out.c                 \
              libvo/yuv420_to_nv12.S            \
              libvo/vo_omap_drm.c               \
              libvo/vo_omap_drm_egl.c           \
              libmpcodecs/ad.c                  \
              libmpcodecs/ad_pcm.c              \
              libmpcodecs/dec_audio.c           \
              libmpcodecs/dec_video.c           \
              libmpcodecs/img_format.c          \
              libmpcodecs/mp_image.c            \
              libmpcodecs/vd.c                  \
              libmpcodecs/vf.c                  \
              libmpcodecs/vf_vo.c               \
              libmpcodecs/ad_ffmpeg.c           \
              libmpcodecs/ad_spdif.c            \
              libmpcodecs/ad_mpg123.c           \
              libmpcodecs/vd_ffmpeg.c           \
              libmpcodecs/vd_omap_dce.c         \
              libmpdemux/demux_lavf.c           \
              libmpdemux/demuxer.c              \
              libmpdemux/demux_demuxers.c       \
              libmpdemux/mp_taglists.c          \
              libmpdemux/video.c                \
              input/input.c                     \
              osdep/getch2.c                    \
              osdep/timer-linux.c               \
              osdep/shmem.c                     \
              stream/cache2.c                   \
              stream/open.c                     \
              stream/stream.c                   \
              stream/stream_file.c              \
              sub/find_sub.c                    \
              sub/osd.c                         \
              sub/sub.c                         \
              sub/sub_cc.c                      \
              sub/subreader.c                   \
              sub/av_sub.c                      \
              sub/font_load_ft.c                \
              sub/font_load.c                   \

COMMON_LIBS                   += $(COMMON_LIBS-yes)

OBJS_COMMON                   += $(addsuffix .o, $(basename $(SRCS_COMMON)))

MPLAYER_DEPS  = $(OBJS_COMMON)
DEP_FILES     = $(SRCS_COMMON)
$(foreach suffix,.c .cpp .m .S,$(eval DEP_FILES := $(DEP_FILES:$(suffix)=.d)))

ALL_PRG-yes  += mplayer

INSTALL_TARGETS-yes  += install-mplayer

DIRS =  .                       \
        input                   \
        libaf                   \
        libao2                  \
        libmpcodecs             \
        libmpdemux              \
        libvo                   \
        osdep                   \
        stream                  \
        sub                     \

ALL_DIRS = $(DIRS)

ALLHEADERS = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.h))

ADDSUFFIXES     = $(foreach suf,$(1),$(addsuffix $(suf),$(2)))
ADD_ALL_DIRS    = $(call ADDSUFFIXES,$(1),$(ALL_DIRS))
ADD_ALL_EXESUFS = $(1) $(call ADDSUFFIXES,$(EXESUFS_ALL),$(1))


###### generic rules #######

all: $(ALL_PRG-yes)

%.o: %.S
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CC) $(CC_DEPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o: %.m
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -c -o $@ $<

%-rc.o: %.rc
	$(WINDRES) -I. $< -o $@

mplayer: $(MPLAYER_DEPS)
mplayer: EXTRALIBS += $(EXTRALIBS_MPLAYER)
mplayer:
	$(CC) -o $@ $^ $(EXTRALIBS)

codec-cfg-test: HOSTCFLAGS := $(HOSTCFLAGS) -DTESTING
codec-cfg codecs2html:  HOSTCFLAGS := $(HOSTCFLAGS) -DCODECS2HTML
codec-cfg codec-cfg-test codecs2html: codec-cfg.c codec-cfg.h
	$(BUILD_CC) $(HOSTCFLAGS) -o $@ $<

codecs.conf.h: codec-cfg etc/codecs.conf
	./$^ > $@

%: %.c
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)

%.ho: %.h
	$(CC) $(CFLAGS) -Wno-unused -c -o $@ -x c $<

checkheaders: $(ALLHEADERS:.h=.ho)


###### dependency declarations / specific CFLAGS ######

# Make sure all generated header files are created.
codec-cfg.o codec-cfg-test: codecs.conf.h
$(DEP_FILES) $(MPLAYER_DEPS): help_mp.h
mpcommon.o: version.h

###### installation / clean / generic rules #######

install: $(INSTALL_TARGETS-yes)

install-dirs:
	$(INSTALL) -d $(BINDIR)

install-%: % install-dirs
	$(INSTALL) -m 755 $(INSTALLSTRIP) $< $(BINDIR)

clean:
	-rm -f $(call ADD_ALL_DIRS,/*.o /*.d /*.a /*.ho /*~)
	-rm -f $(call ADD_ALL_EXESUFS,mplayer)
	-rm -f $(call ADD_ALL_EXESUFS,codec-cfg)
	-rm -f codecs.conf.h

distclean: clean
	-rm -f config.*


-include $(DEP_FILES)

.PHONY: all checkheaders *install* *clean

# Disable suffix rules.  Most of the builtin rules are suffix rules,
# so this saves some time on slow systems.
.SUFFIXES:
