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
#include <unistd.h> /* getopt(3) */
#include <sys/stat.h> /* S_ISDIR */

#include "cmd.h"
#include "session.h"

#include "objects.h"


void
ls(struct session *s, struct dir* d, const char *prefix, int is_recursive)
{
	char *nextprefix = NULL;
	struct dir *child;
	struct dirent *ent;

	if (d == NULL)
		return;
	ent = d->children;
	while (ent != NULL) {
		if (S_ISDIR(ent->mode)) {
			printf("%s%s/\n", prefix, ent->name);
			if (is_recursive) {
				asprintf(&nextprefix, "%s%s/", prefix, ent->name);
				child = baseline_dir_new();
				s->db_ops->select_dir(s->db_ctx, ent->id, child);
				ls(s, child, nextprefix, is_recursive);
				baseline_dir_free(child);
				free(nextprefix);
			}
		}
		else {
			printf("%s%s\n", prefix, ent->name);
		}
		ent = ent->next;
	}
}

int
cmd_ls(int argc, char **argv)
{
	char *head = NULL;
	int ch;
	int recursive = 0, explicit = 0;
	struct session s;
	struct commit *comm;
	struct dir *dir;

	baseline_session_begin(&s, 0);

	/* parse command line options */
	while ((ch = getopt(argc, argv, "Rc:")) != -1) {
		switch (ch) {
		case 'R':
			recursive = 1;
			break;
		case 'c':
			explicit = 1;
			head = strdup(optarg);
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	if (!explicit) {
		s.db_ops->branch_get_head(s.db_ctx, s.branch, &head);
		if (head == NULL)
			goto ret;
	}

	comm = baseline_commit_new();
	s.db_ops->select_commit(s.db_ctx, head, comm);

	dir = baseline_dir_new();
	s.db_ops->select_dir(s.db_ctx, comm->dir, dir);

	printf("commit: %s\n", comm->id);
	ls(&s, dir, "", recursive);

	baseline_dir_free(dir);

ret:
	baseline_session_end(&s);
	return EXIT_SUCCESS;
}

