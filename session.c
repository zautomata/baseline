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

#include <stdio.h>		/* asprintf(3) */
#include <stdlib.h>		/* EXIT_* */
#include <limits.h>		/* PATH_MAX */
#include <unistd.h>		/* getcwd(3) */
#include <err.h>		/* errx(3) */

#include "defaults.h"
#include "config.h"
#include "session.h"

extern int objdb_baseline_get_ops(struct objdb_ops **);
extern int dircache_simple_get_ops(struct dircache_ops **);

int
baseline_session_begin(struct session *s, u_int8_t options)
{
	char *config = NULL;

	objdb_baseline_get_ops(&(s->db_ops));
	dircache_simple_get_ops(&(s->dc_ops));

	if (getcwd((char *)s->cwd, sizeof(s->cwd)) == NULL)
		errx(EXIT_FAILURE, "error, can not read current directory.");

	if (options & SESSION_NONINIT) {
		return EXIT_SUCCESS;
	}

	s->repo_rootdir = baseline_repo_get_rootdir();
	s->repo_baselinedir = baseline_repo_get_baselinedir();

	if (s->repo_rootdir == NULL)
		errx(EXIT_FAILURE, "error, no repository was found.");
	if (s->db_ops->open(&(s->db_ctx), BASELINE_DB, s->repo_baselinedir) == EXIT_FAILURE)
		return EXIT_FAILURE;
	if (s->dc_ops->open(&(s->dc_ctx), s->db_ctx, s->db_ops, s->repo_rootdir, s->repo_baselinedir) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/* load configurations */
	asprintf(&config, "%s/%s", s->repo_baselinedir, BASELINE_CONFIGFILE);
	if (baseline_config_load(config) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "failed to load configurations.");
	free(config);

	/* get current branch */
	s->dc_ops->branch_get(s->dc_ctx, (char **)&(s->branch));

	return EXIT_SUCCESS;
}

int
baseline_session_end(struct session *s)
{
	if (s->db_ops->close(s->db_ctx) == EXIT_FAILURE)
		return EXIT_FAILURE;
	if (s->dc_ops->close(s->dc_ctx) == EXIT_FAILURE)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

