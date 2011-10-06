# srv

CC = gcc
CFLAGS = -pipe -O2 -W -Wall -Wno-unused -pedantic -fPIC -fomit-frame-pointer

all: urlsrv

urlsrv: urlsrv.c
	${CC} ${CFLAGS} -c urlsrv.c
	${CC} ${CFLAGS} -levent urlsrv.o -o urlsrv

clean:
	rm -f *.o urlsrv
