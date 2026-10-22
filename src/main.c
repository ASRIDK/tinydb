#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"

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

        if (strcmp(input->buffer, ".exit") == 0) {
            input_buffer_free(input);
            return EXIT_SUCCESS;
        }
        printf("Unrecognized command '%s'.\n", input->buffer);
    }
}
