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
#include "config.h"
#include "session.h"
#include "cmd.h"


int
cmd_branch(int argc, char **argv)
{
	char *branch = NULL;
	char *head = NULL;
	enum {
		O_LISTCUR,
		O_LISTALL,
		O_CREATE,
		O_SWITCH
	} op = O_LISTCUR;
	int ch, error = 0, exist = 0;
	struct session s;

	baseline_session_begin(&s, 0);

	/* parse command line options */
	while ((ch = getopt(argc, argv, "c:ls:")) != -1) {
		switch (ch) {
		case 'c':
			op = O_CREATE;
			branch = strdup(optarg);
			break;
		case 'l':
			op = O_LISTALL;
			break;
		case 's':
			op = O_SWITCH;
			branch = strdup(optarg);
			break;
		default:
			error = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (error) {
		return EXIT_FAILURE;
	}

	switch (op) {
	case O_CREATE:
		/* get current branch's head */
		if (s.db_ops->branch_get_head(s.db_ctx, s.branch, &head) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to find branch head.");
		if (head == NULL)
			errx(EXIT_FAILURE, "error, branch \'%s\' does not contain any commits.", s.branch);
		/* create a new branch from the current one */
		if (s.db_ops->branch_create_from(s.db_ctx, branch, s.branch) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to create branch \'%s\'.", branch);
		break;
	case O_LISTALL:
		/* list all branches */
		if (s.db_ops->branch_ls(s.db_ctx) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to list branches.");
		break;
	case O_LISTCUR:
		/* list current branch */
		if (s.db_ops->branch_get_head(s.db_ctx, s.branch, &head) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to find branch head.");
		printf("current branch: %s\nhead: %s\n", s.branch, head);
		free(head);
		break;
	case O_SWITCH:
		if (s.db_ops->branch_if_exists(s.db_ctx, branch, &exist) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to query branch.");
		if (exist)
			s.dc_ops->branch_set(s.dc_ctx, branch);
		else
			errx(EXIT_FAILURE, "branch \'%s\' does not exist.", branch);
		break;
	}

	baseline_session_end(&s);
	return EXIT_SUCCESS;
}
