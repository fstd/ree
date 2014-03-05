/* ree.c - ree(1) source code
 * ree - the round-robin style tee(1) - (C) 2013-2014, Timo Buhrmester
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

#include <err.h>

#include <getopt.h>


#define COUNTOF(A) (sizeof (A) / sizeof (A)[0])

#define MAX_DST 256


static bool s_append;
static bool s_skip_openfail;
static bool s_skip_writefail;
static bool s_flush;
static int s_next;
static int s_count;
static FILE *s_dst[MAX_DST];


static void process_args(int *argc, char ***argv);
static void init(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec);
static bool write_line(FILE *fp, const char* line);


static void
process_args(int *argc, char ***argv)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv, "asSfhV")) != -1;) {
		switch (ch) {
		case 'a':
			s_append = true;
			break;
		case 's':
			s_skip_openfail = true;
			break;
		case 'S':
			s_skip_writefail = true;
			break;
		case 'f':
			s_flush = true;
			break;
		case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
			break;
		case 'V':
			puts(PACKAGE_NAME " v" PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
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

	if (!*argc)
		errx(EXIT_FAILURE, "no files given (try -h)");

	if (*argc > MAX_DST)
		errx(EXIT_FAILURE, "too many files. max: %d", MAX_DST);

	/* open all files, fill s_dst with their FILE*s */
	for (int i = 0; i < *argc; i++) {
		errno = 0;
		s_dst[s_count] = (strcmp((*argv)[i], "-") == 0)
		    ? stdout : fopen((*argv)[i], s_append ? "a" : "w");

		if (!s_dst[s_count]) {
			warn((*argv)[i]);
			if (!s_skip_openfail)
				exit(EXIT_FAILURE);
		} else
			s_count++;
	}

	if (!s_count)
		errx(EXIT_FAILURE, "couldn't open any of the output files");

	errno = 0;
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define I(STR) fputs(STR "\n", str)
	I("====================================================================");
	I("==                    ree - round-robin tee(1)                    ==");
	I("====================================================================");
	fprintf(str, "usage: %s [-asSfhV] <file1> <file2> .. <fileN>\n", a0);
	I("");
	I("Reads linewise from standard input.  For each line, if it is");
	I("  the n-th line read so far, it is written out to the");
	I("  (n % N)-th output file (N being the total number of files");
	I("  specified (see above usage statement)).");
	I("In other words, the first line goes to file1, the second line");
	I("  goes to file2, the N-th line goes to fileN, and the N+1-th");
	I("  line goes to file1 again.");
	I("A file name consisting of a single dash ``-'' is interpreted");
	I("  as standard output (and may appear more than once).");
	I("");
	I("Parameter summary:");
	I("\t-a: Append to files instead of truncating them");
	I("\t-f: Flush after each line");
	I("\t-s: Skip files which could not be opened, instead of failing");
	I("\t-S: If writing to a file produces an error, skip it and ");
	I("\t      proceed with the remaining files, instead of failing");
	I("\t-h: Display this usage statement and terminate");
	I("\t-V: Print version information");
	I("");
	I("Version: " PACKAGE_VERSION);
	I("(C) 2013-2014, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef I
	exit(ec);
}


static bool
write_line(FILE *fp, const char* line)
{
	size_t cnt = 0;
	size_t len = strlen(line);

	while (cnt < len) {
		size_t n = fwrite(line + cnt, 1, len - cnt, fp);

		/* short return count implies write error */
		if (n < len - cnt)
			return false;

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
	size_t remain = s_count;

	while ((readlen = getline(&line, &len, stdin)) != -1) {
		errno = 0;
		if (!write_line(s_dst[s_next], line)) {
			warn("write_line() failed");
			if (!s_skip_writefail)
				exit(EXIT_FAILURE);

			fclose(s_dst[s_next]);
			s_dst[s_next] = NULL;
			if (--remain == 0)
				errx(EXIT_SUCCESS, "no files remaining");
		} else if (s_flush)
			fflush(s_dst[s_next]);

		/* advance, skip over entries which failed earlier */
		while (!s_dst[s_next = (s_next + 1) % s_count])
			;

		errno = 0;
	}

	if (ferror(stdin))
		err(EXIT_FAILURE, "getline() failed");
}
