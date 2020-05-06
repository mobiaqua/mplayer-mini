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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "audio_out.h"

#include "mp_msg.h"
#include "help_mp.h"
#include "mp_core.h" /* for exit_player() */

/* set up audio OUTBURST. Do not change this! */
#define OUTBURST 512

// there are some globals:
ao_data_t ao_data={0,0,0,0,OUTBURST,-1,0};
char *ao_subdevice = NULL;

extern const ao_functions_t audio_out_null;
extern const ao_functions_t audio_out_alsa;

const ao_functions_t* const audio_out_drivers[] =
{
        &audio_out_alsa,
        &audio_out_null,
        NULL
};

void list_audio_out(void){
    int i=0;
    mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AvailableAudioOutputDrivers);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_OUTPUTS\n");
    while (audio_out_drivers[i]) {
        const ao_info_t *info = audio_out_drivers[i++]->info;
        mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n", info->short_name, info->name);
    }
    mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

static int init_wrapper(const ao_functions_t *ao, int rate, int channels, int format, int flags)
{
    int res = ao->init(rate, channels, format, flags);
    if (ao_data.format == (AF_FORMAT_U8 ^ AF_FORMAT_LE) ||
        ao_data.format == (AF_FORMAT_S8 ^ AF_FORMAT_LE))
        ao_data.format ^= AF_FORMAT_LE;
    return res;
}

const ao_functions_t* init_best_audio_out(char** ao_list,int use_plugin,int rate,int channels,int format,int flags){
    int i;
    // first try the preferred drivers, with their optional subdevice param:
    if(ao_list && ao_list[0])
      while(ao_list[0][0]){
        char* ao=ao_list[0];
        int ao_len;
        if (strncmp(ao, "alsa9", 5) == 0 || strncmp(ao, "alsa1x", 6) == 0) {
            mp_msg(MSGT_AO, MSGL_FATAL, MSGTR_AO_ALSA9_1x_Removed);
            exit_player(EXIT_NONE);
        }
        free(ao_subdevice);
        ao_subdevice = NULL;
        ao_subdevice=strchr(ao,':');
        if(ao_subdevice){
            ao_len = ao_subdevice - ao;
            ao_subdevice = strdup(&ao[ao_len + 1]);
        }
        else
            ao_len = strlen(ao);

        mp_msg(MSGT_AO, MSGL_V, "Trying preferred audio driver '%.*s', options '%s'\n",
               ao_len, ao, ao_subdevice ? ao_subdevice : "[none]");

        for(i=0;audio_out_drivers[i];i++){
            const ao_functions_t* audio_out=audio_out_drivers[i];
            if(!strncmp(audio_out->info->short_name,ao,ao_len)){
                // name matches, try it
                if(init_wrapper(audio_out,rate,channels,format,flags))
                    return audio_out; // success!
                else
                    mp_msg(MSGT_AO, MSGL_WARN, MSGTR_AO_FailedInit, ao);
                break;
            }
        }
	if (!audio_out_drivers[i]) // we searched through the entire list
            mp_msg(MSGT_AO, MSGL_WARN, MSGTR_AO_NoSuchDriver, ao_len, ao);
        // continue...
        ++ao_list;
        if(!(ao_list[0])) return NULL; // do NOT fallback to others
      }
    free(ao_subdevice);
    ao_subdevice = NULL;

    mp_msg(MSGT_AO, MSGL_V, "Trying every known audio driver...\n");

    // now try the rest...
    for(i=0;audio_out_drivers[i];i++){
        const ao_functions_t* audio_out=audio_out_drivers[i];
//        if(audio_out->control(AOCONTROL_QUERY_FORMAT, (int)format) == CONTROL_TRUE)
        if(init_wrapper(audio_out,rate,channels,format,flags))
            return audio_out; // success!
    }
    return NULL;
}

void mp_ao_resume_refill(const ao_functions_t *ao, int prepause_space)
{
    int fillcnt = ao->get_space() - prepause_space;
    if (fillcnt > 0 && !(ao_data.format & AF_FORMAT_SPECIAL_MASK)) {
      void *silence = calloc(fillcnt, 1);
      ao->play(silence, fillcnt, 0);
      free(silence);
    }
}
