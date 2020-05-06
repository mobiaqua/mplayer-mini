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

#ifndef MPLAYER_CFG_MPLAYER_H
#define MPLAYER_CFG_MPLAYER_H

/*
 * config for cfgparser
 */

#include <stddef.h>
#include "cfg-common.h"
#include "libmpcodecs/vd.h"
#include "libvo/aspect.h"
#include "libvo/geometry.h"
#include "mp_fifo.h"


const m_option_t vd_conf[]={
    {"help", "Use MPlayer with an appropriate video file instead of live partners to avoid vd.\n", CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

/*
 * CONF_TYPE_FUNC_FULL :
 * allows own implementations for passing the params
 *
 * the function receives parameter name and argument (if it does not start with - )
 * useful with a conf.name like 'aa*' to parse several parameters to a function
 * return 0 =ok, but we didn't need the param (could be the filename)
 * return 1 =ok, we accepted the param
 * negative values: see cfgparser.h, ERR_XXX
 *
 * by Folke
 */

const m_option_t mplayer_opts[]={
    /* name, pointer, type, flags, min, max */

//---------------------- libao/libvo options ------------------------
    {"o", "Option -o has been renamed to -vo (video-out), use -vo.\n",
     CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    {"vo", &video_driver_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"ao", &audio_driver_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"fixed-vo", &fixed_vo, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
    {"nofixed-vo", &fixed_vo, CONF_TYPE_FLAG,CONF_GLOBAL, 1, 0, NULL},
    {"ontop", &vo_ontop, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noontop", &vo_ontop, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"rootwin", &vo_rootwin, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"border", &vo_border, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noborder", &vo_border, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    {"aop", "-aop has been removed, use -af instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    {"dsp", "-dsp has been removed. Use -ao oss:dsp_path instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    {"mixer", &mixer_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"mixer-channel", &mixer_channel, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"softvol", &soft_vol, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nosoftvol", &soft_vol, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"softvol-max", &soft_vol_max, CONF_TYPE_FLOAT, CONF_RANGE, 10, 10000, NULL},
    {"volstep", &volstep, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
    {"volume", &start_volume, CONF_TYPE_FLOAT, CONF_RANGE, -1, 10000, NULL},
    {"master", "Option -master has been removed, use -af volume instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    // override audio buffer size (used only by -ao oss, anyway obsolete...)
    {"abs", &ao_data.buffersize, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

    // -ao pcm options:
    {"aofile", "-aofile has been removed. Use -ao pcm:file=<filename> instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"waveheader", "-waveheader has been removed. Use -ao pcm:waveheader instead.\n", CONF_TYPE_PRINT, 0, 0, 1, NULL},
    {"nowaveheader", "-nowaveheader has been removed. Use -ao pcm:nowaveheader instead.\n", CONF_TYPE_PRINT, 0, 1, 0, NULL},

    {"alsa", "-alsa has been removed. Remove it from your config file.\n",
     CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"noalsa", "-noalsa has been removed. Remove it from your config file.\n",
     CONF_TYPE_PRINT, 0, 0, 0, NULL},

    // force window width/height or resolution (with -vm)
    {"x", &opt_screen_size_x, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
    {"y", &opt_screen_size_y, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
    // set screen dimensions (when not detectable or virtual!=visible)
    {"screenw", &vo_screenwidth, CONF_TYPE_INT, CONF_RANGE|CONF_OLD, 0, 4096, NULL},
    {"screenh", &vo_screenheight, CONF_TYPE_INT, CONF_RANGE|CONF_OLD, 0, 4096, NULL},
    // Geometry string
    {"geometry", &vo_geometry, CONF_TYPE_STRING, 0, 0, 0, NULL},
    // vo name (X classname) and window title strings
    {"name", &vo_winname, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"title", &vo_wintitle, CONF_TYPE_STRING, 0, 0, 0, NULL},
    // set aspect ratio of monitor - useful for 16:9 TV-out
    {"monitoraspect", &force_monitor_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 9.0, NULL},
    {"monitorpixelaspect", &monitor_pixel_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 9.0, NULL},
    // video mode switching: (x11,xv,dga)
    {"vm", &vidmode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"novm", &vidmode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    // start in fullscreen mode:
    {"fs", &fullscreen, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nofs", &fullscreen, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    // set fullscreen switch method (workaround for buggy WMs)
    {"fsmode", "-fsmode is obsolete, avoid it and use -fstype instead.\nIf you really want it, try -fsmode-dontuse, but don't report bugs!\n", CONF_TYPE_PRINT, CONF_RANGE, 0, 31, NULL},
    {"fsmode-dontuse", &vo_fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
    // set bpp (x11+vm, dga, fbdev, vesa, svga?)
    {"bpp", &vo_dbpp, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL},
    {"colorkey", &vo_colorkey, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"nocolorkey", &vo_colorkey, CONF_TYPE_FLAG, 0, 0, 0x1000000, NULL},
    {"double", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nodouble", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    // wait for v-sync (vesa)
    {"vsync", &vo_vsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"novsync", &vo_vsync, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"panscan", &vo_panscan, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    {"panscanrange", &vo_panscanrange, CONF_TYPE_FLOAT, CONF_RANGE, -19.0, 99.0, NULL},
    {"border-pos-x", &vo_border_pos_x, CONF_TYPE_FLOAT, CONF_RANGE, -1, 2, NULL},
    {"border-pos-y", &vo_border_pos_y, CONF_TYPE_FLOAT, CONF_RANGE, -1, 2, NULL},
    {"fs-border-left",   &vo_fs_border_l, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
    {"fs-border-right",  &vo_fs_border_r, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
    {"fs-border-top",    &vo_fs_border_t, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
    {"fs-border-bottom", &vo_fs_border_b, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
    {"monitor-orientation", &vo_rotate, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},

    {"grabpointer", &vo_grabpointer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nograbpointer", &vo_grabpointer, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    {"adapter", &vo_adapter_num, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
    {"refreshrate",&vo_refresh_rate,CONF_TYPE_INT,CONF_RANGE, 0,100, NULL},
    {"wid", &WinID, CONF_TYPE_INT64, 0, 0, 0, NULL},
    {"heartbeat-cmd", &heartbeat_cmd, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"heartbeat-interval", &heartbeat_interval, CONF_TYPE_FLOAT, CONF_MIN, 0.0, 0, NULL},
    {"mouseinput", &vo_nomouse_input, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"nomouseinput", &vo_nomouse_input, CONF_TYPE_FLAG,0, 0, 1, NULL},

    {"xineramascreen", &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, -2, 32, NULL},
    {"screen",         &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, -2, 32, NULL},

    {"brightness",&vo_gamma_brightness, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"saturation",&vo_gamma_saturation, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"contrast",&vo_gamma_contrast, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"hue",&vo_gamma_hue, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"gamma",&vo_gamma_gamma, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"keepaspect", &vo_keepaspect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nokeepaspect", &vo_keepaspect, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    // direct rendering (decoding to video out buffer)
    {"dr", &vo_directrendering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nodr", &vo_directrendering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"vaa_dr", "-vaa_dr has been removed, use -dr.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"vaa_nodr", "-vaa_nodr has been removed, use -nodr.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},

//---------------------- mplayer-only options ------------------------

    {"use-filedir-conf", &use_filedir_conf, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nouse-filedir-conf", &use_filedir_conf, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"use-filename-title", &use_filename_title, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nouse-filename-title", &use_filename_title, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
#ifdef CONFIG_CRASH_DEBUG
    {"crash-debug", &crash_debug, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nocrash-debug", &crash_debug, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
#endif
    {"osdlevel", &osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
    {"osd-duration", &osd_duration, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
    {"osd-fractions", &osd_fractions, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},

    {"sstep", &step_sec, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

    {"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, NULL},
    {"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    {"benchmark", &benchmark, CONF_TYPE_FLAG, 0, 0, 1, NULL},

    {"gui", "The -gui option will only work as the first command line argument.\n", CONF_TYPE_PRINT, 0, 0, 0, PRIV_NO_EXIT},
    {"nogui", "The -nogui option will only work as the first command line argument.\n", CONF_TYPE_PRINT, 0, 0, 0, PRIV_NO_EXIT},

    {"noloop", &mpctx_s.loop_times, CONF_TYPE_FLAG, 0, 0, -1, NULL},
    {"loop", &mpctx_s.loop_times, CONF_TYPE_INT, CONF_RANGE, -1, 10000, NULL},
    {"allow-dangerous-playlist-parsing", &allow_playlist_parsing, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noallow-dangerous-playlist-parsing", &allow_playlist_parsing, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"playlist", NULL, CONF_TYPE_STRING, CONF_NOCFG, 0, 0, NULL},
    {"shuffle", NULL, CONF_TYPE_FLAG, CONF_NOCFG, 0, 0, NULL},
    {"noshuffle", NULL, CONF_TYPE_FLAG, CONF_NOCFG, 0, 0, NULL},

    // a-v sync stuff:
    {"correct-pts", &user_correct_pts, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nocorrect-pts", &user_correct_pts, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"noautosync", &autosync, CONF_TYPE_FLAG, 0, 0, -1, NULL},
    {"autosync", &autosync, CONF_TYPE_INT, CONF_RANGE, 0, 10000, NULL},

    {"softsleep", &softsleep, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nortc", &nortc, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"rtc", &nortc, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"rtc-device", &rtc_device, CONF_TYPE_STRING, 0, 0, 0, NULL},

    {"term-osd", &term_osd, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noterm-osd", &term_osd, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"term-osd-esc", &term_osd_esc, CONF_TYPE_STRING, 0, 0, 1, NULL},
    {"playing-msg", &playing_msg, CONF_TYPE_STRING, 0, 0, 0, NULL},

    {"slave", &slave_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
    {"idle", &player_idle_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
    {"noidle", &player_idle_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 1, 0, NULL},
    {"use-stdin", "-use-stdin has been renamed to -noconsolecontrols, use that instead.", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"key-fifo-size", &key_fifo_size, CONF_TYPE_INT, CONF_RANGE, 2, 65000, NULL},
    {"noconsolecontrols", &noconsolecontrols, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"consolecontrols", &noconsolecontrols, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"mouse-movements", &enable_mouse_movements, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nomouse-movements", &enable_mouse_movements, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"pausing", &pausing_default, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL},

    {"list-properties", &list_properties, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"identify", &mp_msg_levels[MSGT_IDENTIFY], CONF_TYPE_FLAG, CONF_GLOBAL, 0, MSGL_V, NULL},
    {"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},

    {"vd", vd_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    {"progbar-align", &progbar_align, CONF_TYPE_INT, CONF_GLOBAL, 0, 100, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#endif /* MPLAYER_CFG_MPLAYER_H */
