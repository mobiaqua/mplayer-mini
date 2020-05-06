
#ifndef MPLAYER_CONFIG_H
#define MPLAYER_CONFIG_H


/* set up max. outburst. use 131072 for TrueHD SPDIF pass-through */
#define MAX_OUTBURST 131072

#define MPLAYER_DATADIR "/usr/share/mplayer"
#define MPLAYER_CONFDIR "/usr/etc/mplayer"

#define MSG_CHARSET "UTF-8"

/* configurable options */

#ifndef MP_DEBUG
#undef MP_DEBUG
#endif

#ifndef MP_DEBUG
#define CONFIG_SIGHANDLER 1
#else
#define CONFIG_SIGHANDLER 0
#endif

#ifndef MP_DEBUG
#undef CONFIG_CRASH_DEBUG
#endif


#endif /* MPLAYER_CONFIG_H */
