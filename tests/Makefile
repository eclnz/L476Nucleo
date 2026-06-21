CC = gcc
CFLAGS = -Wall -Wextra -g -I stubs

TESTS = test_ringbuf test_diskio

all: $(TESTS)

test_ringbuf: test_ringbuf.c ../Core/Src/ringbuf.c
	$(CC) $(CFLAGS) -o $@ $^

test_diskio: test_diskio.c
	$(CC) $(CFLAGS) -I ../Core/Inc -o $@ $^

run: $(TESTS)
	./test_ringbuf
	./test_diskio

clean:
	rm -f $(TESTS)
