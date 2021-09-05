/*
 * Linux PS3 BD Remote input interface
 *
 * Copyright (C) 2021 Pawel Kolodziejski
 * Copyright (C) 2008 Benjamin Zores <ben at geexbox dot org>
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

#include "config.h"
#include "input.h"
#include "ps3remote.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/types.h>
#include <linux/input.h>

#include "mp_msg.h"
#include "help_mp.h"

#define EVDEV_MAX_EVENTS 32

#define USB_VENDOR_PS3REMOTE          0x054C
#define USB_DEV_PS3REMOTE             0x0306

static const struct {
  int linux_keycode;
  int mp_keycode;
} ps3remote_mapping[] = {
  { KEY_ESC,           'q'               },
//  { KEY_1,             KEY_1            },
//  { KEY_2,             KEY_2            },
//  { KEY_3,             KEY_3            },
//  { KEY_4,             KEY_4            },
//  { KEY_5,             KEY_5            },
//  { KEY_6,             KEY_6            },
//  { KEY_7,             KEY_7            },
//  { KEY_8,             KEY_8            },
//  { KEY_9,             KEY_9            },
//  { KEY_0,             KEY_0            },
  { KEY_ENTER,         ' '              },
  { KEY_UP,            0x1000013        },
  { KEY_LEFT,          0x1000011        },
  { KEY_RIGHT,         0x1000010        },
  { KEY_DOWN,          0x1000012        },
  { KEY_PAUSE,         ' '              },
  { KEY_STOP,          'q'              },
//  { KEY_MENU,          KEY_MENU         },
//  { KEY_BACK,          KEY_BACK         },
//  { KEY_FORWARD,       KEY_FORWARD      },
//  { KEY_EJECTCD,       KEY_EJECTCD      },
//  { KEY_REWIND,        KEY_REWIND       },
//  { KEY_HOMEPAGE,      KEY_HOMEPAGE     },
  { KEY_PLAY,          ' '              },
//  { BTN_0,             BTN_0            },
//  { BTN_TL,            BTN_TL           },
//  { BTN_TR,            BTN_TR           },
//  { BTN_TL2,           BTN_TL2          },
//  { BTN_TR2,           BTN_TR2          },
//  { BTN_START,         BTN_START        },
//  { BTN_THUMBL,        BTN_THUMBL       },
//  { BTN_THUMBR,        BTN_THUMBR       },
//  { KEY_SELECT,        KEY_SELECT       },
//  { KEY_CLEAR,         KEY_CLEAR        },
//  { KEY_OPTION,        KEY_OPTION       },
//  { KEY_INFO,          KEY_INFO         },
//  { KEY_TIME,          KEY_TIME         },
//  { KEY_SUBTITLE,      KEY_SUBTITLE     },
//  { KEY_ANGLE,         KEY_ANGLE        },
//  { KEY_SCREEN,        KEY_SCREEN       },
//  { KEY_AUDIO,         KEY_AUDIO        },
//  { KEY_RED,           KEY_RED          },
//  { KEY_GREEN,         KEY_GREEN        },
//  { KEY_YELLOW,        KEY_YELLOW       },
//  { KEY_BLUE,          KEY_BLUE         },
//  { KEY_NEXT,          KEY_NEXT         },
//  { KEY_PREVIOUS,      KEY_PREVIOUS     },
//  { KEY_FRAMEBACK,     KEY_FRAMEBACK    },
//  { KEY_FRAMEFORWARD,  KEY_FRAMEFORWARD },
//  { KEY_CONTEXT_MENU,  KEY_CONTEXT_MENU },
  { -1,                  -1             }
};

int mp_input_ps3remote_init (char *dev)
{
  int i, fd;

  if (dev)
  {
    mp_msg (MSGT_INPUT, MSGL_V, "Initializing PS3 BD Remote on %s\n", dev);
    fd = open (dev, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
      mp_msg (MSGT_INPUT, MSGL_ERR,
              "Can't open PS3 BD Remote device: %s\n", strerror (errno));
      return -1;
    }

    return fd;
  }
  else
  {
    /* look for a valid PS3 BD Remote device on system */
    for (i = 0; i < EVDEV_MAX_EVENTS; i++)
    {
      struct input_id id;
      char file[64];

      sprintf (file, "/dev/input/event%d", i);
      fd = open (file, O_RDONLY | O_NONBLOCK);
      if (fd < 0)
        continue;

      if (ioctl(fd, EVIOCGID, &id) != -1 &&
          id.bustype == BUS_BLUETOOTH &&
          id.vendor  == USB_VENDOR_PS3REMOTE &&
          id.product == USB_DEV_PS3REMOTE)
      {
        mp_msg (MSGT_INPUT, MSGL_V, "Detected PS3 BD Remote on %s\n", file);
        return fd;
      }
      close (fd);
    }

    mp_msg (MSGT_INPUT, MSGL_ERR,
            "Can't open PS3 BD Remote device: %s\n", strerror (errno));
  }

  return -1;
}

int mp_input_ps3remote_read (int fd)
{
  struct input_event ev;
  int i, r;

  r = read (fd, &ev, sizeof (struct input_event));
  if (r <= 0 || r < sizeof (struct input_event))
    return MP_INPUT_NOTHING;

  /* check for key press only */
  if (ev.type != EV_KEY)
    return MP_INPUT_NOTHING;

  /* EvDev Key values:
   *  0: key release
   *  1: key press
   */
  if (ev.value == 0)
    return MP_INPUT_NOTHING;

//printf("%d\n\n", ev.code);

  /* find Linux evdev -> MPlayer keycode mapping */
  for (i = 0; ps3remote_mapping[i].linux_keycode != -1; i++)
    if (ps3remote_mapping[i].linux_keycode == ev.code)
      return ps3remote_mapping[i].mp_keycode;

  return MP_INPUT_NOTHING;
}
