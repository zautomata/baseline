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

#ifndef _OBJDB_H_
#define _OBJDB_H_

#include <sys/types.h>
#include "objects.h"

struct objdb_ctx {
	char *db_name;
	char *db_path;
	char *db_version;
};

struct objdb_ops {
	char *name;
	char *version;
	/**/
	int (*open)(struct objdb_ctx **, const char *, const char *);
	int (*close)(struct objdb_ctx *);
	int (*init)(struct objdb_ctx *);
	/* file ops */
	//int (*insert_from_file)(struct objdb_ctx *, enum objdb_type, const char *, char **);
	int (*insert_file)(struct objdb_ctx *, struct file *);
	int (*insert_dir)(struct objdb_ctx *, struct dir *);
	int (*insert_commit)(struct objdb_ctx *, struct commit *);
	int (*select_file)(struct objdb_ctx *, const char *, struct file *);
	int (*select_dir)(struct objdb_ctx *, const char *, struct dir *);
	int (*select_commit)(struct objdb_ctx *, const char *, struct commit *);
	/* branch ops */
	int (*branch_create)(struct objdb_ctx *, const char *);
	int (*branch_create_from)(struct objdb_ctx *, const char *, const char *);
	int (*branch_if_exists)(struct objdb_ctx *, const char *, int *);
	int (*branch_set_head)(struct objdb_ctx *, const char *, const char *);
	int (*branch_get_head)(struct objdb_ctx *, const char *, char **);
	int (*branch_ls)(struct objdb_ctx *);
	/**/
	int (*remove)(struct objdb_ctx *, const char *, const char *);
	int (*fsck)(struct objdb_ctx *);
	int (*compress)(struct objdb_ctx *);
	int (*dedup)(struct objdb_ctx *);
};

#endif
