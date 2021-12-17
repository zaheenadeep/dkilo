#ifndef GLINE_H
#define GLINE_H

#define KEY_RET '\n'
#define KEY_BACK '\b'
#define NUL '\0'

typedef int FLAG;
enum stdfd { SIN, SOUT, SERR };

FLAG Get_Line(char *prompt, char *buffer, int len);
#endif
