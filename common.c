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

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <sys/stat.h>

#include "defaults.h"
#include "common.h"


/* cache the repository root dir for future calls of baseline_repo_get_rootdir() */
static char *repo_rootdir = NULL;
static char *repo_baselinedir = NULL;		/* repo_rootdir/BASELINE_DIR */

int
exists(const char *f)
{
	struct stat s;

	if(stat(f, &s) == -1)
		return 0;
	return 1;
}

int
dir_exists(const char *dir)
{
	struct stat s;

	if (stat(dir, &s) == -1)
		return 0;
	if (!S_ISDIR(s.st_mode))
		return 0;
	return 1;
}

static int
baseline_validate_repo(const char *path)
{
	char *ptr = NULL;
	int valid;

	asprintf(&ptr, "%s/%s", path, BASELINE_DIR);
	if (ptr == NULL)
		return 0;
	valid = dir_exists(ptr);
	/* TODO: use access & check for modes as well */
	/* TODO: ask objdb, and may be dircache too */
	free(ptr);
	return valid;
}

/*
 * may not be so safe
 * do not free the returned pointer, it will be cached for later calls
 * try not to use chdir() as possible
 */
const char *
baseline_repo_get_rootdir()
{
	char cwd[PATH_MAX+1];
	int i;
	size_t len;

	/* check if the repository root dir already cached */
	if (repo_rootdir != NULL)
		return repo_rootdir;
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return NULL;
	len = strlen(cwd);
	for (i=len-1 ; i>=0 ; i--) {
		if (cwd[i] == '/') {
			if (baseline_validate_repo(cwd))
				break;
			cwd[i] = '\0';
		}
	}
	if (i == -1) {
		if (baseline_validate_repo(cwd))
			repo_rootdir = strdup("/");
		else
			return NULL;
	}
	else {
		repo_rootdir = strdup(cwd);
	}
#ifdef DEBUG
	printf("repository root dir: %s\n", repo_rootdir);
#endif
	return repo_rootdir;
}

const char*
baseline_repo_get_baselinedir()
{
	const char *root_dir;
	if (repo_baselinedir != NULL)
		return repo_baselinedir;
	root_dir = baseline_repo_get_rootdir();
	asprintf(&repo_baselinedir, "%s/%s", root_dir, BASELINE_DIR);
	return repo_baselinedir;
}

