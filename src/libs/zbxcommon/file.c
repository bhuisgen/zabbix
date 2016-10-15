/*
** Zabbix
** Copyright (C) 2001-2016 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "common.h"
#include "log.h"

#ifndef _WINDOWS
char	*CONFIG_ROOT_FILESYSTEM;

/******************************************************************************
 *                                                                            *
 * Function: __zbx_zbx_rootfs_path                                            *
 *                                                                            *
 * Purpose: convert a path to rootfs path                                     *
 *                                                                            *
 * Parameters: dst - destination buffer pointer                               *
 *             count - size of destination buffer                             *
 *             src - source buffer pointer                                    *
 *                                                                            *
 * Author: Boris HUISGEN                                                      *
 *                                                                            *
 ******************************************************************************/

void	__zbx_zbx_rootfs_path(char *dst, size_t count, const char *src)
{
	if ((CONFIG_ROOT_FILESYSTEM == NULL) || (strcmp(CONFIG_ROOT_FILESYSTEM, "/") == 0))
	{
		zbx_snprintf(dst, count, "%s", src);
	}
	else
	{
		zbx_snprintf(dst, count, "%s%s", CONFIG_ROOT_FILESYSTEM, src);
	}

	zabbix_log(LOG_LEVEL_DEBUG, "Rootfs path conversion: \"%s\" -> \"%s\"", src, dst);
}

#endif

int	zbx_stat(const char *path, zbx_stat_t *buf)
{
#ifdef _WINDOWS
	int	ret, fd;
	wchar_t	*wpath;

	wpath = zbx_utf8_to_unicode(path);

	if (-1 == (ret = _wstat64(wpath, buf)))
		goto out;

	if (0 != S_ISDIR(buf->st_mode) || 0 != buf->st_size)
		goto out;

	/* In the case of symlinks _wstat64 returns zero file size.   */
	/* Try to work around it by opening the file and using fstat. */

	ret = -1;

	if (-1 != (fd = _wopen(wpath, O_RDONLY)))
	{
		ret = _fstat64(fd, buf);
		_close(fd);
	}
out:
	zbx_free(wpath);

	return ret;
#else
	char 	rootpath[MAX_STRING_LEN];
	int	ret;

	__zbx_zbx_rootfs_path(rootpath, sizeof(rootpath), path);

	ret = stat(rootpath, buf);

	return ret;
#endif
}

int	zbx_open(const char *pathname, int flags)
{
#ifdef _WINDOWS
	int	ret;
	wchar_t	*wpathname;

	wpathname = zbx_utf8_to_unicode(pathname);
	ret = _wopen(wpathname, flags | O_BINARY);
	zbx_free(wpathname);

	return ret;
#else
	char 	rootpath[MAX_STRING_LEN];
	int	ret;

	__zbx_zbx_rootfs_path(rootpath, sizeof(rootpath), pathname);

	ret = open(rootpath, flags);

	return ret;
#endif
}

int	zbx_close(int fd)
{
#ifdef _WINDOWS
	int ret;

	ret = _close(fd);

	return ret;
#else
	int ret;

	ret = close(fd);

	return ret;
#endif
}


/******************************************************************************
 *                                                                            *
 * Function: zbx_read                                                         *
 *                                                                            *
 * Purpose: Read one text line from a file descriptor into buffer             *
 *                                                                            *
 * Parameters: fd       - [IN] file descriptor to read from                   *
 *             buf      - [IN] buffer to read into                            *
 *             count    - [IN] buffer size in bytes                           *
 *             encoding - [IN] pointer to a text string describing encoding.  *
 *                        The following encodings are recognized:             *
 *                          "UNICODE"                                         *
 *                          "UNICODEBIG"                                      *
 *                          "UNICODEFFFE"                                     *
 *                          "UNICODELITTLE"                                   *
 *                          "UTF-16"   "UTF16"                                *
 *                          "UTF-16BE" "UTF16BE"                              *
 *                          "UTF-16LE" "UTF16LE"                              *
 *                          "UTF-32"   "UTF32"                                *
 *                          "UTF-32BE" "UTF32BE"                              *
 *                          "UTF-32LE" "UTF32LE".                             *
 *                        "" (empty string) means a single-byte character set.*
 *                                                                            *
 * Return value: On success, the number of bytes read is returned (0 (zero)   *
 *               indicates end of file).                                      *
 *               On error, -1 is returned and errno is set appropriately.     *
 *                                                                            *
 * Comments: Reading stops after a newline. If the newline is read, it is     *
 *           stored into the buffer.                                          *
 *                                                                            *
 ******************************************************************************/
int	zbx_read(int fd, char *buf, size_t count, const char *encoding)
{
	size_t		i, szbyte;
	const char	*cr, *lf;
	int		nbytes;
	zbx_offset_t	offset;

	if ((zbx_offset_t)-1 == (offset = zbx_lseek(fd, 0, SEEK_CUR)))
		return -1;

	if (0 >= (nbytes = (int)read(fd, buf, count)))
		return nbytes;

	find_cr_lf_szbyte(encoding, &cr, &lf, &szbyte);

	for (i = 0; i <= (size_t)nbytes - szbyte; i += szbyte)
	{
		if (0 == memcmp(&buf[i], lf, szbyte))	/* LF (Unix) */
		{
			i += szbyte;
			break;
		}

		if (0 == memcmp(&buf[i], cr, szbyte))	/* CR (Mac) */
		{
			/* CR+LF (Windows) ? */
			if (i < (size_t)nbytes - szbyte && 0 == memcmp(&buf[i + szbyte], lf, szbyte))
				i += szbyte;

			i += szbyte;
			break;
		}
	}

	if ((zbx_offset_t)-1 == zbx_lseek(fd, offset + (zbx_offset_t)i, SEEK_SET))
		return -1;

	return (int)i;
}

#ifndef _WINDOWS

ssize_t zbx_readlink(const char *pathname, char *buf, size_t bufsiz)
{
	char 	rootpath[MAX_STRING_LEN];
	ssize_t bytes;

	__zbx_zbx_rootfs_path(rootpath, sizeof(rootpath), pathname);

	bytes = readlink(rootpath, buf, bufsiz);

	return bytes;
}

DIR*	zbx_opendir(const char *name)
{
	char 	rootpath[MAX_STRING_LEN];
	DIR*	dir;

	__zbx_zbx_rootfs_path(rootpath, sizeof(rootpath), name);

	dir = opendir(rootpath);

	return dir;
}

int	zbx_closedir(DIR *dirp)
{
	int ret;

	ret = closedir(dirp);

	return ret;
}


struct dirent*	zbx_readdir(DIR *dirp)
{
	struct dirent	*d_ent;

	d_ent = readdir(dirp);

	return d_ent;
}

#endif

FILE*	zbx_fopen(const char *pathname, const char *mode)
{
#ifdef _WINDOWS
	FILE*	fd;
	wchar_t	*pathname;
	wchar_t	*wmode;

	wpathname = zbx_utf8_to_unicode(path);
	wmode = zbx_utf8_to_unicode(mode);
	fd = _wfopen(wpathname, wmode);

	zbx_free(wpathname);
	zbx_free(wmode);

	return fd;
#else
	char 	rootpath[MAX_STRING_LEN];
	FILE*	fd;

	__zbx_zbx_rootfs_path(rootpath, sizeof(rootpath), pathname);

	fd = fopen(rootpath, mode);

	return fd;
#endif
}

int	zbx_fclose(FILE *file)
{
	int ret=0;

	if (file)
	{
	    ret = fclose(file);
	    file = NULL;
	}

	return ret;
}

void	find_cr_lf_szbyte(const char *encoding, const char **cr, const char **lf, size_t *szbyte)
{
	/* default is single-byte character set */
	*cr = "\r";
	*lf = "\n";
	*szbyte = 1;

	if ('\0' != *encoding)
	{
		if (0 == strcasecmp(encoding, "UNICODE") || 0 == strcasecmp(encoding, "UNICODELITTLE") ||
				0 == strcasecmp(encoding, "UTF-16") || 0 == strcasecmp(encoding, "UTF-16LE") ||
				0 == strcasecmp(encoding, "UTF16") || 0 == strcasecmp(encoding, "UTF16LE"))
		{
			*cr = "\r\0";
			*lf = "\n\0";
			*szbyte = 2;
		}
		else if (0 == strcasecmp(encoding, "UNICODEBIG") || 0 == strcasecmp(encoding, "UNICODEFFFE") ||
				0 == strcasecmp(encoding, "UTF-16BE") || 0 == strcasecmp(encoding, "UTF16BE"))
		{
			*cr = "\0\r";
			*lf = "\0\n";
			*szbyte = 2;
		}
		else if (0 == strcasecmp(encoding, "UTF-32") || 0 == strcasecmp(encoding, "UTF-32LE") ||
				0 == strcasecmp(encoding, "UTF32") || 0 == strcasecmp(encoding, "UTF32LE"))
		{
			*cr = "\r\0\0\0";
			*lf = "\n\0\0\0";
			*szbyte = 4;
		}
		else if (0 == strcasecmp(encoding, "UTF-32BE") || 0 == strcasecmp(encoding, "UTF32BE"))
		{
			*cr = "\0\0\0\r";
			*lf = "\0\0\0\n";
			*szbyte = 4;
		}
	}
}

int	zbx_is_regular_file(const char *path)
{
	zbx_stat_t	st;

	if (0 == zbx_stat(path, &st) && 0 != S_ISREG(st.st_mode))
		return SUCCEED;

	return FAIL;
}

