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

#include <sys/stat.h>   /* stat(3) */

#include <stdio.h>
#include <stdlib.h>	/* malloc(2) */
#include <string.h>	/* str*(2), mem*(2) */
#include <unistd.h>	/* close(2), rmdir(2), unlink(2) */

#include <fcntl.h>	/* O_* macros */
#include <fts.h>	/* fts_*(3) */

#include "defaults.h"
#include "config.h"
#include "objdb.h"
#include "dircache.h"
#include "objects.h"
#include "helper.h"

int dircache_simple_get_ops(struct dircache_ops **);
static int simple_open(struct dircache_ctx **, struct objdb_ctx *, struct objdb_ops *, const char *, const char *);
static int simple_close(struct dircache_ctx *);
static int simple_init(struct dircache_ctx *);
static int simple_insert(struct dircache_ctx *, const char *);
static int simple_commit(struct dircache_ctx *, const char *);
static int simple_branch_get(struct dircache_ctx *, char **);
static int simple_branch_set(struct dircache_ctx *, const char *);
static int simple_workdir_get(struct dircache_ctx *, char **);
static int simple_workdir_set(struct dircache_ctx *, const char *);

static struct dircache_ops simple_ops = {
	.name = "simple",
	.version = "1.0",
	.open = simple_open,
	.close = simple_close,
	.init = simple_init,
	.insert = simple_insert,
	.remove = NULL,
	.commit = simple_commit,
	.branch_get = simple_branch_get,
	.branch_set = simple_branch_set,
	.workdir_get = simple_workdir_get,
	.workdir_set = simple_workdir_set,
	.fsck = NULL,
	.compress = NULL,
	.dedup = NULL
};

int
dircache_simple_get_ops(struct dircache_ops **ops)
{
	if (ops == NULL)
		return EXIT_FAILURE;
	*ops = (struct dircache_ops *)malloc(sizeof(struct dircache_ops));
	memcpy(*ops, &simple_ops, sizeof(struct dircache_ops));
	return EXIT_SUCCESS;
}

/* FIXME */
static char *
get_dircache_path(struct dircache_ctx *ctx)
{
        char *path = NULL;

        if (ctx == NULL)
                return NULL;
        asprintf(&path, "%s/%s", ctx->repo_baselinepath, BASELINE_DIRCACHE);
        return path;
}

static const char *
strmismatch(const char *first, const char *second)
{
	int i;
	size_t minlen;

	minlen = strlen(first) < strlen(second) ? strlen(first) : strlen(second);
	for (i=0 ; i<minlen ; i++) {
		if (first[i] == second[i])
			continue;
		break;
	}
#ifdef DEUG
	printf("[DEBUG] mismatch at i = %d\n", i);
#endif
	return &first[i];
}

static const char*
dir_diff(const char *first, const char *second)
{
	const char *mis = strmismatch(first, second);
	if (*mis == '/')
		mis++;
	return mis;
}

static int
mkdirp(const char *parent, const char *dir)
{
	char *o, *start, *end;
	char *path = NULL;
	int retval;
	struct stat s;

	o = start = end = strdup(dir);
	if (*o == '/') {
		start++;
		end++;
	}
	while (1) {
		if (*end == '\0')
			break;
		if (*end == '/') {
			*end = '\0';
			/* OpenBSD's asprintf(3) uses realloc(), hence no need to free */
			asprintf(&path, "%s/%s", parent, start);
#ifdef DEBUG
			printf("[DEBUG] create %s\n", path);
#endif
			if (stat(path, &s) == 0) {
				if (!S_ISDIR(s.st_mode)) {
					retval = EXIT_FAILURE;
					goto ret;
				}
			}
			else {
				if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
					return EXIT_FAILURE;
			}
			*end = '/';
		}
		end++;
	}
ret:
	free(path);
	free(o);
	return EXIT_SUCCESS;
}

static int
simple_open(struct dircache_ctx **dc_ctx, struct objdb_ctx *db_ctx, struct objdb_ops *db_ops, const char *rootpath, const char *baselinepath)
{
	if (dc_ctx == NULL || db_ctx == NULL)
		return EXIT_FAILURE;
	*dc_ctx = (struct dircache_ctx *)malloc(sizeof(struct dircache_ctx));
	(*dc_ctx)->db_ctx = db_ctx;
	(*dc_ctx)->db_ops = db_ops;
	(*dc_ctx)->repo_rootpath = strdup(rootpath);
	(*dc_ctx)->repo_baselinepath = strdup(baselinepath);
	return EXIT_SUCCESS;
}

static int
simple_close(struct dircache_ctx *dc_ctx)
{
	/* TODO */
	return EXIT_SUCCESS;
}

static int
simple_init(struct dircache_ctx *dc_ctx)
{
	char *branch_name = NULL, *branch_path;
	char *dc_path;
	char *workdir_fname;
	int exist, fd, retval;
	struct stat s;
	FILE *branch_fp;

	if (dc_ctx == NULL)
		return EXIT_FAILURE;
	dc_path = get_dircache_path(dc_ctx);
	asprintf(&branch_path, "%s/branch", dc_ctx->repo_baselinepath);
	/* check if '.baseline/dircache' dir exists */
	if (stat(dc_path, &s) == -1) {
		/* create '.baseline/dircache' dir */
		if (mkdir(dc_path, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
			return EXIT_FAILURE;
		}
	}
	/* check if '.baseline/branch' file exists */
	/* TODO: a lot of that code should be moved to is_init() func or cmd-init file */
	/* may be already initialized ... should just return failure? */
	if(stat(branch_path, &s) == 0) {
		if (!S_ISREG(s.st_mode)) {
			retval = EXIT_FAILURE;
			goto ret;
		}
		if (!(s.st_mode & S_IRUSR) || !(s.st_mode & S_IWUSR)) {
			retval = EXIT_FAILURE;
			goto ret;
		}
	}
	else {
		asprintf(&branch_name, "%s", DEFAULT_BRANCH);
		/* this piece of code does not belong here */
		if (dc_ctx->db_ops->branch_if_exists(dc_ctx->db_ctx, branch_name, &exist) == EXIT_FAILURE) {
			retval = EXIT_FAILURE;
			free(branch_name);
			goto ret;
		}
		if (!exist)
			dc_ctx->db_ops->branch_create(dc_ctx->db_ctx, branch_name);
		/* create '.baseline/branch' file */
		if ((branch_fp = fopen(branch_path, "w")) == NULL) {
			retval = EXIT_FAILURE;
			goto ret;
		}
		fprintf(branch_fp, "%s", branch_name);
		fclose(branch_fp);
		free(branch_name);
	}
	/* create '.baseline/workdir' file */
	asprintf(&workdir_fname, "%s/workdir", dc_ctx->repo_baselinepath);
	if ((fd = open(workdir_fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
		free(workdir_fname);
		retval = EXIT_FAILURE;
		goto ret;
	}
	close(fd);
ret:
	free(dc_path);
	free(branch_path);
	return EXIT_SUCCESS;
}

static int
simple_insert(struct dircache_ctx *dc_ctx, const char *path)
{
	char *objid, *paths[2], *cache_path;
	char *dc_path, *p;
	struct stat s, fs;
	FILE *fp;
	FTS *dir;
	FTSENT *entry;
	struct file *file;

	dc_path = get_dircache_path(dc_ctx);
	if (stat(path, &s) == -1)
		return EXIT_FAILURE;
	if (S_ISDIR(s.st_mode)) {
		/* dirs are cached for now, and should be inserted at commit time */
		paths[0] = (char *)path;
		paths[1] = NULL;
		if ((dir = fts_open(paths, FTS_NOCHDIR, 0)) == NULL)
			return EXIT_FAILURE;
		while ((entry = fts_read(dir)) != NULL) {
			/* skip directories starting with '.', other than our top-level directory */
			if (entry->fts_name[0] == '.' && entry->fts_level != FTS_ROOTLEVEL) {
				fts_set(dir, entry, FTS_SKIP);
				continue;
			}
			if (entry->fts_info & FTS_D) {
#ifdef DEBUG
				printf("DS: %s\n", entry->fts_path);
#endif
				/* FIXME: path check is required */
				p = dir_diff(entry->fts_path, dc_ctx->repo_rootpath);
				if (*p != '\0') {
					asprintf(&cache_path, "%s/%s", dc_path, p);
					if (mkdir(cache_path, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
						free(cache_path);
						return EXIT_FAILURE;
					}
#ifdef DEBUG
					printf("[DEBUG] dircache: created dir %s\n", cache_path);
#endif
					free(cache_path);
				}
			}
			if(entry->fts_info & FTS_F) {
#ifdef DEBUG
				printf("F: %s\n", entry->fts_path);
#endif
				if (stat(entry->fts_path, &fs) == -1)
					return EXIT_FAILURE;
				/* TODO: baseline_file_new2() */
				file = baseline_file_new();
				file->loc = LOC_FS;
				if ((file->fd = open(entry->fts_path, O_RDONLY, 0)) == -1)
					return EXIT_FAILURE;
				dc_ctx->db_ops->insert_file(dc_ctx->db_ctx, file);
				close(file->fd);
				objid = strdup(file->id);
				baseline_file_free(file);
#ifdef DEBUG
				printf("\t ID = %s\n", objid);
#endif
				/* FIXME: path check is required */
				asprintf(&cache_path, "%s/%s", dc_path, dir_diff(entry->fts_path, dc_ctx->repo_rootpath));
				if ((fp = fopen(cache_path, "w")) == NULL) {
					free(cache_path);
					return EXIT_FAILURE;
				}
#ifdef DEBUG
				printf("[DEBUG] dircache: created file %s\n", cache_path);
#endif
				fprintf(fp, "F %s %06o\n", objid, fs.st_mode);
				fclose(fp);
				free(cache_path);
			}
		}
		fts_close(dir);
	}
	else {
		/* it's a file, why the hell would we wait, insert it immediately */
		if (stat(path, &fs) == -1)
			return EXIT_FAILURE;
		/* TODO: baseline_file_new2() */
		file = baseline_file_new();
		file->loc = LOC_FS;
		if ((file->fd = open(path, O_RDONLY, 0)) == -1)
			return EXIT_FAILURE;
		dc_ctx->db_ops->insert_file(dc_ctx->db_ctx, file);
		close(file->fd);
		objid = strdup(file->id);
		baseline_file_free(file);

		if (mkdirp(dc_path, dir_diff(path, dc_ctx->repo_rootpath)) == EXIT_FAILURE)
			return EXIT_FAILURE;
		asprintf(&cache_path, "%s/%s", dc_path, dir_diff(path, dc_ctx->repo_rootpath));
		if ((fp = fopen(cache_path, "w")) == NULL) {
			free(cache_path);
			return EXIT_FAILURE;
		}
		fprintf(fp, "F %s %06o\n", objid, fs.st_mode);
		fclose(fp);
		free(objid);
		free(cache_path);
	}
	return EXIT_SUCCESS;
}

static int
gen_dindex(struct dircache_ctx *dc_ctx, char **didx)
{
	char *dircache_path, *didx_path = NULL;
	char *paths[2];
	int didx_fd;
	struct stat s;
	FILE *didx_fp;
	FTS *ftsp;
	FTSENT *entry;

	dircache_path = get_dircache_path(dc_ctx);
	if (stat(dircache_path, &s) == -1)
		return EXIT_FAILURE;
	if (!S_ISDIR(s.st_mode))
		return EXIT_FAILURE; 
	/* create dir-only index */
	asprintf(&didx_path, "%s/dindex.XXXXXXX", dc_ctx->repo_baselinepath);
	if ((didx_fd = mkstemp(didx_path)) == -1) {
		return EXIT_FAILURE;
	}
	if ((didx_fp = fdopen(didx_fd, "w")) == NULL) {
		unlink(didx_path);
		close(didx_fd);
		return EXIT_FAILURE;
	}
	paths[0] = (char *)dircache_path;
	paths[1] = NULL;
	if ((ftsp = fts_open(paths, FTS_NOCHDIR, 0)) == NULL)
		return EXIT_FAILURE;
	while ((entry = fts_read(ftsp)) != NULL) {
		/* skip directories starting with '.', other than our top-level directory */
		if (entry->fts_name[0] == '.' && entry->fts_level != FTS_ROOTLEVEL) {
			fts_set(ftsp, entry, FTS_SKIP);
			continue;
		}
		if (entry->fts_info & FTS_DP) {
			/* FIXME: spaces! */
			fprintf(didx_fp, "%s\n", entry->fts_path);
		}
	}
	fts_close(ftsp);
	fclose(didx_fp);
	free(dircache_path);
	*didx = didx_path;
	return EXIT_SUCCESS;
}

static int
simple_commit(struct dircache_ctx *dc_ctx, const char *msgfile)
{
	char *cur_branch, *cur_head;
	char *objid, *path, *paths[2], tmp_objid[1024];
	char *dircache_path, *didx_path;
	struct stat s;
	FILE *didx_fp, *fp;
	FTS *dir;
	FTSENT *entry;
	unsigned int mode;
	size_t size;
	ssize_t len;
	char type;
	struct dir *ndir;
	struct dirent *ent;
	struct commit *com;

	dircache_path = get_dircache_path(dc_ctx);
	if (stat(dircache_path, &s) == -1)
		return EXIT_FAILURE;
	if (!S_ISDIR(s.st_mode))
		return EXIT_FAILURE; 
	/* get current branch */
	if (simple_branch_get(dc_ctx, &cur_branch) == EXIT_FAILURE)
		return EXIT_FAILURE;
	/* get current branch's head (commit's parent) */
	if (dc_ctx->db_ops->branch_get_head(dc_ctx->db_ctx, cur_branch, &cur_head) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/* FIXME: empty dirache directory */
	gen_dindex(dc_ctx, &didx_path);
	if ((didx_fp = fopen(didx_path, "r")) == NULL)
		return EXIT_FAILURE;
	objid = NULL;
	size = 0;
	path = NULL;	/* if not set to NULL, realloc() will get pissed. And, it can waste your day! */
	while ((len = getline(&path, &size, didx_fp)) != -1) {
		if (path[len-1] == '\n') {
			path[len-1] = 0;
			--len;
		}
		ndir = baseline_dir_new();

		paths[0] = (char *)path;
		paths[1] = NULL;
		if ((dir = fts_open(paths, FTS_NOCHDIR, 0)) == NULL)
			return EXIT_FAILURE;
		while ((entry = fts_read(dir)) != NULL) {
			if (entry->fts_name[0] == '.' && entry->fts_level != FTS_ROOTLEVEL) {
				fts_set(dir, entry, FTS_SKIP);
				continue;
			}
			if (entry->fts_info & FTS_F) {
#ifdef DEBUG
				printf("\t FILE: %s\n", entry->fts_path);
#endif
				if ((fp = fopen(entry->fts_path, "r")) == NULL)
					return EXIT_FAILURE;
				fscanf(fp, "%c %s %o", &type, tmp_objid, &mode);
				if (type == 'F') {
					ent = (struct dirent *)calloc(1, sizeof(struct dirent));
					ent->id = strdup(tmp_objid);
					ent->name = strdup(entry->fts_name);
					ent->mode = mode;
					ent->type = T_FILE;
					baseline_dir_append(ndir, ent);
					printf("[+] F %06o\t%s\n", mode, entry->fts_name);
				}
				else if (type == 'D') {
					ent = (struct dirent *)calloc(1, sizeof(struct dirent));
					ent->id = strdup(tmp_objid);
					ent->name = strdup(entry->fts_name);
					ent->mode = mode;
					ent->type = T_DIR;
					baseline_dir_append(ndir, ent);
					/* FIXME: dir mode are copied from dircache, hence not preserved */
					printf("[+] D %06o\t%s\n", mode, entry->fts_name);
				}
				fclose(fp);
				unlink(entry->fts_path);
			}
			else {
				if (entry->fts_level >= 1) {
					fts_set(dir, entry, FTS_SKIP);
				}
			}
		}
		fts_close(dir);
		dc_ctx->db_ops->insert_dir(dc_ctx->db_ctx, ndir);
		free(objid);
		objid = strdup(ndir->id);
		baseline_dir_free(ndir);
#ifdef DEBUG
		printf("dir \'%s\' commited with ID = %s\n", path, objid);
#endif
		if (stat(path, &s) == -1)
			return EXIT_FAILURE;
		/* FIXME: do not remove '.baseline/dircache' dir */
		if (rmdir(path) == -1)
			return EXIT_FAILURE;
		if ((fp = fopen(path, "w")) == NULL)
			return EXIT_FAILURE;
		fprintf(fp, "%c %s %06o", 'D', objid, s.st_mode);
		fclose(fp);
	}
	fclose(didx_fp);
	unlink(didx_path);
	/* temp */
	unlink(dircache_path);
	if (mkdir(dircache_path, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
		return EXIT_FAILURE;
	}
	/* generate commit */
	if ((com = baseline_helper_commit_build(dc_ctx, objid, cur_head, msgfile)) == NULL)
		return EXIT_FAILURE;
	/* insert the commit into the db */
	dc_ctx->db_ops->insert_commit(dc_ctx->db_ctx, com);
	printf("commit id: %s\n", com->id);
	unlink(path);
	/* update the branch's head */
	dc_ctx->db_ops->branch_set_head(dc_ctx->db_ctx, cur_branch, com->id);
	/* update the working dir */
	simple_workdir_set(dc_ctx, com->id);
	free(cur_branch);
	free(cur_head);
	free(path);
	free(objid);
	baseline_commit_free(com);
	return EXIT_SUCCESS;
}

static int
simple_branch_get(struct dircache_ctx *dc_ctx, char **branch_name)
{
	char *fname;
	size_t size = 0;
	FILE *fp;

	if (dc_ctx == NULL || branch_name == NULL)
		return EXIT_FAILURE;
	asprintf(&fname, "%s/branch", dc_ctx->repo_baselinepath);
	*branch_name = NULL;
	if ((fp = fopen(fname, "r")) == NULL)
		return EXIT_FAILURE;
	if (getline(branch_name, &size, fp) == -1) {
		free(*branch_name);
		return EXIT_FAILURE;
	}
	fclose(fp);
	free(fname);
	return EXIT_SUCCESS;
}

static int
simple_branch_set(struct dircache_ctx *dc_ctx, const char *branch_name)
{
	char *fname;
	FILE *fp;

	if (dc_ctx == NULL || branch_name == NULL)
		return EXIT_FAILURE;
	asprintf(&fname, "%s/branch", dc_ctx->repo_baselinepath);
	if ((fp = fopen(fname, "w")) == NULL)
		return EXIT_FAILURE;
	fprintf(fp, "%s", branch_name);
	fclose(fp);
	free(fname);
	return EXIT_SUCCESS;
}

static int
simple_workdir_get(struct dircache_ctx *dc_ctx, char **commit_id)
{
	char *fname;
	size_t size = 0;
	FILE *fp;

	if (dc_ctx == NULL || commit_id == NULL)
		return EXIT_FAILURE;
	asprintf(&fname, "%s/workdir", dc_ctx->repo_baselinepath);
	if ((fp = fopen(fname, "r")) == NULL)
		return EXIT_FAILURE;
	*commit_id = NULL;
	if (getline(commit_id, &size, fp) == -1) {
		/* assuming the file is empty */
		/* FIXME: check for errors */
		*commit_id = NULL;
	}
	fclose(fp);
	free(fname);
	return EXIT_SUCCESS;
}

static int
simple_workdir_set(struct dircache_ctx *dc_ctx, const char *commit_id)
{
	char *fname;
	FILE *fp;

	if (dc_ctx == NULL || commit_id == NULL)
		return EXIT_FAILURE;
	asprintf(&fname, "%s/workdir", dc_ctx->repo_baselinepath);
	if ((fp = fopen(fname, "w")) == NULL)
		return EXIT_FAILURE;
	fprintf(fp, "%s", commit_id);
	fclose(fp);
	free(fname);
	return EXIT_SUCCESS;
}

