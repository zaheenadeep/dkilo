#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "gline.h"
#include "beep.h"

FLAG Get_Line(char *prompt, char *buffer, int len) {
	char *cptr = buffer;
	char key;

	if (len < 2) return 0; /* safety check */
	printf("%s :", prompt);
	fflush(stdout);
	for (;;) {
		read(SIN, &key, 1);
		if (isprint(key)) {
			if (cptr - buffer >= len - 1) Beep();
			else {
				*cptr++ = key;
				printf("%c", key);
				fflush(stdout);
			}
		} else if (key == KEY_RET) {
			*cptr = NUL;
			printf("\n");
			return 1;
		} else Beep();
	}
}
