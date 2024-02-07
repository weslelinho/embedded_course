INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS= -Wall
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= helloworld.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	helloworld

clean:
	-rm -f *.o *.d
	-rm -f perfmon helloworld

distclean:
	-rm -f *.o *.d
	-rm -f helloworld

helloworld: helloworld.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
