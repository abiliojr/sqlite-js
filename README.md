# SQLite's Cafe: JavaScript for SQLite

Create new SQL functions! Write them using JavaScript.


# Usage

This is an [SQLite](http://sqlite.org/) plugin. After loading it, you can use the function ```createjs``` to define your own functions, like this:

```
SELECT createjs({parameters});
```

This query will return ```ok``` if everything went fine.

SQL supports two types of functions:
  * Scalar: returns a result for each record.
  * Aggregate: process all records, then return a single final result.


## Scalar

When creating a scalar function, you must provide 2 parameters, in the following order:
  * Name: the function you are defining.
  * Code: well... the JavaScript code. Returns the result.

For example, to create a function that calculates the cosine of an angle:
```
SELECT createjs('cos', 'Math.cos(arg[0])');
```

And now you can calculate cosines on a query:
```
CREATE TABLE t(angle NUMERIC);
INSERT INTO t(angle) VALUES (0),(1.571),(3.1416);

SELECT cos(angle) FROM t; -- should return approximately {1, 0, -1}
```


## Aggregate

When creating aggregate functions, you must provide 4 parameters, in the following order:
  * Name:  the function you are defining.
  * Init:  this code will execute before the first record is processed.
  * Step:  code called on each record.
  * Final: code for the last step after all records have been processed. Returns the result.

For example, to create a function that calculates the [geometric mean](https://en.wikipedia.org/wiki/Geometric_mean) of a set of numbers:

```
SELECT createjs('gmean',
                 'prod = 1; n = 0;',
                 'n++; prod = prod * arg[0];',
                 'Math.pow(prod, 1/n)');
```

And now, used on a query:

```
CREATE TABLE data(val NUMERIC);
INSERT INTO data(val) VALUES (2), (4), (8);

SELECT gmean(val) FROM data; -- should return 4
```


# Parameters

Parameters supplied to the functions can be accessed from within JavaScript. The array: ```arg[i]``` contains the values. ```arg``` is a zero-based array (0 <= i < n), like all JavaScript arrays.

Example:
```
-- this is a pattern matching example using JavaScript's internal function
SELECT createjs('regex', 'arg[0].match(arg[1])[arg[2]]');

SELECT regex('abc 24 ef', '(\w+) (\d+)', 1); -- Should return abc
SELECT regex('abc 24 ef', '(\w+) (\d+)', 2); -- Should return 24
SELECT regex('abc 24 ef', '\d+', 0); -- Should return 24
```


# Returning values

As seen in the previous examples, the result is returned by leaving the value in the JavaScript machine stack, e.g., ```a = b + c; a;``` will return the result of ```b + c```.

Functions must always return a value. For aggregates, this is only performed on the final step. Currently, you can't return an array into SQLite.


# Reading from files

You can use the auxiliary function named ```loadfile``` to get the content of a file. This is useful to declare long functions. The signature is:

```
loadfile(filename,[type])
```

For example, to create a function which source code is stored in a file named ```longcode.js```, do:

```
select createjs('longcode', select loadfile('longcode.js'));
```

```loadfile``` can take an optional type parameter. If type is 'b', the file content will be read as a blob. This can be useful for other applications, e.g., reading binary files inside the database.


# Notes

* You can redefine a function at any time by calling ```createjs``` again. This holds true even for SQLite's native functions.


# Building

This plugin uses [Duktape](http://duktape.org/) as the JavaScript machine. You must download their code from http://duktape.org/download.html, and extract it in the plugin base directory. You must end up with a directory called duktape-DUKTAPE_VERSION (ie: duktape-3.2.0).

On FreeBSD, you must have ```sqlite3``` installed. The Linux equivalent is something like ```libsqlite3-dev``` (Ubuntu) or ```sqlite-devel``` (Fedora). The library names and their locations could be different on other Operating Systems. If that's the case, you may need to edit the Makefile.

On Windows, using Visual Studio, you can use the provided ```sqlitejs.mak```. You'll need to get [sqlite source code amalgamation](https://www.sqlite.org/download.html) from their site. The files sqlite3.h and sqlite3ext.h must be extracted inside a folder named sqlite. Afterwards, you can compile the dll like this:

```
nmake -f sqlitejs.mak
```

# Loading

To use this plugin from within the sqlite3 command line tool, you must run:

```.load path-to-plugin/js``` (for security reasons, SQLite needs you to provide the library path if the plugin is not located inside ```LD_LIBRARY_PATH```. Current directory '.' is a valid path, e.g., ```.load ./js```).

You could also use this from within almost any code that uses sqlite3 as a library. Refer to SQLite website to get more information about ```sqlite3_load_extension``` and ```sqlite3_enable_load_extension``` or look for their equivalents in your language bindings' documentation.


## License

This code is published under the Simplified BSD License.

Copyright (c) 2015, Abilio Marques All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors and should not be interpreted as representing official policies, either expressed or implied, of the FreeBSD Project.
