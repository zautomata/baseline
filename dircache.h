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

#ifndef _DIRCACHE_H_
#define _DIRCACHE_H_

#include <sys/types.h>
#include "objdb.h"

struct dircache_ctx {
	char *repo_rootpath;
	char *repo_baselinepath;
	struct objdb_ctx *db_ctx;
	struct objdb_ops *db_ops;
};

struct dircache_ops {
	char *name;
	char *version;
	int (*open)(struct dircache_ctx **, struct objdb_ctx *, struct objdb_ops *, const char *, const char *);
	int (*close)(struct dircache_ctx *);
	int (*init)(struct dircache_ctx *);
	int (*insert)(struct dircache_ctx *, const char *);
	int (*remove)(struct dircache_ctx *, const char *);
	int (*commit)(struct dircache_ctx *, const char *);
	int (*branch_get)(struct dircache_ctx *, char **);
	int (*branch_set)(struct dircache_ctx *, const char *);
	int (*workdir_get)(struct dircache_ctx *, char **);
	int (*workdir_set)(struct dircache_ctx *, const char *);
	int (*fsck)(struct dircache_ctx *);
	int (*compress)(struct dircache_ctx *);
	int (*dedup)(struct dircache_ctx *);
};

#endif
