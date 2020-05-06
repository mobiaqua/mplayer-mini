/*
 * GyS-TermIO v2.0 (for GySmail v3)
 * a very small replacement of ncurses library
 *
 * copyright (C) 1999 A'rpi/ESP-team
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

#ifndef MPLAYER_GETCH2_H
#define MPLAYER_GETCH2_H

#include "config.h"

/* Screen size. Initialized by load_termcap() and get_screen_size() */
extern int screen_width;
extern int screen_height;

/* Termcap code to erase to end of line */
extern char * erase_to_end_of_line;

/* Get screen-size using IOCTL call. */
void get_screen_size(void);

/* Load key definitions from the TERMCAP database. 'termtype' can be NULL */
int load_termcap(char *termtype);

/* Enable and disable STDIN line-buffering */
void getch2_enable(void);
void getch2_disable(void);

/* Read a character or a special key code (see keycodes.h) */
void getch2(void);

/**
 * \brief gets the name of the system's terminal character set
 * \return a malloced string indicating the system charset
 *
 * Be warned that this function on many systems is in no way thread-safe
 * since it modifies global data
 */
char *get_term_charset(void);

#define USE_SELECT  1
#define MP_INPUT_SLAVE_CMD_FUNC     NULL

#endif /* MPLAYER_GETCH2_H */
