CFLAGS = -I../../include
LIBOTOWI = ../../src/libotowi.so
LDADD = -L../../src -lotowi

main: main.c Makefile $(LIBOTOWI)
	$(CC) $(CFLAGS) $< $(LDADD) -o $@
main.cpp: main.c
	cpp $(CFLAGS) $< -o $@

run: main
	LD_LIBRARY_PATH=$(BASEPATH)/src gdb main
