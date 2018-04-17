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
#include <stdlib.h> /* EXIT_SUCCESS, strtonum(3) */
#include <string.h> /* strdup(3) */
#include <time.h> /* ctime(3) */
#include <unistd.h> /* getopt(3) */
#include <err.h> /* errx(3) */
#include <limits.h> /* INT_MAX */

#include "cmd.h"
#include "session.h"

#include "objects.h"

static char *default_fmt =
	"commit: %i\n"
	"author:\n"
	"\tname: %an\n"
	"\temail: %ae\n"
	"\ttime: %at\n"
	"committer:\n"
	"\tname: %cn\n"
	"\temail: %ce\n"
	"\ttime: %ct\n"
	"message:\n%m\n";


int
cmd_log(int argc, char **argv)
{
	char *head = NULL, *fmtstr = NULL;
	char timestr[26];
	const char *errstr;
	int ch, done = 0, i, k = 0, kmax = 0;
	int fmt = 0, limited = 0, explicit = 0;
	struct session s;
	struct commit *comm;

	baseline_session_begin(&s, 0);

	/* parse command line options */
	while ((ch = getopt(argc, argv, "f:n:c:")) != -1) {
		switch (ch) {
		case 'f':
			fmt = 1;
			fmtstr = strdup(optarg);
			break;
		case 'n':
			limited = 1;
			kmax = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(EXIT_FAILURE, "error, number (%s) is %s.", optarg, errstr);
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

	if (!fmt)
		fmtstr = default_fmt;

	do {
		k++;
		if ((limited) && (k > kmax))
			break;
		comm = baseline_commit_new();
		s.db_ops->select_commit(s.db_ctx, head, comm);
		for (i = 0 ; i<strlen(fmtstr) ; i++) {
			if ((i < strlen(fmtstr) - 1) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'n')) {
				printf("%d", k);
				i++;
			}
			else if ((i < strlen(fmtstr) - 1) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'i')) {
				printf("%s", comm->id);
				i++;
			}
			else if ((i < strlen(fmtstr) - 2) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'a') && (fmtstr[i+2] == 'n')) {
				printf("%s", comm->author.name);
				i += 2;
			}
			else if ((i < strlen(fmtstr) - 2) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'a') && (fmtstr[i+2] == 'e')) {
				printf("%s", comm->author.email);
				i += 2;
			}
			else if ((i < strlen(fmtstr) - 2) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'a') && (fmtstr[i+2] == 't')) {
				bzero(timestr, sizeof(timestr));
				ctime_r(&(comm->author.timestamp), timestr);
				timestr[sizeof(timestr) - 2] = '\0';
				printf("%s", timestr);
				i += 2;
			}
			else if ((i < strlen(fmtstr) - 2) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'c') && (fmtstr[i+2] == 'n')) {
				printf("%s", comm->committer.name);
				i += 2;
			}
			else if ((i < strlen(fmtstr) - 2) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'c') && (fmtstr[i+2] == 'e')) {
				printf("%s", comm->committer.email);
				i += 2;
			}
			else if ((i < strlen(fmtstr) - 2) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'c') && (fmtstr[i+2] == 't')) {
				bzero(timestr, sizeof(timestr));
				ctime_r(&(comm->committer.timestamp), timestr);
				timestr[sizeof(timestr) - 2] = '\0';
				printf("%s", timestr);
				i += 2;
			}
			else if ((i < strlen(fmtstr) - 1) && (fmtstr[i] == '%') && (fmtstr[i + 1] == 'm')) {
				printf("%s", comm->message);
				i++;
			}
			else if ((i < strlen(fmtstr) - 1) && (fmtstr[i] == '\\') && (fmtstr[i + 1] == 'n')) {
				printf("\n");
				i++;
			}
			else if ((i < strlen(fmtstr) - 1) && (fmtstr[i] == '\\') && (fmtstr[i + 1] == 't')) {
				printf("\t");
				i++;
			}
			else
				printf("%c", fmtstr[i]);
		}
		if (comm->n_parents == 0)
			done = 1;
		else
			head = strdup(comm->parents[0]);
		baseline_commit_free(comm);
	} while (!done);

ret:
	free(head);
	if (fmt)
		free(fmtstr);
	baseline_session_end(&s);
	return EXIT_SUCCESS;
}

