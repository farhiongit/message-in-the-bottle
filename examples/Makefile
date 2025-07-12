CFLAGS+=-I.. -Wall
#CFLAGS+=-DLIMITED_BUFFER
#CFLAGS+=-O
#CFLAGS+=-g
LDFLAGS=-pthread

all: build

.PHONY: build
build: bottle_auto_var bottle_perf bottle_fifo_example bottle_example bottle_simple_example bottle_token_example hanoi semaphore
	@echo "Type 'make run' to execute."

.PHONY: run
run: build
	./bottle_auto_var
	./bottle_fifo_example
	./bottle_simple_example
	./bottle_token_example
	./bottle_example
	./hanoi
	./bottle_perf
	./semaphore
