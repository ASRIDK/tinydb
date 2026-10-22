#include "input.h"

#include <stdio.h>
#include <stdlib.h>

InputBuffer *input_buffer_new(void) {
    InputBuffer *input = malloc(sizeof(InputBuffer));
    if (input == NULL) {
        return NULL;
    }
    input->buffer = NULL;
    input->buffer_length = 0;
    input->input_length = 0;
    return input;
}

void input_buffer_free(InputBuffer *input) {
    free(input->buffer);
    free(input);
}

int read_input(InputBuffer *input) {
    ssize_t bytes_read = getline(&input->buffer, &input->buffer_length, stdin);
    if (bytes_read <= 0) {
        return -1;
    }

    /* Strip the trailing newline. */
    if (input->buffer[bytes_read - 1] == '\n') {
        input->buffer[--bytes_read] = '\0';
    }
    input->input_length = bytes_read;
    return 0;
}
