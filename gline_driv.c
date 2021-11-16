#include "gline.h"
#define BUFLEN 9

int main() {
	char buffer[BUFLEN];

	Get_Line((char *)"Type man", buffer, BUFLEN);

	return 0;
}
