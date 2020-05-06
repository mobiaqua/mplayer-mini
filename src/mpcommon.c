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

#include <stdlib.h>
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "codec-cfg.h"
#include "osdep/timer.h"
#include "path.h"
#include "mplayer.h"
#include "sub/font_load.h"
#include "sub/sub.h"
#include "libvo/video_out.h"
#include "help_mp.h"
#include "mp_msg.h"
#include "parser-cfg.h"
#include "version.h"
#include "sub/av_sub.h"
#include "sub/sub_cc.h"
#include "libavutil/intreadwrite.h"
#include "m_option.h"
#include "mpcommon.h"
#include <libavutil/avutil.h>

double sub_last_pts = -303;

sub_data* subdata = NULL;
subtitle* vo_sub_last = NULL;
char *spudec_ifo;
int forced_subs_only;

const char *mplayer_version  = "Mini MPlayer "  VERSION;

void print_version(const char* name)
{
    mp_msg(MSGT_CPLAYER, MSGL_INFO, MP_TITLE, name);
}

static int is_text_sub(int type)
{
    return type == 't' || type == 'm' || type == 'a';
}

static int is_av_sub(int type)
{
    return type == 'b' || type == 'p' || type == 'x';
}

void update_subtitles(sh_video_t *sh_video, double refpts, demux_stream_t *sub, int reset)
{
    double curpts = refpts - sub_delay;
    unsigned char *packet=NULL;
    int len;
    int type = sub->sh ? ((sh_sub_t *)sub->sh)->type : 'v';
    static subtitle subs;
    if (reset) {
        sub_clear_text(&subs, MP_NOPTS_VALUE);
        if (vo_sub) {
            set_osd_subtitle(NULL);
        }
        if (is_av_sub(type))
            reset_avsub(sub->sh);
        subcc_reset();
    }
    // find sub
    if (subdata) {
        if (sub_fps==0) sub_fps = sh_video ? sh_video->fps : 25;
        current_module = "find_sub";
        if (refpts > sub_last_pts || refpts < sub_last_pts-1.0) {
            find_sub(subdata, curpts *
                     (subdata->sub_uses_time ? 100. : sub_fps));
            if (vo_sub) vo_sub_last = vo_sub;
            // FIXME! frame counter...
            sub_last_pts = refpts;
        }
    }

    // DVD sub:
    if (vo_config_count && type == 'v') {
        int timestamp;
        current_module = "sub";
        /* Get a sub packet from the DVD or a vobsub */
        while(1) {
                // DVD sub
                len = ds_get_packet_sub(sub, (unsigned char**)&packet, NULL, NULL);
                if (len > 0) {
                    // XXX This is wrong, sh_video->pts can be arbitrarily
                    // much behind demuxing position. Unfortunately using
                    // d_video->pts which would have been the simplest
                    // improvement doesn't work because mpeg specific hacks
                    // in video.c set d_video->pts to 0.
                    float x = sub->pts - refpts;
                    if (x > -20 && x < 20) // prevent missing subs on pts reset
                        timestamp = 90000*sub->pts;
                    else timestamp = 90000*curpts;
                    mp_dbg(MSGT_CPLAYER, MSGL_V, "\rsub: len=%d  "
                           "v_pts=%5.3f  s_pts=%5.3f  ts=%d \n", len,
                           refpts, sub->pts, timestamp);
                }
            if (len<=0 || !packet) break;
        }
    } else if (is_text_sub(type) || is_av_sub(type) || type == 'd' || type == 'c') {
        int orig_type = type;
        double endpts;
        if (sub->non_interleaved)
            ds_get_next_pts(sub);
        while (1) {
            double subpts = curpts;
            type = orig_type;
            len = ds_get_packet_sub(sub, &packet, &subpts, &endpts);
            if (len < 0)
                break;
            if (is_av_sub(type)) {
                type = decode_avsub(sub->sh, &packet, &len, &subpts, &endpts);
                if (type < 0)
                    mp_msg(MSGT_CPLAYER, MSGL_WARN, "lavc failed decoding subtitle\n");
                if (type <= 0)
                    continue;
            }
            if (type == 'm') {
                if (len < 2) continue;
                len = FFMIN(len - 2, AV_RB16(packet));
                packet += 2;
            }
            if (type == 'd') {
                continue;
            }
            if (type == 'c') {
                subcc_process_data(packet, len);
                continue;
            }
            if (subpts != MP_NOPTS_VALUE) {
                if (endpts == MP_NOPTS_VALUE)
                    sub_clear_text(&subs, MP_NOPTS_VALUE);
                if (type == 'a') { // ssa/ass subs without libass => convert to plaintext
                    int i;
                    unsigned char* p = packet;
                    int skip_commas = 8;
                    if (len > 10 && memcmp(packet, "Dialogue: ", 10) == 0)
                        skip_commas = 9;
                    for (i=0; i < skip_commas && *p != '\0'; p++)
                        if (*p == ',')
                            i++;
                    if (*p == '\0')  /* Broken line? */
                        continue;
                    len -= p - packet;
                    packet = p;
                }
                if (endpts == MP_NOPTS_VALUE) endpts = subpts + 4;
                sub_add_text(&subs, packet, len, endpts, 1);
                set_osd_subtitle(&subs);
            }
            if (sub->non_interleaved)
                ds_get_next_pts(sub);
        }
        if (sub_clear_text(&subs, curpts))
            set_osd_subtitle(&subs);
    }

    current_module=NULL;
}

int select_audio(demuxer_t* demuxer, int audio_id, char* audio_lang)
{
    if (audio_id == -1 && audio_lang)
        audio_id = demuxer_audio_track_by_lang(demuxer, audio_lang);
    if (audio_id == -1)
        audio_id = demuxer_default_audio_track(demuxer);
    if (audio_id != -1) // -1 (automatic) is the default behaviour of demuxers
        demuxer_switch_audio(demuxer, audio_id);
    if (audio_id == -2) { // some demuxers don't yet know how to switch to no sound
        demuxer->audio->id = -2;
        demuxer->audio->sh = NULL;
    }
    return demuxer->audio->id;
}

int select_video(demuxer_t* demuxer, int video_id)
{
    if (video_id == -1)
        video_id = demuxer_default_video_track(demuxer);
    if (video_id != -1) // -1 (automatic) is the default behaviour of demuxers
        demuxer_switch_video(demuxer, video_id);

    return demuxer->video->id;
}

/* Parse -noconfig common to both programs */
int disable_system_conf=0;
int disable_user_conf=0;

/* Disable all configuration files */
static void noconfig_all(void)
{
    disable_system_conf = 1;
    disable_user_conf = 1;
}

m_config_t *mconfig;

int cfg_inc_verbose(m_option_t *conf)
{
    ++verbose;
    return 0;
}

int cfg_include(m_option_t *conf, const char *filename)
{
    return m_config_parse_config_file(mconfig, filename, 0);
}

const m_option_t noconfig_opts[] = {
    {"all", noconfig_all, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 0, NULL},
    {"system", &disable_system_conf, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 1, NULL},
    {"user", &disable_user_conf, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 1, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

/**
 * Code to fix any kind of insane defaults some OS might have.
 * Currently mostly fixes for insecure-by-default Windows.
 */
static void sanitize_os(void)
{
}

/**
 * Initialization code to be run at the very start, must not depend
 * on option values.
 */
void common_preinit(int *argc_ptr, char **argv_ptr[])
{
    (void)argc_ptr;
    (void)argv_ptr;
    sanitize_os();
    InitTimer();
    srand(GetTimerMS());

    mp_msg_init();
}

/**
 * Initialization code to be run after command-line parsing.
 */
int common_init(void)
{
    /* Check codecs.conf. */
    if (!codecs_file || !parse_codec_cfg(codecs_file)) {
        char *conf_path = get_path("codecs.conf");
        if (!parse_codec_cfg(conf_path)) {
            if (!parse_codec_cfg(MPLAYER_CONFDIR "/codecs.conf")) {
                if (!parse_codec_cfg(NULL)) {
                    free(conf_path);
                    return 0;
                }
                mp_msg(MSGT_CPLAYER, MSGL_V, "Using built-in default codecs.conf.\n");
            }
        }
        free(conf_path);
    }

    // check font
    init_freetype();

    vo_init_osd();

    return 1;
}

void common_uninit(void)
{
    current_module = "uninit_font";
    if (sub_font && sub_font != vo_font)
        free_font_desc(sub_font);
    sub_font = NULL;
    if (vo_font)
        free_font_desc(vo_font);
    vo_font = NULL;
    done_freetype();
    free_osd_list();

}

/// Returns a_pts
double calc_a_pts(sh_audio_t *sh_audio, demux_stream_t *d_audio)
{
    double a_pts;
    if(!sh_audio || !d_audio)
        return MP_NOPTS_VALUE;
    // first calculate the end pts of audio that has been output by decoder
    a_pts = sh_audio->pts;
    // If we cannot get any useful information at all from the demuxer layer
    // just count the decoded bytes. This is still better than constantly
    // resetting to 0.
    if (sh_audio->pts_bytes && a_pts == MP_NOPTS_VALUE &&
        !d_audio->pts && !sh_audio->i_bps)
        a_pts = 0;
    if (a_pts != MP_NOPTS_VALUE)
        // Good, decoder supports new way of calculating audio pts.
        // sh_audio->pts is the timestamp of the latest input packet with
        // known pts that the decoder has decoded. sh_audio->pts_bytes is
        // the amount of bytes the decoder has written after that timestamp.
        a_pts += sh_audio->pts_bytes / (double) sh_audio->o_bps;
    else {
        // Decoder doesn't support new way of calculating pts (or we're
        // being called before it has decoded anything with known timestamp).
        // Use the old method of audio pts calculation: take the timestamp
        // of last packet with known pts the decoder has read data from,
        // and add amount of bytes read after the beginning of that packet
        // divided by input bps. This will be inaccurate if the input/output
        // ratio is not constant for every audio packet or if it is constant
        // but not accurately known in sh_audio->i_bps.

        a_pts = d_audio->pts;
        // ds_tell_pts returns bytes read after last timestamp from
        // demuxing layer, decoder might use sh_audio->a_in_buffer for bytes
        // it has read but not decoded
        if (sh_audio->i_bps)
            a_pts += (ds_tell_pts(d_audio) - sh_audio->a_in_buffer_len) /
                     (double)sh_audio->i_bps;
    }
    return a_pts;
}
