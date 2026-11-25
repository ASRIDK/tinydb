#ifndef TINYDB_STATEMENT_H
#define TINYDB_STATEMENT_H

#include "input.h"
#include "row.h"

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT, STATEMENT_COUNT } StatementType;

typedef struct {
    StatementType type;
    Row row_to_insert;   /* only used by insert statements */
    int select_by_id;    /* nonzero if a select is restricted to one id */
    uint32_t select_id;  /* the id to look up when select_by_id is set */
} Statement;

/* Compiles a line of SQL into a Statement. Tokenizes the input buffer in place. */
PrepareResult prepare_statement(InputBuffer *input, Statement *statement);

#endif
