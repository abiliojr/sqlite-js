# To build this, download and extract duktape
#
# extract the content of duktape source code tar. You must end with a directory called duktape-DUKTAPE_VERSION (e.g.,: duktape-2.3.0)
#
# To build this on FreeBSD install sqlite3
#
# on Linux you must install:
#	libsqlite3-dev or similar


SQLITE_FLAGS=`pkg-config --cflags --silence-errors sqlite3`
COMPILE_SWITCHES=-std=c99 -O3 -fPIC
DUKTAPE_LIB != ls -d duktape-?.?.?

all: js.so

js.so: sqlitejs.o duktape.o
	cc -shared -o js.so sqlitejs.o duktape.o -lm
	@[ "`uname -s`" == "Darwin" ] && mv js.so js.dylib || :

sqlitejs.o: src/sqlitejs.c
	cc -c $(COMPILE_SWITCHES) $(SQLITE_FLAGS) -I$(DUKTAPE_LIB)/src/ src/sqlitejs.c

duktape.o:
	cc -c $(COMPILE_SWITCHES) $(DUKTAPE_LIB)/src/duktape.c

clean:
	@rm *.o
