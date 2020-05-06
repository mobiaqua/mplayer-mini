/*
 * generic alpha renderers for all YUV modes and RGB depths
 * Optimized by Nick and Michael.
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

static inline void RENAME(vo_draw_alpha_yv12)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]) dstbase[x]=((dstbase[x]*srca[x])>>8)+src[x];
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
}

static inline void RENAME(vo_draw_alpha_yuy2)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]) {
               dstbase[2*x]=((dstbase[2*x]*srca[x])>>8)+src[x];
               dstbase[2*x+1]=((((signed)dstbase[2*x+1]-128)*srca[x])>>8)+128;
           }
        }
	src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
}

static inline void RENAME(vo_draw_alpha_uyvy)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
  int y;
  for(y=0;y<h;y++){
    register int x;
    for(x=0;x<w;x++){
      if(srca[x]) {
	dstbase[2*x+1]=((dstbase[2*x+1]*srca[x])>>8)+src[x];
	dstbase[2*x]=((((signed)dstbase[2*x]-128)*srca[x])>>8)+128;
      }
    }
    src+=srcstride;
    srca+=srcstride;
    dstbase+=dststride;
  }
}

static inline void RENAME(vo_draw_alpha_rgb24)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned char *dst = dstbase;
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
		dst[0]=((dst[0]*srca[x])>>8)+src[x];
		dst[1]=((dst[1]*srca[x])>>8)+src[x];
		dst[2]=((dst[2]*srca[x])>>8)+src[x];
            }
            dst+=3; // 24bpp
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
}

static inline void RENAME(vo_draw_alpha_rgb32)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
		dstbase[4*x+0]=((dstbase[4*x+0]*srca[x])>>8)+src[x];
		dstbase[4*x+1]=((dstbase[4*x+1]*srca[x])>>8)+src[x];
		dstbase[4*x+2]=((dstbase[4*x+2]*srca[x])>>8)+src[x];
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
}
