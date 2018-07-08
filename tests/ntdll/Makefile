BASEPATH=../../
CFLAGS = -g -O0 -I../../include -DSTANDALONE
LIBOTOWI = ../../src/libotowi.so
LDADD = $(LIBOTOWI) -L../../src -lotowi
test: path.c Makefile $(LIBOTOWI)
	$(CC) $(CFLAGS) $< $(LDADD) -o $@

gdb: test
	LD_LIBRARY_PATH=$(BASEPATH)/src gdb test

run: test
	LD_LIBRARY_PATH=$(BASEPATH)/src ./test
