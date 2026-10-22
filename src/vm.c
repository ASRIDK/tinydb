#include "vm.h"

#include <stdio.h>
#include <string.h>

MetaCommandResult do_meta_command(const InputBuffer *input) {
    if (strcmp(input->buffer, ".exit") == 0) {
        return META_COMMAND_EXIT;
    }
    return META_COMMAND_UNRECOGNIZED_COMMAND;
}

ExecuteResult execute_statement(const Statement *statement) {
    switch (statement->type) {
    case STATEMENT_INSERT:
        printf("This is where we would do an insert.\n");
        break;
    case STATEMENT_SELECT:
        printf("This is where we would do a select.\n");
        break;
    }
    return EXECUTE_SUCCESS;
}
