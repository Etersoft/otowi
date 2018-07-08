BASEPATH=../
CFLAGS = -g -O0 -I../include
LIBOTOWI = ../src/libotowi.so
LDADD = -L../src -lotowi
LIBOTOWI_STATIC = ../src/libotowi_static.a
LDADD_STATIC = $(LIBOTOWI_STATIC) -ldl
openfile: openfile.c Makefile $(LIBOTOWI)
	$(CC) $(CFLAGS) $< $(LDADD) -o $@
openfile.static: openfile.c Makefile $(LIBOTOWI_STATIC)
	$(CC) $(CFLAGS) $< $(LDADD_STATIC) -o $@
gdb: openfile
	export LD_LIBRARY_PATH=$(BASEPATH)/src ; gdb openfile
run: openfile
	export LD_LIBRARY_PATH=$(BASEPATH)/src ; ./openfile

