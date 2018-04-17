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
#include <unistd.h> /* getcwd(3) */
#include <err.h>    /* err(3) */

#include "defaults.h"
#include "session.h"
#include "config.h"
#include "cmd.h"


static const char msg_head[] = {
	"\n"
	"# Please enter your commit message.\n"
	"# Lines starting with \'#\' will be ignored.\n"
};

static int
gen_temp_msgfile_from_msg(const char *path, char **filename, const char *msg)
{
	char *msg_file = NULL;
	int msgfd;

	if (msg == NULL)
		return EXIT_FAILURE;
        asprintf(&msg_file, "%s/tmp.XXXXXX", path);
	if ((msgfd = mkstemp(msg_file)) == -1)
		return EXIT_FAILURE;
	if (write(msgfd, msg, strlen(msg)) == -1)
		return EXIT_FAILURE;
	if (write(msgfd, "\n", strlen("\n")) == -1)
		return EXIT_FAILURE;
	close(msgfd);
	*filename = msg_file;
	return EXIT_SUCCESS;
}

static int
gen_temp_msgfile(const char *path, char **filename)
{
	char *msg_file = NULL;
	int msgfd;

        asprintf(&msg_file, "%s/tmp.XXXXXX", path);
	if ((msgfd = mkstemp(msg_file)) == -1)
		return EXIT_FAILURE;
	if (write(msgfd, msg_head, sizeof(msg_head) - 1) == -1)
		return EXIT_FAILURE;
	close(msgfd);
	*filename = msg_file;
	return EXIT_SUCCESS;
}

static int
process_msgfile(const char *path, char **filename)
{
	char *proc_file = NULL;
	char *line = NULL;
	int pfd;
	size_t size = 0;
	FILE *fp;

        asprintf(&proc_file, "%s/tmp.XXXXXX", path);
	if ((pfd = mkstemp(proc_file)) == -1)
		return EXIT_FAILURE;
	if((fp = fopen(*filename, "r")) == NULL) {
		unlink(proc_file);
		goto failure;
	}
	while (getline(&line, &size, fp) != -1) {
		if(line[0] == '#')
			continue;
		if (write(pfd, line, strlen(line)) == -1)
			goto failure;
	}
	close(pfd);
	fclose(fp);
	unlink(*filename);
	free(*filename);
	*filename = proc_file;
	return EXIT_SUCCESS;
failure:
	free(proc_file);
	*filename = NULL;
	return EXIT_FAILURE;
}

int
cmd_commit(int argc, char **argv)
{
	char *commit_msg = NULL, *msg_file = NULL;
	char *exec;
	char *branch_head, *workdir_head;
	const char *editor;
	int ch;
	int flag_msg = 0, flag_error = 0, flag_force = 0;
	struct session s;

	baseline_session_begin(&s, 0);

	/* parse command line options */
	while ((ch = getopt(argc, argv, "fm:")) != -1) {
		switch (ch) {
		case 'f':
			flag_force = 1;
			break;
		case 'm':
			flag_msg = 1;
			commit_msg = strdup(optarg);
			break;
		default:
			flag_error = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (flag_error) {
		return EXIT_FAILURE;
	}
	if (flag_msg) {
		/* TODO: should be per-stage file */
		if (gen_temp_msgfile_from_msg(s.repo_baselinedir, &msg_file, commit_msg) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to generate commit message.");
		free(commit_msg);
	}
	else {
		if (gen_temp_msgfile(s.repo_baselinedir, &msg_file) == EXIT_FAILURE)
			errx(EXIT_FAILURE, "error, failed to generate commit message.");
		if((editor = baseline_config_get_val("editor")) != NULL && strcmp(editor, "")) {
			asprintf(&exec, "%s %s", editor, msg_file);
			if (system(exec) != EXIT_SUCCESS)
				errx(EXIT_FAILURE, "error, failed to launch \'%s\' text editor.", editor);
			free(exec);
		}
		else if ((editor = getenv("EDITOR")) != NULL) {
			asprintf(&exec, "%s %s", editor, msg_file);
			if (system(exec) != EXIT_SUCCESS)
				errx(EXIT_FAILURE, "error, failed to launch \'%s\' text editor.", editor);
			free(exec);
		}
		else {
			errx(EXIT_FAILURE, "error, commit message not specified.");
		}
	}
	if (process_msgfile(s.repo_baselinedir, &msg_file) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "error, failed to process commit message.");

	/* make sure that the working dir matches the branch's head */
	if (s.db_ops->branch_get_head(s.db_ctx, s.branch, &branch_head) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "error, failed to query the status of the current branch.");
	if (s.dc_ops->workdir_get(s.dc_ctx, &workdir_head) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "error, failed to query the status of the working directory.");
	/* check if it's the first commit ever */
	if ((branch_head == NULL && workdir_head == NULL) || (strcmp(branch_head, workdir_head) == 0) || flag_force)
		goto next;
	else
		errx(EXIT_FAILURE, "error, the heads of the current branch and working directory do not match.\n"
			"you can:\n\t[1] switch to the correct branch, or\n\t[2] checkout the last head, or\n\t[3] use -f to force commit");

next:
	if (s.dc_ops->commit(s.dc_ctx, msg_file) == EXIT_FAILURE)
		errx(EXIT_FAILURE, "error, failed to commit your changes.\n");

	unlink(msg_file);
	free(msg_file);

	baseline_session_end(&s);
	return EXIT_SUCCESS;
}

