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

#include <stdio.h> /* printf(3) */
#include <stdlib.h> /* EXIT_SUCCESS */
#include <string.h> /* strdup(3) */
#include <sys/stat.h> /* S_ISDIR */
#include <unistd.h> /* getopt(3) */
#include <err.h> /* errx(3) */

#include "cmd.h"
#include "session.h"

#include "objects.h"


static struct dirent*
lookup(struct dir *dir, const char *name)
{
	struct dirent *ent;

	ent = dir->children;
	while (ent != NULL) {
		if (!strcmp(ent->name, name))
			return ent;
		ent = ent->next;
	}
	return NULL;
}

int
cmd_cat(int argc, char **argv)
{
	char *comm_id = NULL, *path;
	char *id = NULL, *p1, *p2;
	char buf[1024];
	int ch, n;
	struct session s;
	struct commit *comm;
	struct dir *dir;
	struct dirent *ent;
	struct file *file;

	baseline_session_begin(&s, 0);

	/* parse command line options */
	while ((ch = getopt(argc, argv, "c:")) != -1) {
		switch(ch) {
		case 'c':
			comm_id = strdup(optarg);
			break;
		default:
			return EXIT_FAILURE;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		errx(EXIT_FAILURE, "error, no file is specified.");
	if (argc > 1)
		errx(EXIT_FAILURE, "error, incorrect number of arguments specified.");

	/* no commit specified, use current branch head */ 
	if (comm_id == NULL) {
		if (s.db_ops->branch_get_head(s.db_ctx, s.branch, &comm_id) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, branch \'%s\' was not found.", s.branch);
		if (comm_id == NULL)
			errx(EXIT_FAILURE, "error, branch \'%s\' has zero commits.", s.branch);
	}

	path = strdup(argv[0]);

	comm = baseline_commit_new();
	if (s.db_ops->select_commit(s.db_ctx, comm_id, comm) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "error, commit \'%s\' was not found.", comm_id);

	dir = baseline_dir_new();
	s.db_ops->select_dir(s.db_ctx, comm->dir, dir);

	p1 = p2 = path;
	while (1) {
		/* skip leading '/' */
		if (*p1 == '/') {
			p1++;
			p2++;
			continue;
		}
		if (*p2 == '/') {
			/* should be a directory */
			*p2 = '\0';
#ifdef DEBUG
			printf("[DEBUG] looking up directory \'%s\'.\n", p1);
#endif
			if ((ent = lookup(dir, p1)) == NULL)
				errx(EXIT_FAILURE, "error, no such a file or directory \'%s\'.", argv[0]);
			if (!S_ISDIR(ent->mode))
				errx(EXIT_FAILURE, "error, \'%s\' is not a directory.", p1);
			free(id);
			id = strdup(ent->id);

			baseline_dir_free(dir);
			dir = baseline_dir_new();
			s.db_ops->select_dir(s.db_ctx, id, dir);

			p1 = ++p2;
			p2 = p1;
		}
		else if (*p2 == '\0') {
			/* found a directory (ending with '/', skipped by above code) */
			if (p1 == p2)
				errx(EXIT_FAILURE, "error, to list directories use \'ls\' command instead.");
			/* should be a file */
#ifdef DEBUG
			printf("[DEBUG] lookuping up file \'%s\'\n", p1);
#endif
			if ((ent = lookup(dir, p1)) == NULL)
				errx(EXIT_FAILURE, "error, no such a file or directory \'%s\'.", argv[0]);
			if (S_ISDIR(ent->mode))
				errx(EXIT_FAILURE, "error, \'%s\' is a directory, to list directories use \'ls\' command instead.", p1);

			file = baseline_file_new();
			s.db_ops->select_file(s.db_ctx, ent->id, file);
#ifdef DEBUG
			printf("[DEBUG] file found with id \'%s\'.\n", file->id);
#endif
			break;
		}
		else {
			p2++;
		}
	}

	/* output file to stdout */
	while ((n = read(file->fd, buf, sizeof(buf))) > 0) {
		write(1, buf, n);
	}
	if (n == -1)
		errx(EXIT_FAILURE, "error, failed to read file.");
	close(file->fd);

	free(path);
	baseline_dir_free(dir);
	baseline_file_free(file);

	baseline_session_end(&s);
	return EXIT_SUCCESS;
}

