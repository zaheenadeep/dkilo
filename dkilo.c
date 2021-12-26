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
	char len;
} AppendBuffer;

/* returns true if nbytes is written; false otherwise */
static int ewrite(int fd, const void *buf, size_t nbytes) {
	return write(fd, buf, nbytes) != nbytes;
}


/* reset screen with write */
static void wreset() {
	write(SOUT, "\x1b[2J", 4); /* reset */
	write(SOUT, "\x1b[H", 3);  /* cursor back */
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
	ab->data = realloc(ab->data, ab->len + len);
	if (ab->data == NULL)
		return;

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
		if (r < E.rows - 1)
			abappend(ab, "\r\n", 2);
	}
}

static void refreshscreen() {
	AppendBuffer ab = AB_INIT;

	/* wreset with abappend */
	abappend(&ab, "\x1b[2J", 4); /* reset */
	abappend(&ab, "\x1b[H", 3); /* cursor back */
	
	drawrows(&ab);
	abappend(&ab, "\x1b[H", 3); /* cursor back */

	/* flush ab */
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

static int fetchdims(int *rp, int *cp) {
	struct winsize ws;
	
	if (ioctl(SOUT, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		const int NBYTES = 12, MAXDIM = 999;
		/* go MAXDIM slots right and down */
		if (printf("\x1b[%dC\x1b[%dB", MAXDIM, MAXDIM) != NBYTES)
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
