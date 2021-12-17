#include <stdlib.h>
#include <termios.h>
#include "gline.h"
#define BUFLEN 9

struct termios oldtty;


void termexit() {
	tcsetattr(SIN, TCSAFLUSH, &oldtty);
}

void terminit() {
	tcgetattr(SIN, &oldtty);
	atexit(termexit);
	struct termios tty = oldtty;
	tty.c_lflag &= ~(ECHO|ICANON);
	tcsetattr(SIN, TCSAFLUSH, &tty);
}

	

int main() {
	char buffer[BUFLEN];
	
	terminit();

	Get_Line((char *)"Type man", buffer, BUFLEN);

	return 0;
}
