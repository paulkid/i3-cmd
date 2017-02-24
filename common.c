#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_PADDING 10
#define min(x, y) (x < y) ? x : y
#define max(x, y) (x > y) ? x : y
#define abs(x) (x > 0) ? x : (x * (-1))


const char *EMPTY_BLOCK = "";

static char *fg_clr_dflt = "#FFFFFF";
static char *bg_clr_dflt = "#000000";

static int blkpadsz = 5; // Default

static bool valid_clr(char *clr) {
	if (clr == NULL) return false;
	
	const int len = strlen(clr);
	bool valid = len == 7 && clr[0] == '#';

	for (int c = 1; c < len; c++)
		valid = valid && isalnum(clr[c]);

	return valid;
}

void set_block_padding(int pad) {
	blkpadsz = min(abs(pad), MAX_PADDING);	
}

void write_block(char *fg_clr, char *bg_clr, char *output_str) {
	
	if ( ! strcmp(output_str, EMPTY_BLOCK))
		return;

	char padl[MAX_PADDING + 1], padr[MAX_PADDING + 1];
	memset(padl, ' ', blkpadsz);
	memset(padr, ' ', blkpadsz);
	padl[blkpadsz] = '\0';

	// Make right-padding slightly smaller than left.
	padr[max(0, blkpadsz - 2)] = '\0';

	fg_clr = valid_clr(fg_clr) ? fg_clr : fg_clr_dflt;
	bg_clr = valid_clr(bg_clr) ? bg_clr : bg_clr_dflt;

	printf("<span foreground=\"%s\" background=\"%s\">", fg_clr, bg_clr);
	printf("%s%s%s</span>\n", padl, output_str, padr);

	exit( 0 );
}

void error_block(char *msg) {
	write_block("#000000", "#B41717", msg);
}

void errno_exit(char *prefix) {
	printf("Error exit.\n%s\n %s\n", prefix, strerror(errno));
	exit( -1 );
}

void extn_cmd(const char *cmd, char *buf, int bufsz) {
	FILE *fd = popen(cmd, "r");

	if (fd == NULL) 
		errno_exit("popen() failed for command.");

	if (fgets(buf, bufsz, fd) == NULL)
		errno_exit("Failed to read output from command.");

	int len = strlen(buf);
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
}
