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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "helper.h"
#include "config.h"

/* TODO: add support for multiple parents */
struct commit *
baseline_helper_commit_build(struct dircache_ctx *dc_ctx, const char *dir_id, const char *parent_head, const char *msg_file)
{
        char *line = NULL;
        const char *user_name, *user_email;
        size_t size = 0;
        struct stat s;
        struct commit *comm;
        FILE *mfp;

	comm = baseline_commit_new();
        if ((user_name = baseline_config_get_val("username")) == NULL)
		return NULL;
        if ((user_email = baseline_config_get_val("useremail")) == NULL)
		return NULL;
        if (msg_file == NULL)
                return NULL;
        if (stat(msg_file, &s) == -1)
                return NULL;
        comm->dir = strdup(dir_id);
        comm->author.name = strdup(user_name);
        comm->author.email = strdup(user_email);
	if ((comm->author.timestamp = time(NULL)) == (time_t)-1)
		return NULL;
        comm->committer.name = strdup(user_name);
        comm->committer.email = strdup(user_email);
	if ((comm->committer.timestamp = time(NULL)) == (time_t)-1)
		return NULL;
        if (parent_head != NULL) {
                comm->parents[0] = strdup(parent_head);
                comm->n_parents = 1;
        }
        else {
                comm->n_parents = 0;
        }
        /* commit message */
        if ((mfp = fopen(msg_file, "r")) == NULL)
                return NULL;
        /* FIXME: loading the whole message file in memory */
	/* use calloc() so that we can safely call strlcat() afterwards */
        if ((comm->message = (char *)calloc(1, s.st_size)) == NULL)
                return NULL;
        while (getline(&line, &size, mfp) != -1)
                strlcat(comm->message, line, s.st_size);
        fclose(mfp);
	return comm;
}

