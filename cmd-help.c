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

#include "cmd.h"

int
cmd_help(int argc, char **argv)
{
	printf("This is a short list of the main commands supported by baseline:\n");
	printf("\tadd\t\tadd a file/directory to the dircache\n");
	printf("\tbranch [lcs]\tdisplay the current branch\n"
		"\t\t\tlist, create or switch branches\n");
	printf("\tcat [c]\t\twrite the content of a committed file to the stdout\n");
	printf("\tcheckout\tcheck out a commit into the working directory\n");
	printf("\tcommit [m]\tcommit the staged contents in the dircache to the repository\n");
	printf("\thelp\t\tdisplay this list\n");
	printf("\tinit\t\tinitialize a new repository in the current directory\n");
	printf("\tlog\t\tdisplay the commit logs\n");
	printf("\tls\t\tlist the content of a commit\n");
	printf("\tversion\t\tdisplay information about the installed version of baseline\n");
	return EXIT_SUCCESS;
}

