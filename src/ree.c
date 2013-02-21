/* ree.c - ree(1) source code
 * ree - the round-robin style tee(1) - (C) 2013, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <getopt.h>


#define COUNTOF(A) (sizeof (A) / sizeof (A)[0])

#define MAX_DST 256


static bool g_append;
static bool g_skip_fail;
static bool g_flush;
static int g_next;
static int g_count;
static FILE *g_dst[MAX_DST];


static void process_args(int *argc, char ***argv);
static void init(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec);
static bool write_line(FILE *fp, const char* line);


static void
process_args(int *argc, char ***argv)
{
	char *a0 = (*argv)[0];

	for(int ch; (ch = getopt(*argc, *argv, "asfvch")) != -1;) {
		switch (ch) {
		case 'a':
			g_append = true;
			break;
		case 's':
			g_skip_fail = true;
			break;
		case 'f':
			g_flush = true;
			break;
		case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, a0, EXIT_FAILURE);
		}
	}

	*argc -= optind;
	*argv += optind;
}


static void
init(int *argc, char ***argv)
{
	process_args(argc, argv);

	if (!*argc) {
		fprintf(stderr, "no files given\n");
		exit(EXIT_FAILURE);
	}

	if (*argc > MAX_DST) {
		fprintf(stderr, "too many files given. max:%d\n", MAX_DST);
		exit(EXIT_FAILURE);
	}

	for(int i = 0; i < *argc; i++) {
		g_dst[g_count] = fopen((*argv)[i], g_append?"a":"w");
		if (!g_dst[g_count]) {
			perror((*argv)[i]);
			if (!g_skip_fail)
				exit(EXIT_FAILURE);
		} else
			g_count++;
	}
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define I(STR) fputs(STR "\n", str)
	I("==============================");
	I("== ree - round-robin tee(1) ==");
	I("==============================");
	fprintf(str, "usage: %s [-asfh] <file1> <file2> .. <fileN>\n", a0);
	I("");
	I("\t-a: Append to files instead of truncating them");
	I("\t-s: Skip files which could not be opened");
	I("\t-f: Flush after each line");
	I("\t-h: Display brief usage statement and terminate");
	I("");
	I("(C) 2013, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef I
	exit(ec);
}


static bool
write_line(FILE *fp, const char* line)
{
	size_t cnt = 0;
	size_t len = strlen(line);

	while(cnt < len)
	{
		errno = 0;
		size_t n = fwrite(line + cnt, 1, len - cnt, fp);
		if (n < len - cnt) {
			if (ferror(fp)) {
				fprintf(stderr, "ferror() is set\n");
				return false;
			} else if (feof(fp)) {
				fprintf(stderr, "feof() is set\n");
				return false;
			} else
				fprintf(stderr, "short write (%zu/%zu)\n",
						n, len-cnt);
		}

		cnt += n;
	}

	return true;
}


int
main(int argc, char **argv)
{
	init(&argc, &argv);

	char *line = NULL;
	size_t len = 0;
	ssize_t readlen;

	while ((readlen = getline(&line, &len, stdin)) != -1) {
		if (!write_line(g_dst[g_next], line)) {
			perror("write_line() failed");
			exit(EXIT_FAILURE);
		}

		if (g_flush)
			fflush(g_dst[g_next]);

		g_next = (g_next + 1) % g_count;
	}

	free(line);

	exit(EXIT_SUCCESS);
}
