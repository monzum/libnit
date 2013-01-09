CC=gcc
CFLAGS=-fPIC
LDFLAGS=-shared -Wl,-soname,libnetworkinterpose.so
LDLIBS=-ldl

default: libnetworkinterpose.so

%.so: %.o
	$(CC) $(LDFLAGS) -g -o $@ $< $(LDLIBS)

clean:
	rm -f *.so *.o
