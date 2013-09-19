CFLAGS = -std=c99 -W -Wall -O3 -D_POSIX_C_SOURCE
LDFLAGS = -lXrandr

xrandrd: xrandrd.c

test: xrandrd
	./xrandrd

install: xrandrd
	mkdir -p $(HOME)/bin
	mkdir -p $(HOME)/.config/autostart
	sed "s@^Exec=.*@Exec=$(HOME)/bin/xrandrd@" < xrandrd.desktop > $(HOME)/.config/autostart/xrandrd.desktop
	cp xrandrd $(HOME)/bin/xrandrd

uninstall:
	rm -f $(HOME)/.config/autostart/xrandrd.desktop $(HOME)/bin/xrandrd

clean:
	rm -f xrandrd

