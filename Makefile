CFLAGS = -std=c99 -W -Wall -O3 -D_POSIX_C_SOURCE
LDFLAGS = -lXrandr

xrandrd: xrandrd.c

test: xrandrd
	./xrandrd

clean:
	rm -f xrandrd

