# to run this: "nmake -f sqlitejs.mak"
#
# Extract the content of duktape source code tar.
# You must end with a directory called duktape-DUKTAPE_VERSION (ie: duktape-1.3.0).
# Please set the DUKTAPE_VERSION variable accordingly.
#
# Extract sqlite3.h and sqlite3ext.h inside a folder called sqlite.
#
# it should generate js.dll as the output
#
# Note: this Makefile hasn't been tested

DUKTAPE_VERSION=1.3.0

CPP=cl.exe

ALL :
	@if not exist ".\sqlite\sqlite3.h" echo "please extract SQLite's header files inside the sqlite folder"
	@if not exist ".\sqlite" mkdir sqlite

	$(CPP) /Ot /GD /nologo /W3 /I .\duktape-$(DUKTAPE_VERSION) /I .\sqlite .\duktape-$(DUKTAPE_VERSION)\src\duktape.c .\src\sqlitejs.c -link -dll -out:js.dll
	@erase *.obj

