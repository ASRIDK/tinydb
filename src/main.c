#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "statement.h"
#include "table.h"
#include "vm.h"

static void print_prompt(void) {
    printf("db > ");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <database-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    Table *table = db_open(argv[1]);
    InputBuffer *input = input_buffer_new();
    if (input == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return EXIT_FAILURE;
    }

    for (;;) {
        print_prompt();
        if (read_input(input) != 0) {
            fprintf(stderr, "Error reading input.\n");
            break;
        }

        if (input->buffer[0] == '.') {
            MetaCommandResult result = do_meta_command(input, table);
            if (result == META_COMMAND_EXIT) {
                input_buffer_free(input);
                db_close(table);
                return EXIT_SUCCESS;
            }
            if (result == META_COMMAND_UNRECOGNIZED_COMMAND) {
                printf("Unrecognized command '%s'.\n", input->buffer);
            }
            continue;
        }

        Statement statement;
        switch (prepare_statement(input, &statement)) {
        case PREPARE_SUCCESS:
            break;
        case PREPARE_NEGATIVE_ID:
            printf("ID must be positive.\n");
            continue;
        case PREPARE_STRING_TOO_LONG:
            printf("String is too long.\n");
            continue;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized keyword at start of '%s'.\n", input->buffer);
            continue;
        }

        switch (execute_statement(&statement, table)) {
        case EXECUTE_SUCCESS:
            printf("Executed.\n");
            break;
        case EXECUTE_DUPLICATE_KEY:
            printf("Error: Duplicate key.\n");
            break;
        case EXECUTE_TABLE_FULL:
            printf("Error: Table full.\n");
            break;
        }
    }

    input_buffer_free(input);
    db_close(table);
    return EXIT_FAILURE;
}
