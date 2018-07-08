CFLAGS = -I../include
LDADD = ../src/libotowi.a
openfile: openfile.c Makefile $(LDADD)
	$(CC) $(CFLAGS) $< $(LDADD) -o $@
