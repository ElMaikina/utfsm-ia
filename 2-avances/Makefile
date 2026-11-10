CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
LDLIBS = -lm

target = ttrp

$(target): ttrp.c
	$(CC) $(CFLAGS) -o $(target) ttrp.c $(LDLIBS)

clean:
	rm -f $(target)

.PHONY: clean
