/* dkilo - A text editor built using kilo's foundations
 *
 * Author: Zaheen Ahmed Deep
 *
 *******************************************************************************
 * Copyright (C) 2016 Salvatore Sanfilippo <antirez at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <string.h>


#define KEY_CTRL(c) ((c) & 0x1f)
#define KEY_ESC1(c) ((c) | 0x0100)
#define KEY_UP      KEY_ESC1('A')
#define KEY_DOWN    KEY_ESC1('B')
#define KEY_RIGHT   KEY_ESC1('C')
#define KEY_LEFT    KEY_ESC1('D')
#define KEY_PGUP    KEY_ESC1('5')
#define KEY_PGDOWN  KEY_ESC1('6')
#define KEY_HOME    KEY_ESC1('7')
#define KEY_END     KEY_ESC1('8')

#define AB_INIT     {NULL, 0}

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
#define VT_RCLF "\r\n"
#define VT_RCLF_N 2


/* shortcuts */
enum stdfd {
	SIN  = STDIN_FILENO,
	SOUT = STDOUT_FILENO,
	SERR = STDERR_FILENO
};

struct edconfig {
	int cx, cy;              /* cursor positions, starts from 0 */
	int rows, cols;          /* row and col count in the terminal */
	struct termios oldtty;   /* old termios struct */
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
			VT_ABAPPEND(ab, VT_RCLF);
	}
}

#define NRBUF 32
static void refreshscreen() {
	AppendBuffer ab = AB_INIT;
	char buf[NRBUF];
	int nscan;
	
	VT_ABAPPEND(&ab, VT_CURSOR_HIDE);
	
	VT_ABAPPEND(&ab, VT_CURSOR_HOME);	
	drawrows(&ab);

	/* move cursor to new position */
	nscan = snprintf(buf, NRBUF, "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abappend(&ab, buf, nscan);
	
	VT_ABAPPEND(&ab, VT_CURSOR_SHOW);

	/* flush ab */
	write(SOUT, ab.data, ab.len);
	abfree(&ab);
}

/*** input ***/
static int getkey() {
	int nread;
	int c = 0;

	/* keep trying to read 1 byte until successful */
	while ((nread = read(SIN, &c, 1)) < 1)
		/* die for non-EAGAIN errors */
		if (nread < 0 && errno != EAGAIN) die("read");

	if (c == '\x1b') {		
		nread = read(SIN, &c, 1);
		if (nread < 1 || c != '[') return '\x1b';

		c = 0;
		nread = read(SIN, &c, 1);
		if (nread < 1) return '\x1b';

                /* for VT100 */
		/* if c is between 0-9, return if next key not ~ */
		if (isdigit(c)) {
			char d;
			nread = read(SIN, &d, 1);
			if (nread < 1 || d != '~') return '\x1b';
		}

		/* for xterm */
		switch (c) {
		case 'H':
			c = KEY_HOME;
			break;
		case 'F':
			c = KEY_END;
			break;
		default:
			return KEY_ESC1(c);
		}
	}
	
	return c;
}

static char processkey(int key) {
	switch (key) {
	case KEY_UP:
		if (E.cy > 0) E.cy--;
		break;
	case KEY_DOWN:
		if (E.cy < E.rows) E.cy++;
		break;
	case KEY_LEFT:
		if (E.cx > 0) E.cx--;
		break;
	case KEY_RIGHT:
		if (E.cx < E.cols) E.cx++;
		break;
	case KEY_HOME:
		E.cx = 0;
		break;
	case KEY_END:
		E.cx = E.cols - 1;
		break;
	case KEY_PGUP:
		E.cy = 0;
		break;
	case KEY_PGDOWN:
		E.cy = E.rows - 1;
		break;
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
	E.cx = 0;
	E.cy = 0;
	
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
