/*
 * libvo common functions, variables used by many/all drivers.
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "config.h"
#include "video_out.h"
#include "aspect.h"
#include "geometry.h"

#include "mp_msg.h"
#include "help_mp.h"
#include "input/input.h"
#include "osdep/shmem.h"

//int vo_flags=0;

int xinerama_screen = -1;
int xinerama_x;
int xinerama_y;

// currect resolution/bpp on screen:  (should be autodetected by vo_init())
int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;

int vo_config_count=0;

// requested resolution/bpp:  (-x -y -bpp options)
int vo_dx=0;
int vo_dy=0;
int vo_dwidth=0;
int vo_dheight=0;
int vo_dbpp=0;

int vo_nomouse_input = 0;
int vo_grabpointer = 1;
int vo_doublebuffering = 1;
int vo_vsync = 0;
int vo_fs = 0;
int vo_fsmode = 0;
float vo_panscan = 0.0f;
float vo_border_pos_x = 0.5;
float vo_border_pos_y = 0.5;
int vo_fs_border_l = 0;
int vo_fs_border_r = 0;
int vo_fs_border_t = 0;
int vo_fs_border_b = 0;
int vo_rotate;
int vo_ontop = 0;
int vo_adapter_num=0;
int vo_refresh_rate=0;
int vo_keepaspect=1;
int vo_rootwin=0;
int vo_border=1;
int64_t WinID = -1;

int vo_pts=0; // for hw decoding
float vo_fps=0;

char *vo_subdevice = NULL;
int vo_directrendering=0;

int vo_colorkey = 0x0000ff00; // default colorkey is green
                              // (0xff000000 means that colorkey has been disabled)

// name to be used instead of the vo's default
char *vo_winname;
// title to be applied to movie window
char *vo_wintitle;

//
// Externally visible list of all vo drivers
//
extern const vo_functions_t video_out_omap_drm;
extern const vo_functions_t video_out_omap_drm_egl;

const vo_functions_t* const video_out_drivers[] =
{
        &video_out_omap_drm,
        &video_out_omap_drm_egl,
        NULL
};

void list_video_out(void){
      int i=0;
      mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_AvailableVideoOutputDrivers);
      mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_OUTPUTS\n");
      while (video_out_drivers[i]) {
        const vo_info_t *info = video_out_drivers[i++]->info;
        mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n", info->short_name, info->name);
      }
      mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

const vo_functions_t* init_best_video_out(char** vo_list){
    int i;
    // first try the preferred drivers, with their optional subdevice param:
    if(vo_list && vo_list[0])
      while(vo_list[0][0]){
        char* buffer=strdup(vo_list[0]);
        char *vo = buffer;
	vo_subdevice=strchr(vo,':');
	if(vo_subdevice){
	    vo_subdevice[0]=0;
	    ++vo_subdevice;
	}
	if (!strcmp(vo, "pgm"))
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_PGM_HasBeenReplaced);
	if (!strcmp(vo, "md5"))
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_MD5_HasBeenReplaced);
	if (!strcmp(vo, "gl2")) {
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_GL2_HasBeenRenamed);
	    vo = "gl_tiled";
        }
	for(i=0;video_out_drivers[i];i++){
	    const vo_functions_t* video_driver=video_out_drivers[i];
	    const vo_info_t *info = video_driver->info;
	    if(!strcmp(info->short_name,vo)){
		// name matches, try it
		if(!video_driver->preinit(vo_subdevice))
		{
		    free(buffer);
		    return video_driver; // success!
		}
	    }
	}
        // continue...
	free(buffer);
	++vo_list;
	if(!(vo_list[0])) return NULL; // do NOT fallback to others
      }
    // now try the rest...
    vo_subdevice=NULL;
    for(i=0;video_out_drivers[i];i++){
	const vo_functions_t* video_driver=video_out_drivers[i];
	if(!video_driver->preinit(vo_subdevice))
	    return video_driver; // success!
    }
    return NULL;
}

int config_video_out(const vo_functions_t *vo, uint32_t width, uint32_t height,
                     uint32_t d_width, uint32_t d_height, uint32_t flags,
                     char *title, uint32_t format) {
  panscan_init();
  aspect_save_orig(width,height);
  aspect_save_prescale(d_width,d_height);

  if (vo->control(VOCTRL_UPDATE_SCREENINFO, NULL) == VO_TRUE) {
    aspect(&d_width,&d_height,A_NOZOOM);
    vo_dx = (int)(vo_screenwidth - d_width) / 2;
    vo_dy = (int)(vo_screenheight - d_height) / 2;
    geometry(&vo_dx, &vo_dy, &d_width, &d_height,
             vo_screenwidth, vo_screenheight);
    geometry_xy_changed |= xinerama_screen >= 0;
    vo_dx += xinerama_x;
    vo_dy += xinerama_y;
    vo_dwidth = d_width;
    vo_dheight = d_height;
  }

  return vo->config(width, height, d_width, d_height, flags, title, format);
}

/**
 * \brief lookup an integer in a table, table must have 0 as the last key
 * \param key key to search for
 * \result translation corresponding to key or "to" value of last mapping
 *         if not found.
 */
int lookup_keymap_table(const struct mp_keymap *map, int key) {
  while (map->from && map->from != key) map++;
  return map->to;
}

/**
 * \brief helper function for the kind of panscan-scaling that needs a source
 *        and destination rectangle like Direct3D and VDPAU
 */
static void src_dst_split_scaling(int src_size, int dst_size, int scaled_src_size,
                                  float bpos,
                                  int *src_start, int *src_end, int *dst_start, int *dst_end) {
  if (scaled_src_size > dst_size) {
    int border = src_size * (scaled_src_size - dst_size) / scaled_src_size;
    // round to a multiple of 2, this is at least needed for vo_direct3d and ATI cards
    border = (border / 2 + 1) & ~1;
    *src_start = border;
    *src_end   = src_size - border;
    *dst_start = 0;
    *dst_end   = dst_size;
  } else {
    *src_start = 0;
    *src_end   = src_size;
    *dst_start = apply_border_pos(dst_size, scaled_src_size, bpos);
    *dst_end   = *dst_start + scaled_src_size;
  }
}

/**
 * Calculate the appropriate source and destination rectangle to
 * get a correctly scaled picture, including pan-scan.
 * Can be extended to take future cropping support into account.
 *
 * \param crop specifies the cropping border size in the left, right, top and bottom members, may be NULL
 * \param borders the border values as e.g. EOSD (ASS) and properly placed DVD highlight support requires,
 *                may be NULL and only left and top are currently valid.
 */
void calc_src_dst_rects(int src_width, int src_height, struct vo_rect *src, struct vo_rect *dst,
                        struct vo_rect *borders, const struct vo_rect *crop) {
  static const struct vo_rect no_crop = {0, 0, 0, 0, 0, 0};
  int scaled_width  = 0;
  int scaled_height = 0;
  if (!crop) crop = &no_crop;
  src_width  -= crop->left + crop->right;
  src_height -= crop->top  + crop->bottom;
  if (src_width  < 2) src_width  = 2;
  if (src_height < 2) src_height = 2;
  dst->left = 0; dst->right  = vo_dwidth;
  dst->top  = 0; dst->bottom = vo_dheight;
  src->left = 0; src->right  = src_width;
  src->top  = 0; src->bottom = src_height;
  if (borders) {
    borders->left = 0; borders->top = 0;
  }
  if (aspect_scaling()) {
    aspect(&scaled_width, &scaled_height, A_WINZOOM);
    panscan_calc_windowed();
    scaled_width  += vo_panscan_x;
    scaled_height += vo_panscan_y;
    if (borders) {
      borders->left = apply_border_pos(vo_dwidth,  scaled_width,  vo_border_pos_x);
      borders->top  = apply_border_pos(vo_dheight, scaled_height, vo_border_pos_y);
      borders->right  = vo_dwidth  - scaled_width  - borders->left;
      borders->bottom = vo_dheight - scaled_height - borders->top;
    }
    src_dst_split_scaling(src_width, vo_dwidth, scaled_width, vo_border_pos_x,
                          &src->left, &src->right, &dst->left, &dst->right);
    src_dst_split_scaling(src_height, vo_dheight, scaled_height, vo_border_pos_y,
                          &src->top, &src->bottom, &dst->top, &dst->bottom);
  }
  src->left += crop->left; src->right  += crop->left;
  src->top  += crop->top;  src->bottom += crop->top;
  src->width  = src->right  - src->left;
  src->height = src->bottom - src->top;
  dst->width  = dst->right  - dst->left;
  dst->height = dst->bottom - dst->top;
}

/**
 * Generates a mouse movement message if those are enable and sends it
 * to the "main" MPlayer.
 *
 * \param posx new x position of mouse
 * \param posy new y position of mouse
 */
void vo_mouse_movement(int posx, int posy) {
  char cmd_str[40];
  if (!enable_mouse_movements)
    return;
  snprintf(cmd_str, sizeof(cmd_str), "set_mouse_pos %i %i", posx, posy);
  mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
}
