#include "vm.h"

#include <stdlib.h>
#include <string.h>

#include "cursor.h"

MetaCommandResult do_meta_command(const InputBuffer *input) {
    if (strcmp(input->buffer, ".exit") == 0) {
        return META_COMMAND_EXIT;
    }
    return META_COMMAND_UNRECOGNIZED_COMMAND;
}

static ExecuteResult execute_insert(const Statement *statement, Table *table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }
    Cursor *cursor = table_end(table);
    serialize_row(&statement->row_to_insert, cursor_value(cursor));
    table->num_rows++;
    free(cursor);
    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_select(Table *table) {
    Cursor *cursor = table_start(table);
    Row row;
    while (!cursor->end_of_table) {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(const Statement *statement, Table *table) {
    switch (statement->type) {
    case STATEMENT_INSERT:
        return execute_insert(statement, table);
    case STATEMENT_SELECT:
        return execute_select(table);
    }
    return EXECUTE_SUCCESS;
}
