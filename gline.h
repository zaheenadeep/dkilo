#ifndef GLINE_H
#define GLINE_H

#define KEY_RET '\n'
#define NUL '\0'

typedef enum { FALSE, TRUE } FLAG;

FLAG Get_Line(char *prompt, char *buffer, int len);
#endif
