# To build this, download and extract duktape
#
# extract the content of duktape source code tar. You must end with a directory called duktape-DUKTAPE_VERSION (ie: duktape-1.5.1)
# please set the DUKTAPE_VERSION variable accordingly
#
# To build this on FreeBSD install sqlite3
#
# on Linux you must install:
#	libsqlite3-dev or similar 


DUKTAPE_VERSION=2.0.0

SQLITE_FLAGS=`pkg-config --cflags --silence-errors sqlite3`
COMPILE_SWITCHES=-std=c99 -O3

all: js.so

js.so: sqlitejs.o duktape.o
	cc -shared -o js.so sqlitejs.o duktape.o -lm
	@[ "`uname -s`" == "Darwin" ] && mv js.so js.dylib || :

sqlitejs.o: src/sqlitejs.c
	cc -c $(COMPILE_SWITCHES) $(SQLITE_FLAGS) -Iduktape-$(DUKTAPE_VERSION)/src/ src/sqlitejs.c

duktape.o:
	cc -c $(COMPILE_SWITCHES) duktape-$(DUKTAPE_VERSION)/src/duktape.c

clean:
	@rm *.o
