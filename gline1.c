#include <stdio.h>
#include <ctype.h>
#include "gline.h"
#include "beep.h"

FLAG Get_Line(char *prompt, char *buffer, int len) {
	char *cptr = buffer;
	int key;

	if (len < 2) return FALSE; /* safety check */
	printf("%s :", prompt);
	
	for (;;) {
		key = getchar();
		if (isprint(key)) {
			if (cptr - buffer >= len - 1) Beep();
			else {
				*cptr++ = key;
				printf("%c", key);
			}
		} else if (key == KEY_RET) {
			*cptr = NUL;
			printf("\n");
			return TRUE;
		} else Beep();
	}
}
