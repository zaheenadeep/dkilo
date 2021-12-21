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

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

void termexit() {
	tcsetattr(SIN, TCSAFLUSH, &oldtty);
}

void terminit() {
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

int main() {
	int e;
	char c;
	
	terminit();

	do {
		c = '\0';
		e = read(SIN, &c, 1);
		if (e < 0 && errno != EAGAIN) die("read");
		
		printf("%d\r\n", c);
	} while (c != KEY_CTRL('q'));
	
	return 0;
}
