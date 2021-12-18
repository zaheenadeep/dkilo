#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "gline.h"
#define BUFLEN 9

struct termios oldtty;


void termexit() {
	tcsetattr(SIN, TCSAFLUSH, &oldtty);
}

void terminit() {
	struct termios tty;
	tcgetattr(SIN, &oldtty);
	atexit(termexit);
	tty = oldtty;
	tty.c_lflag &= ~(ECHO|ICANON);
	tcsetattr(SIN, TCSAFLUSH, &tty);
}

	

int main() {
	char key, nl = '\n';
	terminit();

	while (read(SIN, &key, 1) && key != 'q') {
		write(SOUT, &key, 1);
	}
	write(SOUT, &nl, 1);
	
	return 0;
}
