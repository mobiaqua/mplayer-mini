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

#include "libavcodec/avcodec.h"
#include "libmpdemux/stheader.h"
#include "mp_msg.h"
#include "sub.h"
#include "av_helpers.h"
#include "av_sub.h"

void reset_avsub(struct sh_sub *sh)
{
    if (sh->context) {
        AVCodecContext *ctx = sh->context;
        ctx->extradata = NULL;
        ctx->extradata_size = 0;
        avcodec_close(sh->context);
        av_freep(&sh->context);
    }
}

/**
 * Decode a subtitle packet via libavcodec.
 * \return < 0 on error, > 0 if further processing is needed
 */
int decode_avsub(struct sh_sub *sh, uint8_t **data, int *size,
                 double *pts, double *endpts)
{
    AVCodecContext *ctx = sh->context;
    enum AVCodecID cid = AV_CODEC_ID_NONE;
    int new_type = 0;
    int res;
    int got_sub;
    AVSubtitle sub;
    AVPacket pkt;

    switch (sh->type) {
    case 'b':
        cid = AV_CODEC_ID_DVB_SUBTITLE; break;
    case 'p':
        cid = AV_CODEC_ID_HDMV_PGS_SUBTITLE; break;
    case 'x':
        cid = AV_CODEC_ID_XSUB; break;
    }

    av_init_packet(&pkt);
    pkt.data = *data;
    pkt.size = *size;
    pkt.pts = *pts * 1000;
    if (*pts != MP_NOPTS_VALUE && *endpts != MP_NOPTS_VALUE)
        pkt.duration = (*endpts - *pts) * 1000;
    if (!ctx) {
        AVCodec *sub_codec;
        init_avcodec();
        ctx = avcodec_alloc_context3(NULL);
        if (!ctx) {
            mp_msg(MSGT_SUBREADER, MSGL_FATAL,
                   "Could not allocate subtitle decoder context\n");
            return -1;
        }
        ctx->extradata_size = sh->extradata_len;
        ctx->extradata = sh->extradata;
        sub_codec = avcodec_find_decoder(cid);
        if (!sub_codec || avcodec_open2(ctx, sub_codec, NULL) < 0) {
            mp_msg(MSGT_SUBREADER, MSGL_FATAL,
                   "Could not open subtitle decoder\n");
            av_freep(&ctx);
            return -1;
        }
        sh->context = ctx;
    }
    res = avcodec_decode_subtitle2(ctx, &sub, &got_sub, &pkt);
    if (res < 0)
        return res;
    if (*pts != MP_NOPTS_VALUE) {
        if (sub.end_display_time > sub.start_display_time && sub.end_display_time < 0x7fffffff)
            *endpts = *pts + sub.end_display_time / 1000.0;
        *pts += sub.start_display_time / 1000.0;
    }
    if (got_sub && sub.num_rects > 0) {
        switch (sub.rects[0]->type) {
        case SUBTITLE_TEXT:
            *data = strdup(sub.rects[0]->text);
            *size = strlen(*data);
            new_type = 't';
            break;
        case SUBTITLE_ASS:
            *data = strdup(sub.rects[0]->ass);
            *size = strlen(*data);
            new_type = 'a';
            break;
        }
    }
    if (got_sub)
        avsubtitle_free(&sub);
    return new_type;
}
