/*
 * Get path to config dir/file.
 *
 * Return Values:
 *   Returns the pointer to the ALLOCATED buffer containing the
 *   zero terminated path string. This buffer has to be FREED
 *   by the caller.
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
#include "config.h"
#include "mp_msg.h"
#include "path.h"

char *get_path(const char *filename){
	// temporary buffer that will be freed
	char *tmp = 0;
	char *homedir;
	char *buff;
	const char *config_dir = "/.mplayer";
	int len;

	if ((homedir = getenv("MPLAYER_HOME")) != NULL)
		config_dir = "";
	else if ((homedir = getenv("HOME")) == NULL)
	{
		return NULL;
	}
	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}

	free(tmp);
	mp_msg(MSGT_GLOBAL,MSGL_V,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}

/**
 * @brief Returns the basename substring of a path.
 */
const char *mp_basename(const char *path)
{
    char *s;

    s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/**
 * @brief Allocates a new buffer containing the directory name
 * @param path Original path. Must be a valid string.
 *
 * @note The path returned always contains a trailing slash '/'.
 *       On systems supporting DOS paths, '\' is also considered as a directory
 *       separator in addition to the '/'.
 */
char *mp_dirname(const char *path)
{
    const char *base = mp_basename(path);
    size_t len = base - path;
    char *dirname;

    if (len == 0)
        return strdup("./");
    dirname = malloc(len + 1);
    if (!dirname)
        return NULL;
    strncpy(dirname, path, len);
    dirname[len] = '\0';
    return dirname;
}

/**
 * @brief Join two paths if path is not absolute.
 * @param base File or directory base path.
 * @param path Path to concatenate with the base.
 * @return New allocated string with the path, or NULL in case of error.
 * @warning Do not forget the trailing path separator at the end of the base
 *          path if it is a directory: since file paths are also supported,
 *          this separator will make the distinction.
 * @note Paths of the form c:foo, /foo or \foo will still depends on the
 *       current directory on Windows systems, even though they are considered
 *       as absolute paths in this function.
 */
char *mp_path_join(const char *base, const char *path)
{
    char *ret, *tmp;

    if (path[0] == '/')
        return strdup(path);

    ret = mp_dirname(base);
    if (!ret)
        return NULL;
    tmp = realloc(ret, strlen(ret) + strlen(path) + 1);
    if (!tmp) {
        free(ret);
        return NULL;
    }
    ret = tmp;
    strcat(ret, path);
    return ret;
}

/**
 * @brief Same as mp_path_join but always treat the first parameter as a
 *        directory.
 * @param dir Directory base path.
 * @param append Right part to append to dir.
 * @return New allocated string with the path, or NULL in case of error.
 */
char *mp_dir_join(const char *dir, const char *append)
{
    char *tmp, *ret;
    size_t dirlen = strlen(dir);
    size_t i      = dirlen - 1;

    if (dirlen == 0 || dir[i] == '/')
        return mp_path_join(dir, append);

    tmp = malloc(dirlen + 2);
    if (!tmp)
        return NULL;
    strcpy(tmp, dir);
    strcpy(tmp + dirlen, "/");
    ret = mp_path_join(tmp, append);
    free(tmp);
    return ret;
}
