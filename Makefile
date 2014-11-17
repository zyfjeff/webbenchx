CC=gcc
CFLAGS= -lpthread
OBJ=webbenchx.c
 
webbenchx: webbenchx.o
	${CC} -o webbenchx webbenchx.o ${CFLAGS}

webbenchx.o:
	${CC} -o webbenchx.o -c ${OBJ}

install:
	mv webbenchx /usr/bin/

clean:
	rm -rf webbenchx.o webbenchx
	
