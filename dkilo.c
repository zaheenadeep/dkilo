#include <errno.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#define KEY_CTRL(c) ((c) & 0x1f)

/* shortcuts */
enum stdfd {
	SIN  = STDIN_FILENO,
	SOUT = STDOUT_FILENO,
	SERR = STDERR_FILENO
};

struct termios oldtty;

/*** terminal ***/

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

static void termexit() {
	tcsetattr(SIN, TCSAFLUSH, &oldtty);
}

static void terminit() {
	int e;
	struct termios tty;
	
	e = tcgetattr(SIN, &oldtty);
	if (e < 0) die("tcgetattr");
	
	atexit(termexit);

	tty = oldtty;
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

static char grabkey() {
	int nread;
	char c;

	while ((nread = read(SIN, &c, 1)) < 1)
		if (nread < 0 && errno != EAGAIN)
			die("read");

	return c;
}

static char processkey(char key) {
	switch (key) {
	case KEY_CTRL('q'):
		exit(EXIT_SUCCESS);
		break;
	}
}

/*** output ***/
static void refreshscreen() {
	write(SOUT, "\x1b[2J", 4);
}


/*** init ***/

int main() {
	terminit();

	for (;;) {
		refreshscreen();
		processkey(c = grabkey());
	}

	
	return 0;
}
