#include <errno.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>

#define KEY_CTRL(c) ((c) & 0x1f)

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

/* returns true if nbytes is written; false otherwise */
static int ewrite(int fd, const void *buf, size_t nbytes) {
	return write(fd, buf, nbytes) != nbytes;
}

/*** output ***/
static void cursortotop() {
	write(SOUT, "\x1b[H", 3); /* cursor back */
}

static void resetscreen() {
	write(SOUT, "\x1b[2J", 4); /* reset */
	cursortotop();
}

static void drawrows() {
	int r;
	for (r = 0; r < E.rows; r++) {
		write(SOUT, "~\r\n", 3);
	}
}

static void refreshscreen() {
	resetscreen();

	drawrows();

	cursortotop();
}

/*** terminal ***/

static void die(const char *msg) {
	resetscreen();
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
		resetscreen();
		exit(EXIT_SUCCESS);
		break;
	}
}

static int fetchcursorpos(int *rp, int *cp) {
	char c;
	
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
