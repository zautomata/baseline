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
#include <stdlib.h> /* EXIT_FAILURE */
#include <string.h> /* strstr(3) */
#include <limits.h> /* PATH_MAX */
#include <fcntl.h>  /* open(2) */
#include <unistd.h> /* getcwd(3) */
#include <err.h>    /* errx(3) */

#include "defaults.h"
#include "session.h"
#include "cmd.h"

extern int objdb_baseline_get_ops(struct objdb_ops **);
extern int dircache_simple_get_ops(struct dircache_ops **);

int cmd_branch(int, char **);

static int
checkout(struct session *s, const char *branch_name) {
	return EXIT_SUCCESS;
}

int
cmd_checkout(int argc, char **argv)
{
	struct session s;

	baseline_session_begin(&s, 0);

	if (argc == 2) {	/* 1 arg */
		if (checkout(&s, argv[1]) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "failed to checkout branch \'%s\'", argv[1]);
	}
	else{
		errx(EXIT_FAILURE, "wrong number of parameters for command \'checkout\'");
	}

	baseline_session_end(&s);
	return EXIT_SUCCESS;
}
