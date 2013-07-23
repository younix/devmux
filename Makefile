CFLAGS=-std=c99 -pedantic -Wall

.PHONY: all clean
all: devmux
clean:
	rm -f devmux in out

devmux: devmux.c
	gcc $(CFLAGS) -o $@ $<
