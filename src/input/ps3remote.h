/*
 * PS3 BD Remote input interface
 *
 * Copyright (C) 2021 Pawel Kolodziejski
 * Copyright (C) 2007 Zoltan Ponekker <pontscho at kac.poliod.hu>
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

#ifndef MPLAYER_PS3REMOTE_H
#define MPLAYER_PS3REMOTE_H

int mp_input_ps3remote_init(void);
void mp_input_ps3remote_close(int fd);
int mp_input_ps3remote_read(int fd);

#endif /* MPLAYER_PS3REMOTE_H */
