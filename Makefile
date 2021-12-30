CC = gcc
CFLAGS = -ansi -Wall -g -O0 -Wwrite-strings -Wshadow -pedantic-errors -fstack-protector-all -Wextra

dkilo: dkilo.o                
	$(CC) dkilo.o -o dkilo

dkilo.o: dkilo.c

1: gline_driv.o gline1.o beep.o
	gcc gline_driv.o gline1.o beep.o -o gl1

gline_driv.o: gline.h
gline1.o: gline.h beep.h
gline2.o: gline.h beep.h
beep.o: beep.h
