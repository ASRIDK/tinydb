#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "statement.h"
#include "vm.h"

static void print_prompt(void) {
    printf("db > ");
}

int main(void) {
    InputBuffer *input = input_buffer_new();
    if (input == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return EXIT_FAILURE;
    }

    for (;;) {
        print_prompt();
        if (read_input(input) != 0) {
            fprintf(stderr, "Error reading input.\n");
            input_buffer_free(input);
            return EXIT_FAILURE;
        }

        if (input->buffer[0] == '.') {
            switch (do_meta_command(input)) {
            case META_COMMAND_SUCCESS:
                continue;
            case META_COMMAND_EXIT:
                input_buffer_free(input);
                return EXIT_SUCCESS;
            case META_COMMAND_UNRECOGNIZED_COMMAND:
                printf("Unrecognized command '%s'.\n", input->buffer);
                continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input, &statement)) {
        case PREPARE_SUCCESS:
            break;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized keyword at start of '%s'.\n", input->buffer);
            continue;
        }

        execute_statement(&statement);
        printf("Executed.\n");
    }
}
