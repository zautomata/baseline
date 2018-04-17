/*
 * Copyright (c) 2014 Mohamed Aslan <maslan@sce.carleton.ca>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/stat.h>		/* stat(2) */
#include <stdio.h>
#include <stdlib.h>		/* EXIT_FAILURE */
#include <string.h>		/* strstr(3) */
#include <limits.h>		/* PATH_MAX */
#include <err.h>		/* errx(3) */

#include "session.h"
#include "cmd.h"


static char *
xrealpath(const char *path)
{
	char abspath[PATH_MAX+1];
	if (realpath(path, abspath) == NULL)
		errx(EXIT_FAILURE, "error, failed to resolve path.");
	return strdup(abspath);
}

/*
 * checks if first is subdir of second
 */
static int
is_subdir_of(const char *first, const char *second)
{
	if (first == NULL || second == NULL)
		return 0;
	if (strstr(first, second) == first)
		return 1;
	return 0;
}

int
cmd_add(int argc, char **argv)
{
	char *path, *real;
	struct session s;
	struct stat st;

	baseline_session_begin(&s, 0);

	path = argv[1];
	if (path == NULL)
		errx(EXIT_FAILURE, "error, no path specified.");
	/* all paths send to the backends must be non-relative paths */
	real = xrealpath(path);
	if (!is_subdir_of(real, s.repo_rootdir))
		errx(EXIT_FAILURE, "error, can not add files from outside the repository.");
	if (stat(path, &st) == -1)
		errx(EXIT_FAILURE, "error, \'%s\' does not exist.", path);
	if (s.dc_ops->insert(s.dc_ctx, real) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "failed to add \'%s\'.", path);

	baseline_session_end(&s);
	return EXIT_SUCCESS;
}

