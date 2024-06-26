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
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"
#include "dec_audio.h"
#include "av_helpers.h"
#include "libaf/reorder_ch.h"
#include "fmt-conversion.h"

static const ad_info_t info =
{
	"FFmpeg/libavcodec audio decoders",
	"ffmpeg",
	"Nick Kurshev",
	"ffmpeg.sf.net",
	""
};

LIBAD_EXTERN(ffmpeg)

#define assert(x)

#include "libavcodec/avcodec.h"
#include "libavutil/dict.h"
#include "libavutil/channel_layout.h"

struct adctx {
    int last_samplerate;
    int srate_changed;
};

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=AF_NCH*AVCODEC_MAX_AUDIO_FRAME_SIZE;
  return 1;
}

static int setup_format(sh_audio_t *sh_audio, const AVCodecContext *lavc_context)
{
    int broken_srate = 0;
    int samplerate    = lavc_context->sample_rate;
    int sample_format = samplefmt2affmt(av_get_packed_sample_fmt(lavc_context->sample_fmt));
    if (!sample_format)
        sample_format = sh_audio->sample_format;
    if (lavc_context->ch_layout.nb_channels != sh_audio->channels ||
        samplerate != sh_audio->samplerate ||
        sample_format != sh_audio->sample_format) {
        sh_audio->channels=lavc_context->ch_layout.nb_channels;
        sh_audio->samplerate=samplerate;
        sh_audio->sample_format = sample_format;
        sh_audio->samplesize=af_fmt2bits(sh_audio->sample_format)/ 8;
        if (broken_srate)
            mp_msg(MSGT_DECAUDIO, MSGL_WARN,
                   "Ignoring broken container sample rate for AAC with SBR\n");
        return 1;
    }
    return 0;
}

static int init(sh_audio_t *sh_audio)
{
    int tries = 0;
    int x;
    AVCodecContext *lavc_context;
    AVCodec *lavc_codec;
    AVDictionary *opts = NULL;
    char tmpstr[50];

    mp_msg(MSGT_DECAUDIO,MSGL_V,"FFmpeg's libavcodec audio codec\n");
    init_avcodec();

    lavc_codec = avcodec_find_decoder_by_name(codec_idx2str(sh_audio->codec->dll_idx));
    if(!lavc_codec){
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_MissingLAVCcodec,codec_idx2str(sh_audio->codec->dll_idx));
	return 0;
    }

    lavc_context = avcodec_alloc_context3(lavc_codec);
    sh_audio->context=lavc_context;
    lavc_context->opaque = av_mallocz(sizeof(struct adctx));

    snprintf(tmpstr, sizeof(tmpstr), "%f", drc_level);
    av_dict_set(&opts, "drc_scale", tmpstr, 0);
    lavc_context->sample_rate = sh_audio->samplerate;
    lavc_context->bit_rate = sh_audio->i_bps * 8;
    lavc_context->ch_layout.nb_channels = sh_audio->channels;
    lavc_context->block_align = sh_audio->block_align;
    lavc_context->extradata = sh_audio->codecdata;
    lavc_context->extradata_size = sh_audio->codecdata_len;
    lavc_context->codec_tag = sh_audio->format; //FOURCC
    lavc_context->codec_id = lavc_codec->id; // not sure if required, imho not --A'rpi


    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, &opts) < 0) {
        mp_msg(MSGT_DECAUDIO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
    av_dict_free(&opts);
   mp_msg(MSGT_DECAUDIO,MSGL_V,"INFO: libavcodec \"%s\" init OK!\n", lavc_codec->name);

//   printf("\nFOURCC: 0x%X\n",sh_audio->format);
   if(sh_audio->format==0x3343414D){
       // MACE 3:1
       sh_audio->ds->ss_div = 2*3; // 1 samples/packet
       sh_audio->ds->ss_mul = 2; // 1 byte*ch/packet
   } else
   if(sh_audio->format==0x3643414D){
       // MACE 6:1
       sh_audio->ds->ss_div = 2*6; // 1 samples/packet
       sh_audio->ds->ss_mul = 2; // 1 byte*ch/packet
   }

   // Decode at least 1 byte:  (to get header filled)
   do {
       x=decode_audio(sh_audio,sh_audio->a_buffer,1,sh_audio->a_buffer_size);
   } while (x <= 0 && tries++ < 5);
   if(x>0) sh_audio->a_buffer_len=x;

  sh_audio->i_bps=lavc_context->bit_rate/8;

  switch (lavc_context->sample_fmt) {
      case AV_SAMPLE_FMT_U8:  case AV_SAMPLE_FMT_U8P:
      case AV_SAMPLE_FMT_S16: case AV_SAMPLE_FMT_S16P:
      case AV_SAMPLE_FMT_S32: case AV_SAMPLE_FMT_S32P:
      case AV_SAMPLE_FMT_FLT: case AV_SAMPLE_FMT_FLTP:
          break;
      default:
          return 0;
  }
  setup_format(sh_audio, sh_audio->context);
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    AVCodecContext *lavc_context = sh->context;

    if (avcodec_close(lavc_context) < 0)
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, MSGTR_CantCloseCodec);
    av_freep(&lavc_context->opaque);
    av_freep(&lavc_context->extradata);
    av_freep(&lavc_context);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    AVCodecContext *lavc_context = sh->context;
    switch(cmd){
    case ADCTRL_RESYNC_STREAM:
        avcodec_flush_buffers(lavc_context);
        ds_clear_parser(sh->ds);
    return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static av_always_inline void copy_samples_planar(size_t bps,
                                                 size_t nb_samples,
                                                 size_t nb_channels,
                                                 unsigned char *dst,
                                                 unsigned char **src)
{
    size_t s, c, o = 0;

#if HAVE_NEON_INLINE && !ARCH_AARCH64
    if (nb_channels == 2 && bps == 4) {
        const unsigned char *src0 = src[0];
        const unsigned char *src1 = src[1];
        size_t aligned = nb_samples & ~7;
        const unsigned char *src0_end = src0 + aligned*bps;
        while (src0 < src0_end) {
           __asm__ (
               "vld1.32 {q0}, [%0]!\n\t"
               "vld1.32 {q1}, [%1]!\n\t"
               "vld1.32 {q2}, [%0]!\n\t"
               "vld1.32 {q3}, [%1]!\n\t"
               "vst2.32 {q0,q1}, [%2]!\n\t"
               "vst2.32 {q2,q3}, [%2]!\n\t"
               : "+&r"(src0), "+&r"(src1), "+&r"(dst)
               :: "q0", "q1", "q2", "q3", "memory");
        }
        o += aligned*bps;
        nb_samples -= aligned;
    } else if (nb_channels == 2 && bps == 2) {
        const unsigned char *src0 = src[0];
        const unsigned char *src1 = src[1];
        size_t aligned = nb_samples & ~15;
        const unsigned char *src0_end = src0 + aligned*bps;
        while (src0 < src0_end) {
           __asm__ (
               "vld1.16 {q0}, [%0]!\n\t"
               "vld1.16 {q1}, [%1]!\n\t"
               "vld1.16 {q2}, [%0]!\n\t"
               "vld1.16 {q3}, [%1]!\n\t"
               "vst2.16 {q0,q1}, [%2]!\n\t"
               "vst2.16 {q2,q3}, [%2]!\n\t"
               : "+&r"(src0), "+&r"(src1), "+&r"(dst)
               :: "q0", "q1", "q2", "q3", "memory");
        }
        o += aligned*bps;
        nb_samples -= aligned;
    }
#endif
    for (s = 0; s < nb_samples; s++) {
        for (c = 0; c < nb_channels; c++) {
            memcpy(dst, src[c] + o, bps);
            dst += bps;
        }
        o += bps;
    }
}

static int copy_samples(AVCodecContext *avc, AVFrame *frame,
                        unsigned char *buf, int max_size)
{
    int channels = avc->ch_layout.nb_channels;
    int sample_size = av_get_bytes_per_sample(avc->sample_fmt);
    int size = channels * sample_size * frame->nb_samples;

    if (size > max_size) {
        av_log(avc, AV_LOG_ERROR,
               "Buffer overflow while decoding a single frame\n");
        return AVERROR(EINVAL); /* same as avcodec_decode_audio3 */
    }
    /* TODO reorder channels at the same time */
    if (av_sample_fmt_is_planar(avc->sample_fmt)) {
        switch (sample_size) {
        case 1:
            copy_samples_planar(1, frame->nb_samples, channels,
                                buf, frame->extended_data);
            break;
        case 2:
            copy_samples_planar(2, frame->nb_samples, channels,
                                buf, frame->extended_data);
            break;
        case 4:
            copy_samples_planar(4, frame->nb_samples, channels,
                                buf, frame->extended_data);
            break;
        default:
            copy_samples_planar(sample_size, frame->nb_samples, channels,
                                buf, frame->extended_data);
    }
    } else {
        memcpy(buf, frame->data[0], size);
    }
    return size;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
    int draining_started = 0;
    unsigned char *start=NULL;
    int y,len=-1;
    AVFrame *frame = av_frame_alloc();

    if (!frame)
        return AVERROR(ENOMEM);

    while(len<minlen){
	int len2=maxlen;
	y = avcodec_receive_frame(sh_audio->context, frame);
	if (y == AVERROR(EAGAIN) || y == AVERROR_EOF) {
	AVPacket pkt;
	double pts;
	int x=ds_get_packet_pts(sh_audio->ds,&start, &pts);
	if(x<=0) {
	    start = NULL;
	    x = 0;
	    ds_parse(sh_audio->ds, &start, &x, MP_NOPTS_VALUE, 0);
	} else {
	    int in_size = x;
	    int consumed = ds_parse(sh_audio->ds, &start, &x, pts, 0);
	    sh_audio->ds->buffer_pos -= in_size - consumed;
	    // Explicitly request more data if all was used up by parser
	    if (x == 0 && consumed == in_size && len == -1) len = AVERROR(EAGAIN);
	    // Note: hopefully the following x <= 0 handling is correct, it was only
	    // added because FFmpeg broke the API and 0-sized
	    // packets started to break e.g. AC3 decode.
	}
	    if (x <= 0) {
	        if (sh_audio->ds->eof && !draining_started) {
	            avcodec_send_packet(sh_audio->context, NULL);
	            draining_started = 1;
	            continue;
	        }
	        break; // error or not enough data
	    }

	av_init_packet(&pkt);
	pkt.data = start;
	pkt.size = x;
	if (pts != MP_NOPTS_VALUE) {
	    sh_audio->pts = pts;
	    sh_audio->pts_bytes = 0;
	}
	y=avcodec_send_packet(sh_audio->context, &pkt);
	if(y<0){ mp_msg(MSGT_DECAUDIO,MSGL_V,"lavc_audio: error\n");break; }
	continue;
	}
	if(y<0){ mp_msg(MSGT_DECAUDIO,MSGL_V,"lavc_audio: error\n");break; }
#if 0
	// this should be obsolete since the new API does no support it
	// and we support inserting parsers as necessary instead.
	if(!sh_audio->parser && y<x)
	    sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
#endif
        len2 = copy_samples(sh_audio->context, frame, buf, maxlen);
        if (len2 < 0)
            return len2;
	if(len2>0){
	  if (((AVCodecContext *)sh_audio->context)->ch_layout.nb_channels >= 5) {
            int samplesize = av_get_bytes_per_sample(((AVCodecContext *)
                                    sh_audio->context)->sample_fmt);
            reorder_channel_nch(buf, AF_CHANNEL_LAYOUT_LAVC_DEFAULT,
                                AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
                                ((AVCodecContext *)sh_audio->context)->ch_layout.nb_channels,
                                len2 / samplesize, samplesize);
	  }
	  //len=len2;break;
	  if(len<0) len=len2; else len+=len2;
	  buf+=len2;
	  maxlen -= len2;
	  sh_audio->pts_bytes += len2;
	}
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"Decoded %d -> %d  \n",y,len2);

        if (setup_format(sh_audio, sh_audio->context))
            break;
    }

  av_frame_free(&frame);
  return len;
}
