#define min(x, y) (x < y) ? x : y
#define max(x, y) (x > y) ? x : y
#define abs(x) (x > 0) ? x : (x * (-1))

extern char *EMPTY_BLOCK;

void write_block(char *, char *, char *);


void error_block(char *);


void errno_exit(char *);


void extn_cmd(const char *, char *, int);

