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
#include <stdlib.h>		/* EXIT_* */
#include <string.h>		/* strcmp(1) */
#include <err.h>		/* err(3) */
#include "cmd.h"

void usage(void)
{
	fprintf(stderr, "usage: baseline <command> [args]\n");
}

int
main(int argc, char **argv)
{
	/* TODO: use getopt(3) */
	if (argc < 2) {
		usage();
		return EXIT_FAILURE;
	}
	if (!strcmp(argv[1], "add")) {
		cmd_add(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "branch")) {
		cmd_branch(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "cat")) {
		cmd_cat(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "checkout")) {
		cmd_checkout(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "commit")) {
		cmd_commit(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "diff")) {
		cmd_diff(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "help")) {
		cmd_help(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "init")) {
		cmd_init(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "ls")) {
		cmd_ls(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "log")) {
		cmd_log(argc - 1, argv + 1);
	}
	else if (!strcmp(argv[1], "version")) {
		cmd_version(argc - 1, argv + 1);
	}
	else {
		errx(EXIT_FAILURE, "unkown commad: \'%s\'.\nFor list of available commands, type:\n\tbaseline help", argv[1]);
	}
	return EXIT_SUCCESS;
}

