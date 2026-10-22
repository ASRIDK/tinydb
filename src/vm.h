#ifndef TINYDB_VM_H
#define TINYDB_VM_H

#include "input.h"
#include "statement.h"
#include "table.h"

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_EXIT,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum { EXECUTE_SUCCESS, EXECUTE_TABLE_FULL } ExecuteResult;

/* Handles dot-prefixed commands such as ".exit". */
MetaCommandResult do_meta_command(const InputBuffer *input);

/* Executes a compiled statement against the table. */
ExecuteResult execute_statement(const Statement *statement, Table *table);

#endif
