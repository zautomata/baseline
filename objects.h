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

#ifndef _OBJECTS_H_
#define _OBJECTS_H_

#include <fcntl.h>
#include <time.h>

#define MAX_PARENTS	1

enum objtype {
	O_FILE,
	O_DIR,
	O_COMMIT,
	O_TAG
};

struct user {
	char *name;
	char *email;
	time_t timestamp;
};

struct commit {
	char *id;
	char *dir;
	struct user author;
	struct user committer;
	u_int8_t n_parents;
	char *parents[MAX_PARENTS];
	char *message;
};

struct dirent {
	char *id;
	char *name;
	/* TODO: get rid of type */
	enum {
		T_FILE,
		T_DIR
	} type;
	mode_t mode;
	struct dirent *next;
};

struct dir {
	char *id;
	struct dir *parent;
	struct dirent *children;
};

struct file {
	char *id;
	enum {
		LOC_FS,
		LOC_MEM
	} loc;
	union {
		int fd;
		char *buffer;
	};
};

/* file ops */
struct file* baseline_file_new();
void baseline_file_free(struct file *);
/* commit ops */
struct commit* baseline_commit_new();
void baseline_commit_free(struct commit *);
/* dir ops */
struct dir* baseline_dir_new();
void baseline_dir_free(struct dir *);
void baseline_dir_append(struct dir *, struct dirent *);

#endif
