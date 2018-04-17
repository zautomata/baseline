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
#include <fcntl.h> /* open(2) */
#include <err.h>
#include <unistd.h> /* read(2), write(2) */
#include <sys/stat.h> /* S_ISDIR */
#include <sys/wait.h> /* waitpid(2) */

#include "cmd.h"
#include "session.h"

#include "objects.h"

#include "common.h"


static char *
make_tmpdir()
{
	char *name = NULL;

	name = strdup("/tmp/baseline.XXXXXX");
	if (mkdtemp(name) == NULL) {
		errx(EXIT_FAILURE, "error, failed to create a temporary directory (%s).", name);
	}
	return name;
}

static char *
make_fifo(const char *path, const char *name)
{
	char *fifo_name = NULL;

	asprintf(&fifo_name, "%s/%s", path, name); 
	if (mkfifo(fifo_name, S_IRUSR | S_IWUSR) == -1) {
		errx(EXIT_FAILURE, "error, failed to create FIFO \'%s\'.", fifo_name);
	}
	return fifo_name;
}

static int
open_fifo(const char *fifo)
{
	int fd;

	if ((fd = open(fifo, O_WRONLY)) == -1) {
		errx(EXIT_FAILURE, "error, failed to open FIFO \'%s\'.", fifo);
	}
	return fd;
}

static void
copy_to_fifo(struct file *f, int fifo)
{
	char buf[2048];
	ssize_t n;

	while ((n = read(f->fd, buf, sizeof(buf))) > 0) {
		write(fifo, buf, n);
	}
	if (n == -1) {
		errx(EXIT_FAILURE, "error, failed to read file.");
	}
}

static void
exec_ext_diff(const char *old, const char *old_label, const char *new, const char *new_label)
{
	char buf[2048];
	char *cmd = NULL;
	size_t n;
	FILE *fp;

	if (old == NULL && new == NULL)
		return;

	if (old == NULL)
		asprintf(&cmd, "diff -u %s %s -L %s -L %s", "/dev/null", new, "/dev/null", new_label == NULL ? new : new_label);
	else if(new == NULL)
		asprintf(&cmd, "diff -u %s %s -L %s -L %s", old, "/dev/null", old_label == NULL ? old : old_label, "/dev/null");
	else
		asprintf(&cmd, "diff -u %s %s -L %s -L %s", old, new, old_label == NULL ? old : old_label, new_label == NULL ? new : new_label);

	if ((fp = popen(cmd, "r")) == NULL)
		errx(EXIT_FAILURE, "error, failed to run external diff.");
	/*
	while ((n = fread(buf, sizeof(char), sizeof(buf), pf)) > 0)
		fwrite(buf, sizeof(char), n, stdout);
	*/
	while ((n = read(fileno(fp), buf, sizeof(buf))) > 0)
		write(1, buf, n);
	if (n == -1)
		errx(EXIT_FAILURE, "error, reading output from external diff.");
	pclose(fp);
	free(cmd);
}

static void
ext_diff_proc(struct session *s, struct dirent *ent, char *fifo)
{
	int fifo_fd;
	struct file *f;

	fifo_fd = open_fifo(fifo);

	f = baseline_file_new();
	s->db_ops->select_file(s->db_ctx, ent->id, f);

	copy_to_fifo(f, fifo_fd);
	close(fifo_fd);
	unlink(fifo);

	baseline_file_free(f);
}

static void
ext_diff(struct session *s, const char *tmpdir, struct dirent *ent1, struct dirent *ent2, const char *path)
{
	char *dir1 = NULL, *dir2 = NULL;
	char *label1, *label2;
	char *fifo1 = NULL, *fifo2 = NULL;
	pid_t pid1, pid2;

	label1 = NULL;
	label2 = NULL;
	if (ent1 == NULL && ent2 == NULL)
		return;

	if (ent1 != NULL) {
		asprintf(&dir1, "%s/XXXXXXX", tmpdir);
		if (mkdtemp(dir1) == NULL)
			errx(EXIT_FAILURE, "error, failed to create a temporary directory (%s).", dir1);
		fifo1 = make_fifo(dir1, ent1->id);
		//free(dir1);
		if (strlen(path) > 0)
			asprintf(&label1, "%s/%s", path, ent1->name);
		else
			asprintf(&label1, "%s", ent1->name);
	}
	if(ent2 != NULL) {
		asprintf(&dir2, "%s/XXXXXXX", tmpdir);
		if (mkdtemp(dir2) == NULL)
			errx(EXIT_FAILURE, "error, failed to create a temporary directory (%s).", dir2);
		fifo2 = make_fifo(dir2, ent2->id);
		//free(dir);
		if (strlen(path) > 0)
			asprintf(&label2, "%s/%s", path, ent2->name);
		else
			asprintf(&label2, "%s", ent2->name);
	}

	pid1 = fork();
	if (pid1 < 0) {
		errx(EXIT_FAILURE, "error, failed to create a new process.");
	}
	else if (pid1 == 0) {
		exec_ext_diff(fifo1, label1, fifo2, label2);
		exit(0);
	}
	else {
		if (ent1 != NULL && ent2 != NULL) {
			pid2 = fork();
		
			if (pid2 < 0) {
				errx(EXIT_FAILURE, "error, failed to create a new process.");
			}
			else if (pid2 == 0) {
				ext_diff_proc(s, ent1, fifo1); 
				exit(0);
			}
			else {
				ext_diff_proc(s, ent2, fifo2); 
			}
		}
		else {
			if (ent1 != NULL)
				ext_diff_proc(s, ent1, fifo1); 
			else
				ext_diff_proc(s, ent2, fifo2); 
		}
		waitpid(pid1, NULL, 0);
	}
	remove(dir1);
	remove(dir2);
	free(dir1);
	free(dir2);
	free(fifo1);
	free(fifo2);
}


static void
diff_r(struct session *s, struct dir *d1, struct dir *d2, const char *p, const char *tmpdir)
{
	char *pnext = NULL;
	struct dir *child1, *child2;
	struct dirent *ent1 = NULL, *ent2 = NULL;

	if (d1 == NULL && d2 == NULL)
		return;
	if (p == NULL || tmpdir == NULL)
		return;
	if (d1 != NULL)
		ent1 = d1->children;
	if (d2 != NULL)
		ent2 = d2->children;
	if (*p == '/')
		p++;

	while (1) {
		if (ent1 == NULL && ent2 == NULL)
			break;
		else if (ent1 == NULL) {
			/* +++ ent2 */
			if (S_ISREG(ent2->mode))
				ext_diff(s, tmpdir, NULL, ent2, p);
			else if (S_ISDIR(ent2->mode)) {
				asprintf(&pnext, "%s/%s", p, ent2->name);
				child2 = baseline_dir_new();
				s->db_ops->select_dir(s->db_ctx, ent2->id, child2);
				diff_r(s, NULL, child2, pnext, tmpdir);
				baseline_dir_free(child2);
				free(pnext);
			}
			else
				errx(EXIT_FAILURE, "error, file mode not supported.");
			ent2 = ent2->next;
		}
		else if (ent2 == NULL) {
			/* --- ent1 */
			if (S_ISREG(ent1->mode))
				ext_diff(s, tmpdir, ent1, NULL, p);
			else if (S_ISDIR(ent1->mode)) {
				asprintf(&pnext, "%s/%s", p, ent1->name);
				child1 = baseline_dir_new();
				s->db_ops->select_dir(s->db_ctx, ent1->id, child1);
				diff_r(s, child1, NULL, pnext, tmpdir);
				baseline_dir_free(child1);
				free(pnext);
			}
			else
				errx(EXIT_FAILURE, "error, file mode not supported.");
			ent1 = ent1->next;
		}
		else if (!strcmp(ent1->name, ent2->name)) {
			if (strcmp(ent1->id, ent2->id)) {
				/* *** ent1 & ent2 */
				if (S_ISREG(ent1->mode) && S_ISREG(ent2->mode))
					ext_diff(s, tmpdir, ent1, ent2, p);
				else if (S_ISREG(ent1->mode) && S_ISDIR(ent2->mode)) {
					/* delete old file */
					ext_diff(s, tmpdir, ent1, NULL, p);
					/* add new dir */
					asprintf(&pnext, "%s/%s", p, ent2->name);
					child2 = baseline_dir_new();
					s->db_ops->select_dir(s->db_ctx, ent2->id, child2);
					diff_r(s, NULL, child2, pnext, tmpdir);
					baseline_dir_free(child2);
					free(pnext);
				}
				else if (S_ISDIR(ent1->mode) && S_ISREG(ent2->mode)) {
					/* delete old dir */
					asprintf(&pnext, "%s/%s", p, ent1->name);
					child1 = baseline_dir_new();
					s->db_ops->select_dir(s->db_ctx, ent1->id, child1);
					diff_r(s, child1, NULL, pnext, tmpdir);
					baseline_dir_free(child1);
					free(pnext);
					/* add new file */
					ext_diff(s, tmpdir, NULL, ent2, p);
				}
				else if (S_ISDIR(ent1->mode) && S_ISDIR(ent2->mode)) {
					asprintf(&pnext, "%s/%s", p, ent1->name);
					child1 = baseline_dir_new();
					child2 = baseline_dir_new();
					s->db_ops->select_dir(s->db_ctx, ent1->id, child1);
					s->db_ops->select_dir(s->db_ctx, ent2->id, child2);
					diff_r(s, child1, child2, pnext, tmpdir);
					baseline_dir_free(child1);
					baseline_dir_free(child2);
					free(pnext);
				}
				else
					errx(EXIT_FAILURE, "error, file mode not supported.");
			}
			ent1 = ent1->next;
			ent2 = ent2->next;
		}
		else if (strcmp(ent1->name, ent2->name) < 0) {
			/* --- ent1 */
			if (S_ISREG(ent1->mode))
				ext_diff(s, tmpdir, ent1, NULL, p);
			else if (S_ISDIR(ent1->mode)) {
				asprintf(&pnext, "%s/%s", p, ent1->name);
				child1 = baseline_dir_new();
				s->db_ops->select_dir(s->db_ctx, ent1->id, child1);
				diff_r(s, child1, NULL, pnext, tmpdir);
				baseline_dir_free(child1);
				free(pnext);
			}
			else
				errx(EXIT_FAILURE, "error, file mode not supported.");
			ent1 = ent1->next;
		}
		else if (strcmp(ent1->name, ent2->name) > 0) {
			/* +++ ent2 */
			if (S_ISREG(ent2->mode))
				ext_diff(s, tmpdir, NULL, ent2, p);
			else if (S_ISDIR(ent2->mode)) {
				asprintf(&pnext, "%s/%s", p, ent2->name);
				child2 = baseline_dir_new();
				s->db_ops->select_dir(s->db_ctx, ent2->id, child2);
				diff_r(s, NULL, child2, pnext, tmpdir);
				baseline_dir_free(child2);
				free(pnext);
			}
			else
				errx(EXIT_FAILURE, "error, file mode not supported.");
			ent2 = ent2->next;
		}
	}
}

int
cmd_diff(int argc, char **argv)
{
	char *old = NULL, *new = NULL;
	char *tmpdir = NULL;
	struct session s;
	struct commit *comm_old, *comm_new;
	struct dir *dir_old, *dir_new;

	baseline_session_begin(&s, 0);

	if (argc == 2) {
		new = strdup(argv[1]);
	}
	else if (argc == 3) {
		old = strdup(argv[1]);
		new = strdup(argv[2]);
	}
	else {
		errx(EXIT_FAILURE, "wrong number of arguments (%d)\n", argc);
	}

	tmpdir = make_tmpdir();

	comm_new = baseline_commit_new();
	s.db_ops->select_commit(s.db_ctx, new, comm_new);

	dir_new = baseline_dir_new();
	s.db_ops->select_dir(s.db_ctx, comm_new->dir, dir_new);

	if (old == NULL) {
		if (comm_new->n_parents > 0)
			old = comm_new->parents[0];
		else
			goto ret;
	}

	comm_old = baseline_commit_new();
	s.db_ops->select_commit(s.db_ctx, old, comm_old);

	dir_old = baseline_dir_new();
	s.db_ops->select_dir(s.db_ctx, comm_old->dir, dir_old);

	diff_r(&s, dir_old, dir_new, "", tmpdir);

	baseline_dir_free(dir_old);
	baseline_dir_free(dir_new);

ret:
	remove(tmpdir);
	free(tmpdir);
	free(old);
	free(new);
	baseline_session_end(&s);
	return EXIT_SUCCESS;
}

