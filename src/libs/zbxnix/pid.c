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
#include "pid.h"
#include "log.h"
#include "threads.h"
static FILE	*fpid = NULL;
static int	fdpid = -1;

int	create_pid_file(const char *pidfile)
{
	int		fd;
	zbx_stat_t	buf;
	struct flock	fl;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = getpid();

	/* check if pid file already exists */
#ifdef _WINDOWS
	int	stat, fd;
	wchar_t	*wpath;

	wpath = zbx_utf8_to_unicode(pidfile);

	if (-1 == (stat = _wstat64(wpath, &buf)))
		goto out;

	if (0 != S_ISDIR(buf->st_mode) || 0 != buf->st_size)
		goto out;

	/* In the case of symlinks _wstat64 returns zero file size.   */
	/* Try to work around it by opening the file and using fstat. */

	stat = -1;

	if (-1 != (fd = _wopen(wpath, O_RDONLY)))
	{
		stat = _fstat64(fd, &buf);
		_close(fd);
	}
out:
	zbx_free(wpath);

	if (0 == stat)
#else
	if (0 == stat(pidfile, &buf))
#endif
	{
		if (-1 == (fd = open(pidfile, O_WRONLY | O_APPEND)))
		{
			zbx_error("cannot open PID file [%s]: %s", pidfile, zbx_strerror(errno));
			return FAIL;
		}

		if (-1 == fcntl(fd, F_SETLK, &fl))
		{
			close(fd);
			zbx_error("Is this process already running? Could not lock PID file [%s]: %s",
					pidfile, zbx_strerror(errno));
			return FAIL;
		}

		close(fd);
	}

	/* open pid file */
	if (NULL == (fpid = fopen(pidfile, "w")))
	{
		zbx_error("cannot create PID file [%s]: %s", pidfile, zbx_strerror(errno));
		return FAIL;
	}

	/* lock file */
	if (-1 != (fdpid = fileno(fpid)))
	{
		fcntl(fdpid, F_SETLK, &fl);
		fcntl(fdpid, F_SETFD, FD_CLOEXEC);
	}

	/* write pid to file */
	fprintf(fpid, "%d", (int)getpid());
	fflush(fpid);

	return SUCCEED;
}

int	read_pid_file(const char *pidfile, pid_t *pid, char *error, size_t max_error_len)
{
	int	ret = FAIL;
	FILE	*fpid;

	if (NULL == (fpid = fopen(pidfile, "r")))
	{
		zbx_snprintf(error, max_error_len, "cannot open PID file [%s]: %s", pidfile, zbx_strerror(errno));
		return ret;
	}

	if (1 == fscanf(fpid, "%d", (int *)pid))
		ret = SUCCEED;
	else
		zbx_snprintf(error, max_error_len, "cannot retrieve PID from file [%s]", pidfile);

	zbx_fclose(fpid);

	return ret;
}

void	drop_pid_file(const char *pidfile)
{
	struct flock	fl;

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = zbx_get_thread_id();

	/* unlock file */
	if (-1 != fdpid)
		fcntl(fdpid, F_SETLK, &fl);

	/* close pid file */
	zbx_fclose(fpid);

	unlink(pidfile);
}
