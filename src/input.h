#ifndef TINYDB_INPUT_H
#define TINYDB_INPUT_H

#include <stddef.h>
#include <sys/types.h>

/* A line of user input read from stdin, without the trailing newline. */
typedef struct {
    char *buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

InputBuffer *input_buffer_new(void);
void input_buffer_free(InputBuffer *input);

/* Reads one line into the buffer. Returns 0 on success, -1 on EOF or error. */
int read_input(InputBuffer *input);

#endif
