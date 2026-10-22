#ifndef TINYDB_STATEMENT_H
#define TINYDB_STATEMENT_H

#include "input.h"

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef struct {
    StatementType type;
} Statement;

/* Compiles a line of SQL into a Statement. */
PrepareResult prepare_statement(const InputBuffer *input, Statement *statement);

#endif
