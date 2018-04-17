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

#include <sys/stat.h>	/* stat(3) */

#include <stdio.h>	/* rename(2) */
#include <stdlib.h>	/* malloc(2) */
#include <string.h>	/* str*(2) , mem*(2) */
#include <unistd.h>	/* access(2) */

#include <sha2.h>	/* SHA256*() */

#include <fts.h>        /* fts_*(3) */

#include "defaults.h"
#include "objects.h"
#include "objdb.h"

int objdb_baseline_get_ops(struct objdb_ops **);
static char * get_objdb_dir(struct objdb_ctx *);
static int objdb_bl_open(struct objdb_ctx **, const char *, const char *);
static int objdb_bl_close(struct objdb_ctx *);
static int objdb_bl_init(struct objdb_ctx *);
static int objdb_bl_insert_file(struct objdb_ctx *, struct file *);
static int objdb_bl_insert_dir(struct objdb_ctx *, struct dir *);
static int objdb_bl_insert_commit(struct objdb_ctx *, struct commit *);
static int objdb_bl_select_file(struct objdb_ctx *, const char *, struct file *);
static int objdb_bl_select_dir(struct objdb_ctx *, const char *, struct dir *);
static int objdb_bl_select_commit(struct objdb_ctx *, const char *, struct commit *);
static int objdb_bl_remove(struct objdb_ctx *, const char *, const char *);
static int objdb_bl_branch_create(struct objdb_ctx *, const char *);
static int objdb_bl_branch_create_from(struct objdb_ctx *, const char *, const char *);
static int objdb_bl_branch_if_exists(struct objdb_ctx *, const char *, int *);
static int objdb_bl_branch_set_head(struct objdb_ctx *, const char *, const char *);
static int objdb_bl_branch_get_head(struct objdb_ctx *, const char *, char **);
static int objdb_bl_branch_ls(struct objdb_ctx *);


static const struct objdb_ops baseline_objdb_ops = {
	.name = "baseline",
	.version = "1.0",
	.init = objdb_bl_init,
	.open = objdb_bl_open,
	.close = objdb_bl_close,
	.insert_file = objdb_bl_insert_file,
	.insert_dir = objdb_bl_insert_dir,
	.insert_commit = objdb_bl_insert_commit,
	.select_file = objdb_bl_select_file,
	.select_dir = objdb_bl_select_dir,
	.select_commit = objdb_bl_select_commit,
	.remove = objdb_bl_remove,
	.branch_create = objdb_bl_branch_create,
	.branch_create_from = objdb_bl_branch_create_from,
	.branch_if_exists = objdb_bl_branch_if_exists,
	.branch_set_head = objdb_bl_branch_set_head,
	.branch_get_head = objdb_bl_branch_get_head,
	.branch_ls = objdb_bl_branch_ls,
	.fsck = NULL,
	.compress = NULL,
	.dedup = NULL
};

#define N_MAINDIRS	5
static const char *main_dirs[] = {
	"files",
	"dirs",
	"commits",
	"branches",
	"tags"
};

int
objdb_baseline_get_ops(struct objdb_ops **ops)
{
	if (ops == NULL)
		return EXIT_FAILURE;
	*ops = (struct objdb_ops *)malloc(sizeof(struct objdb_ops));
	memcpy(*ops, &baseline_objdb_ops, sizeof(struct objdb_ops));
	return EXIT_SUCCESS;
}

/* FIXME */
static char *
get_objdb_dir(struct objdb_ctx *ctx)
{
	char *db_dir_name = NULL;
	int len;
	if (ctx == NULL)
		return NULL;
	/* malloc() + strlcpy() + strlcat() = asprintf() */
	len = asprintf(&db_dir_name, "%s/%s", ctx->db_path, ctx->db_name);
	return db_dir_name;
}

static int
read_line(int fd, char *buf, size_t len)
{
	char ch, *ptr = buf;
	int nbytes = 0;

	while (read(fd, &ch, 1) > 0) {
		if (nbytes++ == len - 1) {
			/* TODO: use err() instead */
			fprintf(stderr, "line too big\n");
			return -1;
		}
		*ptr++ = (ch == '\n') ? '\0' : ch;
		if (ch == '\n')
			break;
	}
	return nbytes > 0 ? nbytes - 1 : 0;
}

static int
is_hex(const char *str)
{
	if (str == NULL)
		return 0;
	do {
		if (*str == '\0')
			break;
		if ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f'))
			continue;
		return 0;
	} while (*str++);
	return 1;
}

static int
is_dec(const char *str)
{
	if (str == NULL)
		return 0;
	do {
		if (*str == '\0')
			break;
		if (*str >= '0' && *str <= '9')
			continue;
		return 0;
	} while (*str++);
	return 1;
}

static int
is_oct(const char *str)
{
	if (str == NULL)
		return 0;
	do {
		if (*str == '\0')
			break;
		if (*str >= '0' && *str <= '7')
			continue;
		return 0;
	} while (*str++);
	return 1;
}

/*
 * converts struct commit to char*
 */
char*
commit_serialize(struct commit *comm)
{
	char *raw = NULL;

	if (comm == NULL)
		goto ret;
	if (comm->n_parents == 0)
		asprintf(&raw, "dir %s\nauthor %s <%s> %llu\ncommitter %s <%s> %llu\n%s\n",
			comm->dir, comm->author.name, comm->author.email, comm->author.timestamp,
			comm->committer.name, comm->committer.email, comm->committer.timestamp, comm->message);
	else if (comm->n_parents == 1)
		asprintf(&raw, "dir %s\nparent %s\nauthor %s <%s> %llu\ncommitter %s <%s> %llu\n%s\n",
			comm->dir, comm->parents[0], comm->author.name, comm->author.email, comm->author.timestamp,
			comm->committer.name, comm->committer.email, comm->committer.timestamp, comm->message);
	/* TODO: support more than 1 parent */
ret:
	return raw;
}

static int
commit_deserialize(int fd, struct commit *comm)
{
	char buf[128], *p1, *p2;
	off_t end, pos;
	size_t len;
	ssize_t n;

	/* 1. */
	/* find the commit's dir */
	if (read_line(fd, buf, sizeof(buf)) == -1)
		return EXIT_FAILURE;
	/* "dir " + obj id */
	if (strlen(buf) != 4 + SHA256_DIGEST_LENGTH * 2)
		goto parse_error;
	/* check if line starts with "dir " */
	if (strstr(buf, "dir ") != buf)
		goto parse_error;
	/* skip "dir " */
	p1 = buf + 4;
	if (!is_hex(p1) || strlen(p1) != SHA256_DIGEST_LENGTH * 2)
		goto parse_error;
	comm->dir = strdup(p1);

	/* 2. */
	/* find the commit's parent */
	/* TODO: support more than parent */
	if (read_line(fd, buf, sizeof(buf)) == -1)
		return EXIT_FAILURE;
	/* check if line starts with "parent " */
	if (strstr(buf, "parent ") == buf) {
		p1 = buf + 7;
		if (!is_hex(p1) || strlen(p1) != SHA256_DIGEST_LENGTH * 2)
			goto parse_error;
		comm->n_parents = 1;
		comm->parents[0] = strdup(p1);

		/* read the next line */
		if (read_line(fd, buf, sizeof(buf)) == -1)
			return EXIT_FAILURE;
	}
	else {
		comm->n_parents = 0;
	}

	/* 3. */
	/* find the commit's author */
	/* check if line starts with "author " */
	if (strstr(buf, "author ") != buf)
		goto parse_error;
	/* skip "author " */
	p1 = buf + 7;
	/* find author's name */
	if ((p2 = strstr(p1, " <")) == NULL)
		goto parse_error;
	*p2 = '\0';
	comm->author.name = strdup(p1);
	/* find author's email */
	p1 = ++p2;
	if (*p1 != '<')
		goto parse_error;
	p1++;
	if ((p2 = strchr(p1, '>')) == NULL)
		goto parse_error;
	*p2 = '\0';
	comm->author.email = strdup(p1);
	/* find author's timestamp */
	p1 = ++p2;
	if (!is_dec(++p1))
		goto parse_error;
	/* OpenBSD time_t is now 64-bit */
	/* FIXME: other POSIX systems still use 32-bit time_t */
	comm->author.timestamp = atoll(p1);

	/* 4. */
	/* find the commit's committer */
	if (read_line(fd, buf, sizeof(buf)) == -1)
		return EXIT_FAILURE;
	/* check if line starts with "committer " */
	if (strstr(buf, "committer ") != buf)
		goto parse_error;
	/* skip "committer " */
	p1 = buf + 10;
	/* find committer's name */
	if ((p2 = strstr(p1, " <")) == NULL)
		goto parse_error;
	*p2 = '\0';
	comm->committer.name = strdup(p1);
	/* find committer's email */
	p1 = ++p2;
	if (*p1 != '<')
		goto parse_error;
	p1++;
	if ((p2 = strchr(p1, '>')) == NULL)
		goto parse_error;
	*p2 = '\0';
	comm->committer.email = strdup(p1);
	/* find committer's timestamp */
	p1 = ++p2;
	if (!is_dec(++p1))
		goto parse_error;
	/* OpenBSD time_t is now 64-bit */
	/* FIXME: other POSIX systems still use 32-bit time_t */
	comm->committer.timestamp = atoll(p1);

	/* 5. */
	/* find the commit's message */
	pos = lseek(fd, 0, SEEK_CUR);
	end = lseek(fd, 0, SEEK_END);
	/* even if off_t is 64-bits, we would never be able to support messages of size more than 32-bits */
	len = (size_t)(end - pos);
	lseek(fd, pos, SEEK_SET);
	/* for now the max. size of a message is 10 MBytes */
	if (len < 1048576) {
		comm->message = (char *)malloc(len);
		if ((n = read(fd, comm->message, len)) == -1) {
			fprintf(stderr, "error reading file.\n");
			return EXIT_FAILURE;
		}
		comm->message[n - 1] = '\0';
	}
	else {
		goto bigmsg_error;
	}

	return EXIT_SUCCESS;

parse_error:
	fprintf(stderr, "error parsing file, not a valid commit.\n");
	return EXIT_FAILURE;
bigmsg_error:
	fprintf(stderr, "error parsing file, commit message too big.\n");
	return EXIT_FAILURE;
}


/*
 * converts struct dir to char*
 */
char*
dir_serialize(struct dir *dir)
{
	char *entry = NULL, *raw = NULL, *ptr, ch = ' ';
	size_t size = 1, newsize;
	struct dirent *it;

	if (dir == NULL)
		goto ret;
	raw = (char *)malloc(sizeof(char));
	*raw = '\0';
	if (dir->children == NULL)
		goto ret;
	for (it = dir->children ; it != NULL ; it = it->next) {
		if (it->type == T_FILE)
			ch = 'F';
		else if (it->type == T_DIR)
			ch = 'D';
		/* assuming asprintf() uses realloc() */
		/* free(entry); */
		asprintf(&entry, "%06o %s %s\n", it->mode, it->id, it->name);
		/* allocation failed, need to do something! */
		if (entry == NULL)
			return NULL;
		newsize = size + strlen(entry);
		if ((ptr = realloc(raw, newsize)) == NULL) {
			/* allocation failed, need to do something! */
			return NULL;
		}
		raw = ptr;
		size = newsize;
		strlcat(raw, entry, size);
	}
	free(entry);
ret:
	return raw;
}

static int
dir_deserialize(int fd, struct dir *d)
{
	char buf[512], *id, *name, *p1, *p2;
	int n;
	mode_t mode;
	struct dirent *head = NULL, *tail = NULL, *q = NULL;

	while (1) {
		if ((n = read_line(fd, buf, sizeof(buf))) == -1)
			printf("EXIT\n");
		if (n == 0)
			break;
		/* mode + obj id + at least 1 char name */
		if (n < 9 + SHA256_DIGEST_LENGTH * 2)
			goto parse_error;

		/* 1. mode */
		p1 = buf;
		if ((p2 = strchr(p1, ' ')) == NULL)
			goto parse_error;
		*p2 = '\0';
		if (!is_oct(p1) || strlen(p1) != 6)
			goto parse_error;
		/* TODO: find a safer alternative */
		sscanf(p1, "%6o", &mode);

		/* 2. id */
		p1 = ++p2;
		if ((p2 = strchr(p1, ' ')) == NULL)
			goto parse_error;
		*p2 = '\0';
		if (!is_hex(p1) || strlen(p1) != SHA256_DIGEST_LENGTH * 2)
			goto parse_error;
		id = p1;

		/* 3. name */
		p1 = ++p2;
		if (strlen(p1) == 0)
			goto parse_error;
		name = p1;

		q = (struct dirent *)malloc(sizeof(struct dirent));
		q->mode = mode;
		q->id = strdup(id);
		q->name = strdup(name);
		q->next = NULL;
		if (head == NULL) {
			head = q;
			tail = q;
		}
		else {
			tail->next = q;
			tail = tail->next;
		}
	}

	d->children = head;
	return EXIT_SUCCESS;

parse_error:
	fprintf(stderr, "error parsing file, not a valid directory.\n");
	return EXIT_FAILURE;
}

static char *
file_gen_id(struct file *obj)
{
	char **objid = NULL, *ptr;
	char buf[4096];
	int i, n;
        off_t offset;
        u_int8_t digest[SHA256_DIGEST_LENGTH];
        SHA2_CTX hash_ctx;

	SHA256Init(&hash_ctx);
	/* TODO: locking */
	if (((struct file *)obj)->loc == LOC_FS) {
		/* save file offset */
		offset = lseek(obj->fd, 0, SEEK_CUR);
		do {
			n = read(obj->fd, buf, sizeof(buf));
			if (n == -1)
				return NULL;
			SHA256Update(&hash_ctx, buf, n);
			if (n <= sizeof(buf))
				break;
		} while (1);
		/* restore file offset */
		lseek(obj->fd, offset, SEEK_SET);
	}
	else {
		SHA256Update(&hash_ctx, obj->buffer, strlen(obj->buffer));
	}
	objid = &(obj->id);
	SHA256Final(digest, &hash_ctx);
	*objid = (char *)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
	for (i=0, ptr=*objid ; i<SHA256_DIGEST_LENGTH ; i++, ptr+=2) {
		snprintf(ptr, 3, "%02x", digest[i]);
	}
	return *objid;
}

static char *
commit_gen_id_and_serialize(struct commit *obj, char **raw)
{
	char **objid = NULL, *ptr, *serialized = NULL;
	int i;
	u_int8_t digest[SHA256_DIGEST_LENGTH];
	SHA2_CTX hash_ctx;

	SHA256Init(&hash_ctx);
	serialized = commit_serialize(obj);
	SHA256Update(&hash_ctx, serialized, strlen(serialized));
	objid = &(obj->id);
	SHA256Final(digest, &hash_ctx);
	*objid = (char *)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
	for (i=0, ptr=*objid ; i<SHA256_DIGEST_LENGTH ; i++, ptr+=2) {
		snprintf(ptr, 3, "%02x", digest[i]);
	}
	*raw = serialized;
	return *objid;
}

static char *
commit_gen_id(struct commit *obj)
{
	char *raw;
	char *id = commit_gen_id_and_serialize(obj, &raw);
	free(raw);
	return id;
}

static char *
dir_gen_id_and_serialize(struct dir *obj, char **raw)
{
	char **objid = NULL, *ptr, *serialized = NULL;
	int i;
	u_int8_t digest[SHA256_DIGEST_LENGTH];
	SHA2_CTX hash_ctx;

	SHA256Init(&hash_ctx);
	serialized = dir_serialize(obj);
	SHA256Update(&hash_ctx, serialized, strlen(serialized));
	objid = &(obj->id);
	SHA256Final(digest, &hash_ctx);
	*objid = (char *)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
	for (i=0, ptr=*objid ; i<SHA256_DIGEST_LENGTH ; i++, ptr+=2) {
		snprintf(ptr, 3, "%02x", digest[i]);
	}
	*raw = serialized;
	return *objid;
}

static char *
dir_gen_id(struct dir *obj)
{
	char *raw;
	char *id = dir_gen_id_and_serialize(obj, &raw);
	free(raw);
	return id;
}

static int
objdb_bl_open(struct objdb_ctx **ctx, const char *db_name, const char *db_path)
{
	if (ctx == NULL)
		return EXIT_FAILURE;
	*ctx = (struct objdb_ctx *)malloc(sizeof(struct objdb_ctx));
	asprintf(&((*ctx)->db_name), "%s", db_name);
	asprintf(&((*ctx)->db_path), "%s", db_path);
	asprintf(&((*ctx)->db_version), "1.0");
	return EXIT_SUCCESS;
}

static int
objdb_bl_close(struct objdb_ctx *ctx)
{
	if (ctx == NULL)
		return EXIT_FAILURE;
	free(ctx->db_name);
	free(ctx->db_path);
	free(ctx->db_version);
	free(ctx);
	return EXIT_SUCCESS;
}

static int
objdb_bl_init(struct objdb_ctx *ctx)
{
	char *db_dir_name = NULL, *maindir = NULL;
	int i, retval = EXIT_FAILURE;
	struct stat s;
	/* check if path exists */
	if (stat(ctx->db_path, &s) == -1)
		goto ret;
	db_dir_name = get_objdb_dir(ctx);
	if (mkdir(db_dir_name, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		goto ret;
	/* try to create main sub-dirs as well */
	for (i=0 ; i<N_MAINDIRS ; i++) {
		asprintf(&maindir, "%s/%s", db_dir_name, main_dirs[i]);
		if (mkdir(maindir, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
			free(maindir);
			goto ret;
		}
		free(maindir);
	}
	retval = EXIT_SUCCESS;
ret:
	if (db_dir_name != NULL)
		free(db_dir_name);
	return retval;
}

static int
objdb_bl_insert_file(struct objdb_ctx *ctx, struct file *file)
{
	char *db_dir_name, *full_path, *obj_file_name, *obj_hash, *tmp_file_name;
	char *buf[4096];
	int n, retval, tmpfd;
	off_t offset;

	db_dir_name = get_objdb_dir(ctx);
	if (db_dir_name == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	asprintf(&full_path, "%s/%s", db_dir_name, "files");
	obj_hash = file_gen_id(file);
	/* create a temp file */
	asprintf(&tmp_file_name, "%s/tmp.XXXXXX", full_path);
        if ((tmpfd = mkstemp(tmp_file_name)) == -1) {
		retval =  EXIT_FAILURE;
		goto ret;
	}
	if (file->loc == LOC_FS) {
		/* save file offset */
		offset = lseek(file->fd, 0, SEEK_CUR);
		/* copy file content */
		while((n = read(file->fd, buf, sizeof(buf))) > 0)
			write(tmpfd, buf, n);
		/* restore file offset */
		lseek(file->fd, offset, SEEK_SET);
	}
	else if (file->loc == LOC_MEM) {
		write(tmpfd, file->buffer, strlen(file->buffer));
	}
	close(tmpfd);
	asprintf(&obj_file_name, "%s/%s", full_path, obj_hash);
	/* check if file is already there */
	if (access(obj_file_name, F_OK) != -1) {
		unlink(tmp_file_name);
		goto success;
	}
	if (rename(tmp_file_name, obj_file_name)) {
		retval = EXIT_FAILURE;
		goto ret;
	}

success:
	retval = EXIT_SUCCESS;
ret:
	/* assuming free(NULL) is safe */
	free(db_dir_name);
	free(full_path);
	free(obj_file_name);
	free(tmp_file_name);
	return retval;
}

static int
objdb_bl_insert_dir(struct objdb_ctx *ctx, struct dir *dir)
{
	char *db_dir_name, *full_path, *obj_file_name, *obj_hash, *tmp_file_name;
	char *data;
	int retval, tmpfd;
	FILE *tmpfp;

	db_dir_name = get_objdb_dir(ctx);
	if (db_dir_name == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	asprintf(&full_path, "%s/%s", db_dir_name, "dirs");
	obj_hash = dir_gen_id_and_serialize(dir, &data);

	/* create a temp file */
	asprintf(&tmp_file_name, "%s/tmp.XXXXXX", full_path);
        if ((tmpfd = mkstemp(tmp_file_name)) == -1) {
                retval =  EXIT_FAILURE;
		goto ret;
        }
	if ((tmpfp = fdopen(tmpfd, "w")) == NULL) {
                unlink(tmp_file_name);
                close(tmpfd);
                retval = EXIT_FAILURE;
		goto ret;
        }
	fwrite(data, sizeof(u_int8_t), strlen(data), tmpfp);
	fclose(tmpfp);
	asprintf(&obj_file_name, "%s/%s", full_path, obj_hash);
	/* check if file is already there */
	if (access(obj_file_name, F_OK) != -1) {
		unlink(tmp_file_name);
		goto success;
	}
	if (rename(tmp_file_name, obj_file_name)) {
		retval = EXIT_FAILURE;
		goto ret;
	}

success:
	retval = EXIT_SUCCESS;
ret:
	/* assuming free(NULL) is safe */
	free(db_dir_name);
	free(full_path);
	free(obj_file_name);
	free(tmp_file_name);
	return retval;
}

static int
objdb_bl_insert_commit(struct objdb_ctx *ctx, struct commit *comm)
{
	char *db_dir_name, *full_path, *obj_file_name, *obj_hash, *tmp_file_name;
	char *data;
	int retval, tmpfd;
	FILE *tmpfp;

	db_dir_name = get_objdb_dir(ctx);
	if (db_dir_name == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	asprintf(&full_path, "%s/%s", db_dir_name, "commits");
	obj_hash = commit_gen_id_and_serialize(comm, &data);

	/* create a temp file */
	asprintf(&tmp_file_name, "%s/tmp.XXXXXX", full_path);
        if ((tmpfd = mkstemp(tmp_file_name)) == -1) {
                retval =  EXIT_FAILURE;
		goto ret;
        }
	if ((tmpfp = fdopen(tmpfd, "w")) == NULL) {
                unlink(tmp_file_name);
                close(tmpfd);
                retval = EXIT_FAILURE;
		goto ret;
        }
	fwrite(data, sizeof(u_int8_t), strlen(data), tmpfp);
	fclose(tmpfp);
	asprintf(&obj_file_name, "%s/%s", full_path, obj_hash);
	/* check if file is already there */
	if (access(obj_file_name, F_OK) != -1) {
		unlink(tmp_file_name);
		goto success;
	}
	if (rename(tmp_file_name, obj_file_name)) {
		retval = EXIT_FAILURE;
		goto ret;
	}

success:
	retval = EXIT_SUCCESS;
ret:
	/* assuming free(NULL) is safe */
	free(db_dir_name);
	free(full_path);
	free(obj_file_name);
	free(tmp_file_name);
	return retval;
}

static int
objdb_bl_remove(struct objdb_ctx *ctx, const char *group_name, const char *obj_name)
{
	return EXIT_SUCCESS;
}

static int
objdb_bl_branch_create(struct objdb_ctx *ctx, const char *branch_name)
{
	char *db_dir_name = NULL;
	char *branch_path = NULL;
	char *branch_head = NULL;
	int retval = EXIT_SUCCESS;
	FILE *fp;

	if (branch_name == NULL)
		return EXIT_FAILURE;
	/* FIXME: validate branch name & check if already exists */
	db_dir_name = get_objdb_dir(ctx);
	asprintf(&branch_path, "%s/branches/%s", db_dir_name, branch_name);
#ifdef DEBUG
	printf("creating branch %s\n", branch_path);
#endif
	if (mkdir(branch_path, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	/* create head */
	asprintf(&branch_head, "%s/head", branch_path);
	if ((fp = fopen(branch_head, "w")) == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	fclose(fp);
ret:	
	free(db_dir_name);
	free(branch_path);
	free(branch_head);
	return retval;
}

static int
objdb_bl_branch_create_from(struct objdb_ctx *ctx, const char *new_branch, const char *orig_branch)
{
	char *orig_head = NULL;
	int retval = EXIT_FAILURE;

	if (new_branch == NULL || orig_branch == NULL)
		return EXIT_FAILURE;
	/* get the original branch's name */
	if (objdb_bl_branch_get_head(ctx, orig_branch, &orig_head) == EXIT_FAILURE)
		goto ret;
	/* create the new branch */
	if (objdb_bl_branch_create(ctx, new_branch) == EXIT_FAILURE)
		goto ret;
	/* set the new branch's head */
	if (objdb_bl_branch_set_head(ctx, new_branch, orig_head) == EXIT_FAILURE)
		goto ret;
	retval = EXIT_SUCCESS;
ret:
	free(orig_head);
	return retval;
}

static int
objdb_bl_branch_if_exists(struct objdb_ctx *ctx, const char *branch_name, int *exist)
{
	char *db_dir_name = NULL;
	char *branch_path = NULL;
	int retval;
	struct stat s;

	if (branch_name == NULL)
		return EXIT_FAILURE;
	*exist = 0;
	db_dir_name = get_objdb_dir(ctx);
	asprintf(&branch_path, "%s/branches/%s", db_dir_name, branch_name);
	/* TODO: check if path is writable */
	if (stat(branch_path, &s) == 0) {
		if (S_ISDIR(s.st_mode)) {
			*exist = 1;
			retval = EXIT_SUCCESS;
		}
		else {
			*exist = 0;
			retval = EXIT_FAILURE;
		}
	}
	else {
		*exist = 0;
		retval = EXIT_SUCCESS;
	}
	free(db_dir_name);
	free(branch_path);
	return retval;
}

static int
objdb_bl_branch_set_head(struct objdb_ctx *ctx, const char *branch_name, const char *head_objid)
{
	char *db_dir_name = NULL;
	char *branch_head = NULL;
	int retval = EXIT_SUCCESS;
	FILE *head_fp;
	db_dir_name = get_objdb_dir(ctx);
	asprintf(&branch_head, "%s/branches/%s/head", db_dir_name, branch_name);
	if ((head_fp = fopen(branch_head, "w")) == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	fprintf(head_fp, "%s", head_objid);
	fclose(head_fp);
ret:
	free(db_dir_name);
	free(branch_head);
	return retval;
}

static int
objdb_bl_branch_get_head(struct objdb_ctx *ctx, const char *branch_name, char **head_objid)
{
	char *db_dir_name = NULL;
	char *branch_head = NULL;
	int retval = EXIT_SUCCESS;
	size_t size = 0;
	FILE *head_fp;

	if (ctx == NULL || branch_name == NULL || head_objid == NULL)
		return EXIT_FAILURE;
	db_dir_name = get_objdb_dir(ctx);
	asprintf(&branch_head, "%s/branches/%s/head", db_dir_name, branch_name);
	if ((head_fp = fopen(branch_head, "r")) == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	*head_objid = NULL;
	if (getline(head_objid, &size, head_fp) == -1) {
		/* assuming the head file is empty */
		/* FIXME: check for errors */
		*head_objid = NULL;
	}
	fclose(head_fp);
ret:
	free(db_dir_name);
	free(branch_head);
	return retval;
}

static int
objdb_bl_branch_ls(struct objdb_ctx *ctx)
{
	char *branches_path, *db_dir_name;
	char *paths[2];
	FTS *dir;
	FTSENT *entry;

	db_dir_name = get_objdb_dir(ctx);
	asprintf(&branches_path, "%s/branches", db_dir_name);

	paths[0] = (char *)branches_path;
	paths[1] = NULL;
	if ((dir = fts_open(paths, FTS_NOCHDIR, 0)) == NULL)
		return EXIT_FAILURE;
	while ((entry = fts_read(dir)) != NULL) {
		if (entry->fts_level == FTS_ROOTLEVEL)
			continue;
		if (entry->fts_info & FTS_D) {
			printf("* %s\n", entry->fts_name);
		}
		else {
			if (entry->fts_level >= 1)
				fts_set(dir, entry, FTS_SKIP);
		}
	}
	fts_close(dir);
	free(db_dir_name);
	free(branches_path);
	return EXIT_SUCCESS;
}

static int
objdb_bl_select_commit(struct objdb_ctx *ctx, const char *objid, struct commit *comm)
{
	char *db_dir_name, *full_path;
	int fd, retval;

	db_dir_name = get_objdb_dir(ctx);
	if (db_dir_name == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	asprintf(&full_path, "%s/%s/%s", db_dir_name, "commits", objid);
	/* check if file is already there */
	if (access(full_path, F_OK) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}

	if ((fd = open(full_path, O_RDONLY)) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}

	comm->id = strdup(objid);
	commit_deserialize(fd, comm);

	close(fd);

success:
	retval = EXIT_SUCCESS;
ret:
	free(db_dir_name);
	free(full_path);
	return retval;
}

static int
objdb_bl_select_dir(struct objdb_ctx *ctx, const char *objid, struct dir *d)
{
	char *db_dir_name, *full_path;
	int fd, retval;

	db_dir_name = get_objdb_dir(ctx);
	if (db_dir_name == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	asprintf(&full_path, "%s/%s/%s", db_dir_name, "dirs", objid);
	/* check if file is already there */
	if (access(full_path, F_OK) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}

	if ((fd = open(full_path, O_RDONLY)) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}

	d->id = strdup(objid);
	dir_deserialize(fd, d);

	close(fd);

success:
	retval = EXIT_SUCCESS;
ret:
	free(db_dir_name);
	free(full_path);
	return retval;
}

static int
objdb_bl_select_file(struct objdb_ctx *ctx, const char *objid, struct file *f)
{
	char *db_dir_name, *full_path;
	int fd, retval;

	db_dir_name = get_objdb_dir(ctx);
	if (db_dir_name == NULL) {
		retval = EXIT_FAILURE;
		goto ret;
	}
	asprintf(&full_path, "%s/%s/%s", db_dir_name, "files", objid);
	/* check if file is already there */
	if (access(full_path, F_OK) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}

	if ((fd = open(full_path, O_RDONLY)) == -1) {
		retval = EXIT_FAILURE;
		goto ret;
	}

	f->id = strdup(objid);
	f->loc = LOC_FS;
	f->fd = fd;

	/* the caller should close the file descriptor */

success:
	retval = EXIT_SUCCESS;
ret:
	free(db_dir_name);
	free(full_path);
	return retval;
}

