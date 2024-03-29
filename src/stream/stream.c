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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "config.h"

#include "libavutil/avstring.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "osdep/shmem.h"
#include "osdep/timer.h"
#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"

#include "m_option.h"
#include "m_struct.h"

#include "cache2.h"

static int (*stream_check_interrupt_cb)(int time) = NULL;

extern const stream_info_t stream_info_ffmpeg;
extern const stream_info_t stream_info_file;

static const stream_info_t* const auto_open_streams[] = {
  &stream_info_ffmpeg,
  &stream_info_file,
  NULL
};

static stream_t* open_stream_plugin(const stream_info_t* sinfo, const char* filename,
                                    int mode, char** options, int* file_format,
                                    int* ret, char** redirected_url)
{
  void* arg = NULL;
  stream_t* s;
  m_struct_t* desc = (m_struct_t*)sinfo->opts;

  // Parse options
  if(desc) {
    arg = m_struct_alloc(desc);
    if(sinfo->opts_url) {
      m_option_t url_opt =
	{ "stream url", arg , CONF_TYPE_CUSTOM_URL, 0, 0 ,0, sinfo->opts };
      if(m_option_parse(&url_opt,"stream url",filename,arg,M_CONFIG_FILE) < 0) {
	mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_URLParsingFailed,filename);
	m_struct_free(desc,arg);
	return NULL;
      }
    }
    if(options) {
      int i;
      for(i = 0 ; options[i] != NULL ; i += 2) {
	mp_msg(MSGT_OPEN,MSGL_DBG2, "Set stream arg %s=%s\n",
	       options[i],options[i+1]);
	if(!m_struct_set(desc,arg,options[i],options[i+1]))
	  mp_msg(MSGT_OPEN,MSGL_WARN, MSGTR_FailedSetStreamOption,
		 options[i],options[i+1]);
      }
    }
  }
  s = new_stream(-2,-2);
  s->capture_file = NULL;
  s->url=strdup(filename);
  s->flags |= mode;
  *ret = sinfo->open(s,mode,arg,file_format);
  if((*ret) != STREAM_OK) {
    free(s->url);
    free(s);
    return NULL;
  }
  if(s->type <= -2)
    mp_msg(MSGT_OPEN,MSGL_WARN, MSGTR_StreamNeedType);
  if(s->flags & MP_STREAM_SEEK && !s->seek)
    s->flags &= ~MP_STREAM_SEEK;
  if(s->seek && !(s->flags & MP_STREAM_SEEK))
    s->flags |= MP_STREAM_SEEK;

  s->mode = mode;

  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: [%s] %s\n",sinfo->name,filename);
  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: Description: %s\n",sinfo->info);
  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: Author: %s\n", sinfo->author);
  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: Comment: %s\n", sinfo->comment);

  return s;
}


stream_t* open_stream_full(const char* filename,int mode, char** options, int* file_format) {
  int i,j;

  for(i = 0 ; auto_open_streams[i] ; i++) {
    const stream_info_t *sinfo = auto_open_streams[i];
    for(j = 0 ; sinfo->protocols[j] ; j++) {
      int l = strlen(sinfo->protocols[j]);
      // l == 0 => Don't do protocol matching (ie network and filenames)
      if((l == 0 && !strstr(filename, "://")) ||
         ((av_strncasecmp(sinfo->protocols[j],filename,l) == 0) &&
		      (strncmp("://",filename+l,3) == 0))) {
	int r;
	char *redirected_url = NULL;
	stream_t* s;
	*file_format = DEMUXER_TYPE_UNKNOWN;
	s = open_stream_plugin(sinfo,filename,mode,options,file_format,&r,
				&redirected_url);
	if(s) return s;
	if(r == STREAM_REDIRECTED && redirected_url) {
	  mp_msg(MSGT_OPEN,MSGL_V, "[%s] open %s redirected to %s\n",
		 sinfo->info, filename, redirected_url);
	  s = open_stream_full(redirected_url, mode, options, file_format);
	  free(redirected_url);
	  return s;
	}
	else if(r != STREAM_UNSUPPORTED) {
	  mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_FailedToOpen,filename);
	  return NULL;
	}
	break;
      }
    }
  }

  mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_StreamCantHandleURL,filename);
  return NULL;
}

//=================== STREAMER =========================

static int stream_reconnect(stream_t *s)
{
#define MAX_RECONNECT_RETRIES 5
#define RECONNECT_SLEEP_MS 1000
    int retry = 0;
    int64_t pos = s->pos;
    // Seeking is used as a hack to make network streams
    // reopen the connection, ideally they would implement
    // e.g. a STREAM_CTRL_RECONNECT to do this
    do {
        if (retry >= MAX_RECONNECT_RETRIES)
            return 0;
        if (retry) usec_sleep(RECONNECT_SLEEP_MS * 1000);
        retry++;
        s->eof=1;
        stream_reset(s);
    } while (stream_seek_internal(s, pos) >= 0 || s->pos != pos); // seek failed
    return 1;
}

int stream_read_internal(stream_t *s, void *buf, int len)
{
  int orig_len = len;
  // we will retry even if we already reached EOF previously.
  switch(s->type){
  case STREAMTYPE_STREAM:
    if (s->fill_buffer)
      len = s->fill_buffer(s, buf, len);
    else
      len = read(s->fd, buf, len);
    break;
  case STREAMTYPE_DS:
    len = demux_read_data((demux_stream_t*)s->priv, buf, len);
    break;


  default:
    len= s->fill_buffer ? s->fill_buffer(s, buf, len) : 0;
  }
  if(len<=0){
    // do not retry if this looks like proper eof
    if (s->eof || (s->end_pos && s->pos == s->end_pos))
      goto eof_out;

    // just in case this is an error e.g. due to network
    // timeout reset and retry
    if (!stream_reconnect(s))
      goto eof_out;
    // make sure EOF is set to ensure no endless loops
    s->eof=1;
    return stream_read_internal(s, buf, orig_len);

eof_out:
    s->eof=1;
    return 0;
  }
  // When reading succeeded we are obviously not at eof.
  // This e.g. avoids issues with eof getting stuck when lavf seeks in MPEG-TS
  s->eof=0;
  s->pos+=len;
  return len;
}

int stream_fill_buffer(stream_t *s){
  int len = stream_read_internal(s, s->buffer, STREAM_BUFFER_SIZE);
  if (len <= 0)
    return 0;
  s->buf_pos=0;
  s->buf_len=len;
  while (s->buf_len < STREAM_BUFFER_MIN) {
    assert(s->buf_len + STREAM_BUFFER_MIN < STREAM_BUFFER_SIZE);
    len = stream_read_internal(s, s->buffer + s->buf_len, STREAM_BUFFER_MIN);
    if (len <= 0)
      break;
    s->buf_len += len;
  }
  // since the first read succeeded we are
  // definitely not at EOF yet
  s->eof = 0;
//  printf("[%d]",len);fflush(stdout);
  return s->buf_len;
}

int stream_write_buffer(stream_t *s, unsigned char *buf, int len) {
  int rd;
  if(!s->write_buffer)
    return -1;
  rd = s->write_buffer(s, buf, len);
  if(rd < 0)
    return -1;
  s->pos += rd;
  assert(rd == len && "stream_write_buffer(): unexpected short write");
  return rd;
}

int stream_seek_internal(stream_t *s, int64_t newpos)
{
if(newpos==0 || newpos!=s->pos){
  switch(s->type){
  case STREAMTYPE_STREAM:
    //s->pos=newpos; // real seek
    // Some streaming protocol allow to seek backward and forward
    // A function call that return -1 can tell that the protocol
    // doesn't support seeking.
    if(newpos<s->pos){
      mp_msg(MSGT_STREAM,MSGL_INFO,MSGTR_StreamCannotSeekBackward);
      return 1;
    }
    break;
  default:
    // This should at the beginning as soon as all streams are converted
    if(!s->seek)
      return 0;
    // Now seek
    if(!s->seek(s,newpos)) {
      mp_msg(MSGT_STREAM,MSGL_ERR, MSGTR_StreamSeekFailed);
      return 0;
    }
  }
//   putchar('.');fflush(stdout);
//} else {
//   putchar('%');fflush(stdout);
}
  return -1;
}

int stream_seek_long(stream_t *s, int64_t pos){
  int res;
  int64_t newpos=0;

//  if( mp_msg_test(MSGT_STREAM,MSGL_DBG3) ) printf("seek_long to 0x%X\n",(unsigned int)pos);

  s->buf_pos=s->buf_len=0;

  if(s->mode == STREAM_WRITE) {
    if(!s->seek || !s->seek(s,pos))
      return 0;
    return 1;
  }

  if(s->sector_size)
      newpos = (pos/s->sector_size)*s->sector_size;
  else
      newpos = pos&(~((int64_t)STREAM_BUFFER_SIZE-1));

if( mp_msg_test(MSGT_STREAM,MSGL_DBG3) ){
  mp_msg(MSGT_STREAM,MSGL_DBG3, "s->pos=%"PRIX64"  newpos=%"PRIX64"  new_bufpos=%"PRIX64"  buflen=%X  \n",
    (int64_t)s->pos,(int64_t)newpos,(int64_t)pos,s->buf_len);
}
  pos-=newpos;

  res = stream_seek_internal(s, newpos);
  if (res >= 0)
    return res;

  while(s->pos<newpos){
    if(stream_fill_buffer(s)<=0) break; // EOF
  }

while(stream_fill_buffer(s) > 0 && pos >= 0) {
  if(pos<=s->buf_len){
    s->buf_pos=pos; // byte position in sector
    return 1;
  }
  pos -= s->buf_len;
}

//  if(pos==s->buf_len) printf("XXX Seek to last byte of file -> EOF\n");

  mp_msg(MSGT_STREAM,MSGL_V,"stream_seek: WARNING! Can't seek to 0x%"PRIX64" !\n",(int64_t)(pos+newpos));
  return 0;
}


void stream_reset(stream_t *s){
  if(s->eof){
    s->buf_pos=s->buf_len=0;
    s->eof=0;
  }
  if(s->control) s->control(s,STREAM_CTRL_RESET,NULL);
  //stream_seek(s,0);
}

int stream_control(stream_t *s, int cmd, void *arg){
  if(!s->control) return STREAM_UNSUPPORTED;
  if (s->cache_pid)
    return cache_do_control(s, cmd, arg);
  return s->control(s, cmd, arg);
}

stream_t* new_memory_stream(unsigned char* data,int len){
  stream_t *s;

  if(len < 0)
    return NULL;
  s=calloc(1, sizeof(stream_t)+len);
  s->fd=-1;
  s->type=STREAMTYPE_MEMORY;
  s->buf_pos=0; s->buf_len=len;
  s->start_pos=0; s->end_pos=len;
  stream_reset(s);
  s->pos=len;
  memcpy(s->buffer,data,len);
  return s;
}

stream_t* new_stream(int fd,int type){
  stream_t *s=calloc(1, sizeof(stream_t));
  if(s==NULL) return NULL;

  s->fd=fd;
  s->type=type;
  s->buf_pos=s->buf_len=0;
  s->start_pos=s->end_pos=0;
  s->priv=NULL;
  s->url=NULL;
  s->cache_pid=0;
  stream_reset(s);
  return s;
}

void free_stream(stream_t *s){
//  printf("\n*** free_stream() called ***\n");
    cache_uninit(s);
  if (s->capture_file) {
    fclose(s->capture_file);
    s->capture_file = NULL;
  }

  if(s->close) s->close(s);
  if(s->fd>0){
    /* on unix we define closesocket to close
       on windows however we have to distinguish between
       network socket and file */
    close(s->fd);
  }
  // Disabled atm, i don't like that. s->priv can be anything after all
  // streams should destroy their priv on close
  //free(s->priv);
  free(s->url);
  free(s);
}

stream_t* new_ds_stream(demux_stream_t *ds) {
  stream_t* s = new_stream(-1,STREAMTYPE_DS);
  s->priv = ds;
  return s;
}

void stream_set_interrupt_callback(int (*cb)(int)) {
    stream_check_interrupt_cb = cb;
}

int stream_check_interrupt(int time) {
    if(!stream_check_interrupt_cb) {
        usec_sleep(time * 1000);
        return 0;
    }
    return stream_check_interrupt_cb(time);
}

/**
 * Helper function to read 16 bits little-endian and advance pointer
 */
static uint16_t get_le16_inc(const uint8_t **buf)
{
  uint16_t v = AV_RL16(*buf);
  *buf += 2;
  return v;
}

/**
 * Helper function to read 16 bits big-endian and advance pointer
 */
static uint16_t get_be16_inc(const uint8_t **buf)
{
  uint16_t v = AV_RB16(*buf);
  *buf += 2;
  return v;
}

/**
 * Find a termination character in buffer
 * \param buf buffer to search
 * \param len amount of bytes to search in buffer, may not overread
 * \param utf16 chose between UTF-8/ASCII/other and LE and BE UTF-16
 *              0 = UTF-8/ASCII/other, 1 = UTF-16-LE, 2 = UTF-16-BE
 */
static const uint8_t *find_term_char(const uint8_t *buf, int len, uint8_t term, int utf16)
{
  uint32_t c;
  const uint8_t *end = buf + len;
  switch (utf16) {
  case 0:
    return (uint8_t *)memchr(buf, term, len);
  case 1:
    while (buf < end - 1) {
      GET_UTF16(c, buf < end - 1 ? get_le16_inc(&buf) : 0, return NULL;)
      if (buf <= end && c == term)
        return buf - 1;
    }
    break;
  case 2:
    while (buf < end - 1) {
      GET_UTF16(c, buf < end - 1 ? get_be16_inc(&buf) : 0, return NULL;)
      if (buf <= end && c == term)
        return buf - 1;
    }
    break;
  }
  return NULL;
}

/**
 * Copy a number of bytes, converting to UTF-8 if input is UTF-16
 * \param dst buffer to copy to
 * \param dstsize size of dst buffer
 * \param src buffer to copy from
 * \param len amount of bytes to copy from src
 * \param utf16 chose between UTF-8/ASCII/other and LE and BE UTF-16
 *              0 = UTF-8/ASCII/other, 1 = UTF-16-LE, 2 = UTF-16-BE
 */
static int copy_characters(uint8_t *dst, int dstsize,
                           const uint8_t *src, int *len, int utf16)
{
  uint32_t c;
  uint8_t *dst_end = dst + dstsize;
  const uint8_t *end = src + *len;
  switch (utf16) {
  case 0:
    if (*len > dstsize)
      *len = dstsize;
    memcpy(dst, src, *len);
    return *len;
  case 1:
    while (src < end - 1 && dst_end - dst > 8) {
      uint8_t tmp;
      GET_UTF16(c, src < end - 1 ? get_le16_inc(&src) : 0, ;)
      PUT_UTF8(c, tmp, *dst++ = tmp;)
    }
    *len -= end - src;
    return dstsize - (dst_end - dst);
  case 2:
    while (src < end - 1 && dst_end - dst > 8) {
      uint8_t tmp;
      GET_UTF16(c, src < end - 1 ? get_be16_inc(&src) : 0, ;)
      PUT_UTF8(c, tmp, *dst++ = tmp;)
    }
    *len -= end - src;
    return dstsize - (dst_end - dst);
  }
  return 0;
}

uint8_t *stream_read_until(stream_t *s, uint8_t *mem, int max,
                           uint8_t term, int utf16)
{
  int len;
  const unsigned char *end;
  unsigned char *ptr = mem;
  if (max < 1) return NULL;
  max--; // reserve one for 0-termination
  do {
    len = s->buf_len-s->buf_pos;
    // try to fill the buffer
    if(len <= 0 &&
       (!cache_stream_fill_buffer(s) ||
        (len = s->buf_len-s->buf_pos) <= 0)) break;
    end = find_term_char(s->buffer+s->buf_pos, len, term, utf16);
    if(end) len = end - (s->buffer+s->buf_pos) + 1;
    if(len > 0 && max > 0) {
      int l = copy_characters(ptr, max, s->buffer+s->buf_pos, &len, utf16);
      max -= l;
      ptr += l;
      if (!len)
        break;
    }
    s->buf_pos += len;
  } while(!end);
  ptr[0] = 0;
  if(s->eof && ptr == mem) return NULL;
  return mem;
}
