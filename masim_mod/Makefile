.PHONY: clean help

APPS	:= masim
MASIM	:= masim

CC	:= gcc
CFLAGS	:= -I$(IDIR) -O3 -Wall -std=gnu99  -g

LIBS := -lpthread

OBJ_MSM	:= bin/masim.o bin/misc.o

all: bin $(APPS)

bin:
	mkdir bin

bin/%.o: %.c masim.h
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

bin/%.s: %.c
	$(CC) -S -o $@ $< $(CFLAGS) $(LIBS)

$(MASIM): $(OBJ_MSM)
	$(CC) -o $@ $^ $(LIBS)


clean:
	rm -f *.o *.s $(APPS)
	rm -rf bin

clean_all: clean
	rm -f damon*
	rm -rf *.png

help:
	@echo "Usage: make <target>"
	@echo ""
	@echo "targets: $(APPS)"
	@echo ""
