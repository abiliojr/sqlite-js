/*
*
* SQLite's Cafe: Javascript for SQLite
*
* Create new SQL functions! Write them using Javascript.
*
* By: Abilio Marques <https://github.com/abiliojr/>
*
*
* After loading this plugin, you can use the newly function "createjs" to create your own
* functions.
*
* See README.md for more information on how to use it.
*
* This code is published under the Simplified BSD License
*
*/

#include <stdio.h>
#include <math.h>
#include "duktape.h"


#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1;
#if defined( _WIN32 )
#define _USE_MATH_DEFINES
#endif /* _WIN32 */


//
// Get a new Duk heap
//

static void *createDukHeap(void) {
	duk_context *ctx = NULL;

	ctx = duk_create_heap_default();
	return ctx;
}


//
// Destroy a previously allocated Duk heap
//

static void destroyDukHeap(void *ctx) {
	duk_destroy_heap(ctx);
}


//
// Loads an SQLite value into the Duk stack.
// Tries to match the types
//

static void pushSqliteToDuk(int i, sqlite3_value *value, duk_context *destination, duk_idx_t arr_idx) {
	duk_int_t n;
	duk_double_t d;
	const char *str;

	switch (sqlite3_value_type(value)) {

		case SQLITE_FLOAT:
			d = sqlite3_value_double(value);
			duk_push_number(destination, d);
			break;

		case SQLITE_INTEGER:
			n = sqlite3_value_int(value);
			duk_push_int(destination, n);
			break;

		case SQLITE_NULL:
			duk_push_null(destination);
			break;

		case SQLITE_TEXT:
			str = (char *) sqlite3_value_text(value);
			duk_push_string(destination, str);
			break;

		case SQLITE_BLOB:
			str = sqlite3_value_blob(value);
			n = sqlite3_value_bytes(value);
			duk_push_lstring(destination, str, n);
			break;
	}

	duk_put_prop_index(destination, arr_idx, i);
}


//
// Gets a value from the Duk stack and returns it as an SQLite value.
// Tries to match the types
//

static void popDukToSqlite(duk_context *from, sqlite3_context *ctx) {
	int n;
	double d;
	const char *str;

	if (duk_get_top_index(from) == DUK_INVALID_INDEX) {
		sqlite3_result_null(ctx);
		return;
	}

	switch (duk_get_type(from, -1)) {

		case DUK_TYPE_NONE:
		case DUK_TYPE_UNDEFINED:
		case DUK_TYPE_NULL:
			sqlite3_result_null(ctx);
			return;

		case DUK_TYPE_NUMBER:
			d = duk_get_number(from, -1);
			if (floor(d) == d) {
				sqlite3_result_int(ctx, (int) d);
			} else {
				sqlite3_result_double(ctx, d);
			}
			break;

		case DUK_TYPE_STRING:
			str = duk_get_string(from, -1);
			sqlite3_result_text(ctx, str, -1, SQLITE_TRANSIENT);
			break;

		case DUK_TYPE_BOOLEAN:
			n = duk_get_boolean(from, -1);
			sqlite3_result_int(ctx, n);
			break;

		case DUK_TYPE_BUFFER:
			str = duk_get_buffer(from, -1, &n);
			sqlite3_result_blob(ctx, str, n, SQLITE_TRANSIENT);
			break;

		default:
			sqlite3_result_error(ctx, "Unsupported return type", -1);
			break;
	}

	duk_pop(from);
}


static duk_context *getAssociatedDukHeap(sqlite3_context *ctx) {
	return sqlite3_user_data(ctx);
}


//
// Loads a code chunk into the Duk stack
//

static int pushDukChunk(duk_context *where, const unsigned char *code, const unsigned char *init,
	const unsigned char *final) {
	
	duk_pop_n(where, duk_get_top(where)); // clean the stack

	if (duk_pcompile_string(where, 0, (char *) code)) return 1;

	if (init != NULL && final != NULL) {
		if(duk_pcompile_string(where, 0, (char *) init)) return 2;
		if(duk_pcompile_string(where, 0, (char *) final)) return 3;
	}

	return 0;
}


//
// Loads the passed values inside arg[]
//

static void pushDukParams(duk_context *where, int num_values, sqlite3_value **values) {
	int i;
	duk_idx_t arr_idx;

	arr_idx = duk_push_array(where);

	for (i = 0; i < num_values; i++) {
		pushSqliteToDuk(i, values[i], where, arr_idx);
	}

	duk_put_global_string(where, "arg");
}


//
// Executes code chunk previously stored in the Duk stack
//

static void executeDukChunk(duk_context *where, int chunk) {
	duk_dup(where, chunk);
	duk_call(where, 0);
}


//
// Check for return value
//
static int checkStackLen(duk_context *stack, sqlite3_context *ctx, int validLen) {
	if (duk_get_top(stack) != validLen) {
		// #todo: check next message
		sqlite3_result_error(ctx, "Invalid javascript stack length! " \
			"This normally happens if your code doesn't return any value.", -1);
		return 0;
	}

	return 1;
}

//
// Store a pointer into a Duk table located at pos
//

static void storePointer(duk_context *where, int pos, const char *name, void *p) {
	duk_push_pointer(where, p);
	duk_put_prop_string(where, pos, name);
}


//
// Find a pointer inside a Duk table located at pos
//

static void *findPointer(duk_context *where, int pos, const char *name) {
	void *result = NULL;
	
	if (duk_get_prop_string(where, pos, name)) {
		result = duk_to_pointer(where, -1);
	}

	duk_pop(where);
	return result;
}


//
// Executes scalar function (called by SQLite)
//

static void sql_scalar_duk(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	duk_context *L;

	L = getAssociatedDukHeap(ctx);
	pushDukParams(L, num_values, values);
	executeDukChunk(L, 0);
	if (checkStackLen(L, ctx, 2)) popDukToSqlite(L, ctx); // stack size = 2 (code and return val)
}


//
// Executes init and step parts of an aggregate function (called by SQLite)
//

static void sql_aggregate_duk(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	duk_context *L;
	int *notFirstTime = sqlite3_aggregate_context(ctx, sizeof(int));

	if (notFirstTime == NULL) {
		sqlite3_result_error_nomem(ctx);
		return;
	}

	L = getAssociatedDukHeap(ctx);

	if (*notFirstTime == 0) {
		// it must execute the init chunk once
		executeDukChunk(L, 1);
		duk_pop(L); // pop any return value
		*notFirstTime = 1;
	}

	pushDukParams(L, num_values, values);
	executeDukChunk(L, 0);
	duk_pop(L); // pop any return value
}


//
// Executes the final code chunk of an aggregate function (called by SQLite)
//

static void sql_aggregate_duk_final(sqlite3_context *ctx) {
	duk_context *L;
	L = getAssociatedDukHeap(ctx);

	executeDukChunk(L, 2);

	if (checkStackLen(L, ctx, 4)) popDukToSqlite(L, ctx); // stack size = 4 (3x code and return val)
}


//
// Check that all the function creation parameters are strings
//

static const char *checkCreateDukParameters(int num_values, sqlite3_value **values) {
	int i, adj;

	const char *errors[] = {
		"Invalid function name, string expected",
		"Invalid function code, string expected",
		"Invalid init code, string expected",
		"Invalid step code, string expected",
		"Invalid final code, string expected"
	};

	for (i = 0; i < num_values; i++) {
		adj = (num_values == 2 || i == 0 ? 0 : 1);
		if (sqlite3_value_type(values[i]) != SQLITE_TEXT) return errors[i + adj];
	}

	return NULL;

}


//
// check result of code compilation and print a human readable result
//

static void messageCodeCompilingResult(sqlite3_context *ctx, int res, int num_values) {
	switch (res) {
		case 0:
			sqlite3_result_text(ctx, "ok", -1, SQLITE_TRANSIENT);
			break;

		case 1:
			if (num_values == 2) {
				sqlite3_result_error(ctx, "compilation problem, please check source code", -1);
			} else {
				sqlite3_result_error(ctx, "compilation problem, please check step source code", -1);
			}
			break;

		case 2:
			sqlite3_result_error(ctx, "compilation problem, please check init source code", -1);
			break;

		case 3:
			sqlite3_result_error(ctx, "compilation problem, please check final source code", -1);
			break;
	}
}


//
// Create a new SQL js function (called by SQLite)
//

static void sql_createJS(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	duk_context *L, *functionTable;

	const char *msg, *name;
	int retVal;

	msg = checkCreateDukParameters(num_values, values);
	if (msg != NULL) {
		sqlite3_result_error(ctx, msg, -1);
		return;
	}

	name = (char *) sqlite3_value_text(values[0]);

	functionTable = getAssociatedDukHeap(ctx);

	L = findPointer(functionTable, 0, name);
	if (L == NULL) {
		L = createDukHeap();
		storePointer(functionTable, 0, name, L);
		// now register the function within SQLite
		if (num_values == 2) {
			// scalar
			sqlite3_create_function_v2(sqlite3_context_db_handle(ctx), (char *) name, -1,
				SQLITE_UTF8, (void *) L, sql_scalar_duk, NULL, NULL, destroyDukHeap);
		} else {
			// aggregate
			sqlite3_create_function_v2(sqlite3_context_db_handle(ctx), (char *) name, -1,
				SQLITE_UTF8, (void *) L, NULL, sql_aggregate_duk, sql_aggregate_duk_final, destroyDukHeap);
		}

	}

	// load the code inside the stack
	if (num_values == 2) {
		// scalar
		retVal = pushDukChunk(L, sqlite3_value_text(values[1]), NULL, NULL);

	} else {
		// aggregate
		retVal = pushDukChunk(L, sqlite3_value_text(values[2]), sqlite3_value_text(values[1]),
			sqlite3_value_text(values[3]));
	}

	if (retVal != 0) {
		sqlite3_create_function_v2(sqlite3_context_db_handle(ctx), (char *) name, -1,
			SQLITE_UTF8, NULL, NULL, NULL, NULL, NULL);
	}

	messageCodeCompilingResult(ctx, retVal, num_values);
}


//
// Load data from a file (return a text or a blob)
//

static void sql_loadFile(sqlite3_context *ctx, int num_values, sqlite3_value **values) {
	FILE *f;
	char *buffer = 0;
	int length;

	f = fopen(sqlite3_value_text(values[0]), "rb");

	if (!f) {
		sqlite3_result_error(ctx, "Unable to open the file", -1);
		return;
	}

	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer = sqlite3_malloc(length + 1);

	if (buffer) {
		fread(buffer, 1, length, f);
		
		if (num_values == 2 && sqlite3_value_text(values[1])[0] == 'b') {
			sqlite3_result_blob(ctx, buffer, length, sqlite3_free);
		} else {
			sqlite3_result_text(ctx, buffer, length, sqlite3_free);
		}
	} else {
		sqlite3_result_error(ctx, "unable to get free memory to hold the file contents", -1);
	}

	fclose (f);
}


//
// plugin main
//
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_extension_init(sqlite3 *db, char **error, const sqlite3_api_routines *api) {
	duk_context *mainHeap; // going to hold the list of created functions

	SQLITE_EXTENSION_INIT2(api);


	mainHeap = createDukHeap();
	if (mainHeap == NULL) return 0;
	duk_push_array(mainHeap);

	sqlite3_create_function_v2(db, "createjs", 2, SQLITE_UTF8, mainHeap, sql_createJS,
		NULL, NULL, NULL);
	sqlite3_create_function_v2(db, "createjs", 4, SQLITE_UTF8, mainHeap, sql_createJS,
		NULL, NULL, destroyDukHeap);

	sqlite3_create_function_v2(db, "loadfile", 1, SQLITE_UTF8, NULL, sql_loadFile,
		NULL, NULL, NULL);
	sqlite3_create_function_v2(db, "loadfile", 2, SQLITE_UTF8, NULL, sql_loadFile,
		NULL, NULL, NULL);


	return SQLITE_OK;
}
