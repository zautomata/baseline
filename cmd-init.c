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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h> /* EXIT_FAILURE */
#include <string.h> /* strstr(3) */
#include <limits.h> /* PATH_MAX */
#include <fcntl.h>  /* open(2) */
#include <unistd.h> /* getcwd(3) */
#include <err.h>    /* errx(3) */

#include "defaults.h"
#include "session.h"
#include "config.h"
#include "cmd.h"

/*
 * creates a new repository in the given path.
 */
static int
init(struct session *s) {
	char *baseline_path, *config_path;

	/* create '.baseline' dir */
	asprintf(&baseline_path, "%s/%s", s->cwd, BASELINE_DIR);
	if (mkdir(baseline_path, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
		return EXIT_FAILURE;
	}
	/* create '.baseline/config file */
	asprintf(&config_path, "%s/config", baseline_path);
	if (baseline_config_create(config_path) == EXIT_FAILURE)
		return EXIT_FAILURE;
	/* FIXME: error checks */

	/* let objdb initialize it's structures too */
	s->db_ops->open(&(s->db_ctx), BASELINE_DB, baseline_path);
	s->db_ops->init(s->db_ctx);
	/* as well as dircache */
	s->dc_ops->open(&(s->dc_ctx), s->db_ctx, s->db_ops, s->cwd, baseline_path);
	s->dc_ops->init(s->dc_ctx);

	free(config_path);
	free(baseline_path);
	return EXIT_SUCCESS;
}

int
cmd_init(int argc, char **argv)
{
	struct session s;

	baseline_session_begin(&s, SESSION_NONINIT);
	s.repo_rootdir = baseline_repo_get_rootdir();

	if (s.repo_rootdir != NULL)
		errx(EXIT_SUCCESS, "repository already initialized.");
	if (exists(s.repo_baselinedir))
		errx(EXIT_FAILURE, "error, corrupted repository already exists.");
	if (init(&s) == EXIT_FAILURE) 
		errx(EXIT_FAILURE, "error, could not initialize repository.");
	printf("baseline: repository successfully initialized at \'%s\'.\n", s.cwd);

	baseline_session_end(&s);
	return EXIT_SUCCESS;
}
