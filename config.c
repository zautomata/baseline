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

#include <stdio.h>		/* printf(3) */
#include <stdlib.h>		/* EXIT_SUCCESS */
#include <string.h>		/* strchr(3), strcmp(3) */

#include "config.h"

#define N_OPTIONS	3

static const char config_sample[] =
"#\n"
"# baseline configuration file\n"
"# edit as you wish, but be cautious!\n"
"#\n"
"\n"
"username = Anonymous\n"
"useremail = anon@localhost.localdomain\n"
"\n";

struct option {
	char *key;
	char val[32];
};

static struct option options[] = {
	{.key = "username", .val = ""},
	{.key = "useremail", .val = ""},
	{.key = "editor", .val = ""}
};

static char*
skip_spaces(char *str)
{
	if (str == NULL)
		return NULL;
	while ((*str == ' ' || *str == '\t') && *str != '\0')
		str++;
	return str;
}

int
baseline_config_create(const char *path)
{
	FILE *fp;
	if (path == NULL)
		return EXIT_FAILURE;
	if ((fp = fopen(path, "w")) == NULL)
		return EXIT_FAILURE;
	fwrite(config_sample, sizeof(char), sizeof(config_sample) - 1, fp);
	fclose(fp);
	return EXIT_SUCCESS;
}


int
baseline_config_load(const char *path)
{
	char *line = NULL;	/* must be initialized to NULL or getline()'s realloc() might complain */
	char *ptr, *tmp;
	char key[32], val[32];
	int i;
	size_t size = 0;
	ssize_t len;
	FILE *fp;
	if (path == NULL)
		return EXIT_FAILURE;
	if ((fp = fopen(path, "r")) == NULL)
		return EXIT_FAILURE;
	while ((len = getline(&line, &size, fp)) != -1) {
		/* skip blank lines */
		if (line[0] == '\n')
			continue;
		ptr = skip_spaces(line);
		/* skip comments */
		if (ptr[0] == '#' || ptr[0] == ';')
			continue;
		/* find the end of the key */
		tmp = ptr;
		while(*tmp != ' ' && *tmp != '\t'&& *tmp != '=')
			tmp++;
		*tmp++ = '\0';
		strlcpy(key, ptr, sizeof(key));
		tmp = skip_spaces(tmp);
		/* skip the '=' */
		if (*tmp == '=')
			tmp++;
		/* skip white spaces */
		ptr = tmp = skip_spaces(tmp);
		while (*tmp != '\n' && *tmp != '\0')
			tmp++;
		*tmp = '\0';
		strlcpy(val, ptr, sizeof(val));
		for (i = 0; i < N_OPTIONS ; i++) {
			if (strcmp(options[i].key, key) == 0) {
				strlcpy(options[i].val, val, sizeof(options[i].val));
				break;
			}
		}
	}
	fclose(fp);
	return EXIT_SUCCESS;
}

const char *
baseline_config_get_val(const char *key) {
	int i;
	for (i=0 ; i<N_OPTIONS ; i++)
		if(!strcmp(options[i].key, key))
			return options[i].val;
	return NULL;
}

