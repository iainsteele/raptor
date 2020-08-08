CC=gcc

INC=-I /usr/local/xclib/inc -I ~/star-2018A/include
LDIR=-L ~/star-2018A/lib

XCLIB=/usr/local/xclib/lib/xclib_x86_64.a

LIBS=-lm -lcfitsio

OBJ = multrun.o

CFLAGS=$(INC) -DC_GNU64=400  -DOS_LINUX

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

multrun:  $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDIR) $(XCLIB) $(LIBS)

clean:
	rm *.o






