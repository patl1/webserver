#remove the -g when turning in the final project
CFLAGS=-Wall -Werror -g
LDLIBS=-lpthread

all:		sysstatd	

sysstatd:	list.o threadpool.o http.o

clean:
	rm -f *.o sysstatd
