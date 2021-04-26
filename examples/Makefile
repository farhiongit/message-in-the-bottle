CFLAGS=-I..
#CFLAGS+=-O
CFLAGS+=-g
LDFLAGS=-pthread

all: bottle_auto_var bottle_perf bottle_fifo_example bottle_example bottle_simple_example bottle_token_example hanoi

.PHONY: run
run: all
	./bottle_auto_var
	./bottle_fifo_example
	./bottle_simple_example
	./bottle_token_example
	./bottle_example
	./hanoi
	time ./bottle_perf

hanoi: CFLAGS+=-DTRACE
