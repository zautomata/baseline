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

#include "objects.h"

struct file*
baseline_file_new()
{
	struct file *file = (struct file *)malloc(sizeof(struct file));
	file->id = NULL;
	return file;
}

void
baseline_file_free(struct file *file)
{
	if (file == NULL)
		return;
	free(file->id);
	if (file->loc == LOC_MEM)
		free(file->buffer);
	free(file);
}

struct commit*
baseline_commit_new()
{
	struct commit *comm = (struct commit *)malloc(sizeof(struct commit));
	comm->id = NULL;
	comm->dir = NULL;
	comm->author.name = NULL;
	comm->author.email = NULL;
	comm->n_parents = 0;
	comm->message = NULL;
	return comm;
}

void
baseline_commit_free(struct commit *comm)
{
	int i;
	if (comm == NULL)
		return;
	free(comm->id);
	free(comm->dir);
	for (i=0 ; i<comm->n_parents ; i++)
		free(comm->parents[i]);
	free(comm->message);
	free(comm);
}

struct dir*
baseline_dir_new()
{
	struct dir *dir = (struct dir *)malloc(sizeof(struct dir));
	dir->id = NULL;
	dir->parent = NULL;
	dir->children = NULL;
	return dir;
}

void
baseline_dir_free(struct dir *dir)
{
	struct dirent *it, *ptr;

	if (dir == NULL)
		return;
	if (dir->children == NULL) {
		free(dir);
		return;
	}
	for (it = dir->children ; it != NULL ;) {
		ptr = it;
		it = it->next;
		free(ptr);
	}
	free(dir);
}

void
baseline_dir_append(struct dir *dir, struct dirent *ent)
{
	struct dirent *it;

	if (dir == NULL || ent == NULL)
		return;
	if (dir->children == NULL) {
		dir->children = ent;
	}
	else {
		for (it = dir->children ; it->next != NULL ; it = it->next);
		it->next = ent;
	}
	ent->next = NULL;
}

