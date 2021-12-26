#include <errno.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <string.h>

#define KEY_CTRL(c) ((c)&0x1f)
#define AB_INIT {NULL, 0}

#define VT_WRITE(cmd) (write(SOUT, (cmd), (cmd##_N)))
#define VT_ABAPPEND(abufp, cmd) (abappend((abufp), (cmd), (cmd##_N)))
#define VT_CURSOR_HOME "\x1b[H"
#define VT_CURSOR_HOME_N 3
#define VT_ERASE_ALL "\x1b[2J"
#define VT_ERASE_ALL_N 4
#define VT_ERASE_REST_OF_LINE "\x1b[0K"
#define VT_ERASE_REST_OF_LINE_N 4
#define VT_CURSOR_HIDE "\x1b[?25l"
#define VT_CURSOR_HIDE_N 6
#define VT_CURSOR_SHOW "\x1b[?25h"
#define VT_CURSOR_SHOW_N 6


/* shortcuts */
enum stdfd {
	SIN  = STDIN_FILENO,
	SOUT = STDOUT_FILENO,
	SERR = STDERR_FILENO
};

struct edconfig {
	struct termios oldtty;
	int rows;
	int cols;
};

struct edconfig E;

typedef struct apbuf {
	char *data;
	int len;
} AppendBuffer;

/* returns true if nbytes is written; false otherwise */
static int ewrite(int fd, const void *buf, int nbytes) {
	return write(fd, buf, nbytes) != nbytes;
}


/* reset screen with write */
static void wreset() {
	VT_WRITE(VT_ERASE_ALL);
	VT_WRITE(VT_CURSOR_HOME);
}

/*** terminal ***/

static void die(const char *msg) {
	wreset();
	
	perror(msg);
	exit(EXIT_FAILURE);
}

static void termexit() {
	tcsetattr(SIN, TCSAFLUSH, &E.oldtty);
}

static void terminit() {
	int e;
	struct termios tty;
	
	e = tcgetattr(SIN, &E.oldtty);
	if (e < 0) die("tcgetattr");
	
	atexit(termexit);

	tty = E.oldtty;
	tty.c_lflag &= ~( ECHO | ICANON | ISIG | IEXTEN );
	tty.c_iflag &= ~( IXON | ICRNL | INPCK | ISTRIP | BRKINT );
	tty.c_oflag &= ~( OPOST );
	tty.c_cflag |= CS8;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1;
	
	e = tcsetattr(SIN, TCSAFLUSH, &tty);
	if (e < 0) die("tcsetattr");
}

/*** append buffer ***/
static void abappend(AppendBuffer *ab, const char *data, int len) {
	char *new = realloc(ab->data, ab->len + len);
	if (new == NULL)
		return;

	ab->data = new;
	memcpy(ab->data + ab->len, data, len);
	ab->len += len;
}

static void abfree(AppendBuffer *ab) {
	free(ab->data);
}

/*** output ***/
static void drawrows(AppendBuffer *ab) {
	int r;
	for (r = 0; r < E.rows; r++) {
		abappend(ab, "~", 1);
		VT_ABAPPEND(ab, VT_ERASE_REST_OF_LINE);
		if (r < E.rows - 1)
			abappend(ab, "\r\n", 2);
	}
}

static void refreshscreen() {
	AppendBuffer ab = AB_INIT;

	VT_ABAPPEND(&ab, VT_CURSOR_HIDE);
	
	VT_ABAPPEND(&ab, VT_CURSOR_HOME);	
	drawrows(&ab);
	VT_ABAPPEND(&ab, VT_CURSOR_HOME);

	VT_ABAPPEND(&ab, VT_CURSOR_SHOW);

	/* flush ab */
	printf("%d\n", ab.len);
	write(SOUT, ab.data, ab.len);
	abfree(&ab);
}

/*** input ***/
static char getkey() {
	int nread;
	char c;

	/* keep trying to read 1 byte until successful */
	while ((nread = read(SIN, &c, 1)) < 1)
		/* die for non-EAGAIN errors */
		if (nread < 0 && errno != EAGAIN)
			die("read");

	return c;
}

static char processkey(char key) {
	switch (key) {
	case KEY_CTRL('q'):
		wreset();
		exit(EXIT_SUCCESS);
		break;
	}
}

static int fetchcursorpos(int *rp, int *cp) {
	if (ewrite(SOUT, "\x1b[6n", 4)) return -1;
	if (scanf("\x1b[%d;%d", rp, cp) != 2) return -1;
	
	return 0;
}

#define MAXDIM 999
static int fetchdims(int *rp, int *cp) {
	struct winsize ws;
	
	if (ioctl(SOUT, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		/* go MAXDIM slots right and down */
		if (printf("\x1b[%dC\x1b[%dB", MAXDIM, MAXDIM) != 12)
			return -1;
		fflush(stdout);
		return fetchcursorpos(rp, cp);
	}
	else {
		*rp = ws.ws_row;
		*cp = ws.ws_col;
		return 0;
	}
}

/*** init ***/

static void Einit() {
	if (fetchdims(&E.rows, &E.cols) < 0) die("fetchdims");	
}

int main() {	
	terminit();
	Einit();

	for (;;) {
		refreshscreen();
		processkey(getkey());
	}
	
	return 0;
}
