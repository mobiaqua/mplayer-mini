/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_CFG_COMMON_H
#define MPLAYER_CFG_COMMON_H

#include <sys/types.h>

#include "libmpcodecs/ad.h"
#include "libmpcodecs/dec_audio.h"
#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/vd.h"
#include "libmpdemux/demuxer.h"
#include "sub/sub.h"
#include "stream/stream.h"
#include "codec-cfg.h"
#include "config.h"
#include "m_config.h"
#include "m_option.h"
#include "mp_msg.h"
#include "mpcommon.h"

#include "libaf/af.h"
const m_option_t audio_filter_conf[]={
    {"list", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"force", &af_cfg.force, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

const m_option_t msgl_config[]={
    { "all", &mp_msg_level_all, CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL},

    { "global", &mp_msg_levels[MSGT_GLOBAL], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cplayer", &mp_msg_levels[MSGT_CPLAYER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "gplayer", &mp_msg_levels[MSGT_GPLAYER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "vo", &mp_msg_levels[MSGT_VO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "ao", &mp_msg_levels[MSGT_AO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "demuxer", &mp_msg_levels[MSGT_DEMUXER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "ds", &mp_msg_levels[MSGT_DS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "demux", &mp_msg_levels[MSGT_DEMUX], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "header", &mp_msg_levels[MSGT_HEADER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "avsync", &mp_msg_levels[MSGT_AVSYNC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "autoq", &mp_msg_levels[MSGT_AUTOQ], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cfgparser", &mp_msg_levels[MSGT_CFGPARSER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "decaudio", &mp_msg_levels[MSGT_DECAUDIO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "decvideo", &mp_msg_levels[MSGT_DECVIDEO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "seek", &mp_msg_levels[MSGT_SEEK], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "win32", &mp_msg_levels[MSGT_WIN32], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "open", &mp_msg_levels[MSGT_OPEN], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "parsees", &mp_msg_levels[MSGT_PARSEES], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "lirc", &mp_msg_levels[MSGT_LIRC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "stream", &mp_msg_levels[MSGT_STREAM], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cache", &mp_msg_levels[MSGT_CACHE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "mencoder", &mp_msg_levels[MSGT_MENCODER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "xacodec", &mp_msg_levels[MSGT_XACODEC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "tv", &mp_msg_levels[MSGT_TV], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "radio", &mp_msg_levels[MSGT_RADIO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "osdep", &mp_msg_levels[MSGT_OSDEP], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "spudec", &mp_msg_levels[MSGT_SPUDEC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "playtree", &mp_msg_levels[MSGT_PLAYTREE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "input", &mp_msg_levels[MSGT_INPUT], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "vfilter", &mp_msg_levels[MSGT_VFILTER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "osd", &mp_msg_levels[MSGT_OSD], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "network", &mp_msg_levels[MSGT_NETWORK], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cpudetect", &mp_msg_levels[MSGT_CPUDETECT], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "codeccfg", &mp_msg_levels[MSGT_CODECCFG], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "sws", &mp_msg_levels[MSGT_SWS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "vobsub", &mp_msg_levels[MSGT_VOBSUB], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "subreader", &mp_msg_levels[MSGT_SUBREADER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "afilter", &mp_msg_levels[MSGT_AFILTER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "netst", &mp_msg_levels[MSGT_NETST], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "muxer", &mp_msg_levels[MSGT_MUXER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "osd-menu", &mp_msg_levels[MSGT_OSD_MENU], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "identify", &mp_msg_levels[MSGT_IDENTIFY], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "ass", &mp_msg_levels[MSGT_ASS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "statusline", &mp_msg_levels[MSGT_STATUSLINE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "fixme", &mp_msg_levels[MSGT_FIXME], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    {"help", "Available msg modules:\n"
    "   global     - common player errors/information\n"
    "   cplayer    - console player (mplayer.c)\n"
    "   vo         - libvo\n"
    "   ao         - libao\n"
    "   demuxer    - demuxer.c (general stuff)\n"
    "   ds         - demux stream (add/read packet etc)\n"
    "   demux      - fileformat-specific stuff (demux_*.c)\n"
    "   header     - fileformat-specific header (*header.c)\n"
    "   avsync     - mplayer.c timer stuff\n"
    "   autoq      - mplayer.c auto-quality stuff\n"
    "   cfgparser  - cfgparser.c\n"
    "   decaudio   - av decoder\n"
    "   decvideo\n"
    "   seek       - seeking code\n"
    "   win32      - win32 dll stuff\n"
    "   open       - open.c (stream opening)\n"
    "   parsees    - parse_es.c (mpeg stream parser)\n"
    "   lirc       - lirc_mp.c and input lirc driver\n"
    "   stream     - stream.c\n"
    "   cache      - cache2.c\n"
    "   mencoder\n"
    "   xacodec    - XAnim codecs\n"
    "   tv         - TV input subsystem\n"
    "   osdep      - OS-dependent parts\n"
    "   spudec     - spudec.c\n"
    "   playtree   - Playtree handling (playtree.c, playtreeparser.c)\n"
    "   input\n"
    "   vfilter\n"
    "   osd\n"
    "   network\n"
    "   cpudetect\n"
    "   codeccfg\n"
    "   sws\n"
    "   vobsub\n"
    "   subreader\n"
    "   osd-menu   - OSD menu messages\n"
    "   afilter    - Audio filter messages\n"
    "   netst      - Netstream\n"
    "   muxer      - muxer layer\n"
    "   identify   - identify output\n"
    "   ass        - libass messages\n"
    "   statusline - playback/encoding status line\n"
    "   fixme      - messages not yet fixed to map to module\n"
    "\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}

};

const m_option_t common_opts[] = {
// ------------------------- common options --------------------
    {"quiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"noquiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"really-quiet", &verbose, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_PRE_PARSE, 0, -10, NULL},
    {"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL},
    {"msglevel", msgl_config, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
    {"msgcolor", &mp_msg_color, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nomsgcolor", &mp_msg_color, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"msgmodule", &mp_msg_module, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nomsgmodule", &mp_msg_module, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"msgcharset", &mp_msg_charset, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
    {"include", cfg_include, CONF_TYPE_FUNC_PARAM_IMMEDIATE, CONF_NOSAVE, 0, 0, NULL},
    {"noconfig", noconfig_opts, CONF_TYPE_SUBCONFIG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 0, NULL},

// ------------------------- stream options --------------------

    {"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 32, 0x7fffffff, NULL},
    {"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"cache-min", &stream_cache_min_percent, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
    {"cache-seek-min", &stream_cache_seek_min_percent, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
    {"alang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"slang", &sub_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},

// ------------------------- demuxer options --------------------

    // number of frames to play/convert
    {"frames", &play_n_frames_mf, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

    // seek to byte/seconds position
    {"sb", &seek_to_byte, CONF_TYPE_POSITION, CONF_MIN, 0, 0, NULL},
    {"ss", &seek_to_sec, CONF_TYPE_TIME, 0, 0, 0, NULL},

    // stop at given position
    {"endpos", &end_at, CONF_TYPE_TIME_SIZE, 0, 0, 0, NULL},

    // select audio/video/subtitle stream
    {"aid", &audio_id, CONF_TYPE_INT, CONF_RANGE, -2, 8190, NULL},
    {"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, -2, 8190, NULL},
    {"sid", &sub_id, CONF_TYPE_INT, CONF_RANGE, -2, 8190, NULL},
    {"nosub", &sub_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},
    {"novideo", &video_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},

    // demuxer.c - select audio/sub file/demuxer
    { "audiofile", &audio_stream, CONF_TYPE_STRING, 0, 0, 0, NULL },
    { "audiofile-cache", &audio_stream_cache, CONF_TYPE_INT, CONF_RANGE, 50, 65536, NULL},
    { "subfile", &sub_stream, CONF_TYPE_STRING, 0, 0, 0, NULL },
    { "demuxer", &demuxer_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
    { "audio-demuxer", &audio_demuxer_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
    { "sub-demuxer", &sub_demuxer_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
    { "extbased", &extension_parsing, CONF_TYPE_FLAG, 0, 0, 1, NULL },
    { "noextbased", &extension_parsing, CONF_TYPE_FLAG, 0, 1, 0, NULL },

// ------------------------- a-v sync options --------------------

    // set A-V sync correction speed (0=disables it):
    {"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},

    // force video/audio rate:
    {"fps", &force_fps, CONF_TYPE_DOUBLE, CONF_MIN, 0, 0, NULL},
    {"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},
    {"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 1, 8, NULL},
    {"format", &audio_output_format, CONF_TYPE_AFMT, 0, 0, 0, NULL},
    {"speed", &playback_speed, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 100.0, NULL},

    // set a-v distance
    {"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -100.0, 100.0, NULL},

    // ignore header-specified delay (dwStart)
    {"ignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 1, 0, NULL},

// ------------------------- codec/vfilter options --------------------

    // MP3-only: select stereo/left/right
    {"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},

    // disable audio
    {"sound", &audio_id, CONF_TYPE_FLAG, 0, -2, -1, NULL},
    {"nosound", &audio_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},

    {"af*", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"af-adv", audio_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

    {"vop", "-vop has been removed, use -vf instead.\n", CONF_TYPE_PRINT, CONF_NOCFG ,0,0, NULL},
    {"vf*", &vf_settings, CONF_TYPE_OBJ_SETTINGS_LIST, 0, 0, 0, &vf_obj_list},
    // select audio/video codec (by name) or codec family (by number):
    {"afm", &audio_fm_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"vfm", &video_fm_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"ac", &audio_codec_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"vc", &video_codec_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},

    {"lavdopts", lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    {"lavfdopts",  lavfdopts_conf, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
    {"codecs-file", &codecs_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
// ------------------------- subtitles options --------------------

    {"sub", &sub_name, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"sub-paths", &sub_paths, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"subcp", &sub_cp, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
    {"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
    {"autosub", &sub_auto, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noautosub", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nounicode", &sub_unicode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"utf8", &sub_utf8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noutf8", &sub_utf8, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"forcedsubsonly", &forced_subs_only, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    // enable Closed Captioning display
    {"subcc", &subcc_enabled, CONF_TYPE_INT, CONF_RANGE, 0, 8, NULL},
    {"nosubcc", &subcc_enabled, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"overlapsub", &suboverlap_enabled, CONF_TYPE_FLAG, 0, 0, 2, NULL},
    {"nooverlapsub", &suboverlap_enabled, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"sub-bg-color", &sub_bg_color, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
    {"sub-bg-alpha", &sub_bg_alpha, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
    {"sub-no-text-pp", &sub_no_text_pp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"sub-fuzziness", &sub_match_fuzziness, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"font", &font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subfont", &sub_font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL},
    {"subpos", &sub_pos, CONF_TYPE_INT, CONF_RANGE, 0, 150, NULL},
    {"subalign", &sub_alignment, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"subwidth", &sub_width_p, CONF_TYPE_INT, CONF_RANGE, 10, 100, NULL},
    {"subfont-encoding", &subtitle_font_encoding, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subfont-text-scale", &text_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
    {"subfont-osd-scale", &osd_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
    {"subfont-blur", &subtitle_font_radius, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
    {"subfont-outline", &subtitle_font_thickness, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
    {"subfont-autoscale", &subtitle_autoscale, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
    {"fontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, -1, 1, NULL},
    {"nofontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, 1, -1, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#endif /* MPLAYER_CFG_COMMON_H */
