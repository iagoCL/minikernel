INCLUDEDIR=../include
INCLUDEDIR2=../../minikernel/include

CC=gcc
CFLAGS=-Wall -g -fPIC -I$(INCLUDEDIR) -I$(INCLUDEDIR2)

all: version libserv.a

version:
	@ln -sf misc.o_`getconf LONG_BIT` misc.o

serv.o: $(INCLUDEDIR)/servicios.h $(INCLUDEDIR2)/llamsis.h

libserv.a: serv.o misc.o
	ar -r $@ serv.o misc.o

clean:
	rm -f serv.o libserv.a misc.o