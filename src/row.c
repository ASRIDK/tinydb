#include "row.h"

#include <stdio.h>
#include <string.h>

void serialize_row(const Row *source, void *destination) {
    char *dest = destination;
    memcpy(dest + ID_OFFSET, &source->id, ID_SIZE);
    memcpy(dest + USERNAME_OFFSET, source->username, USERNAME_SIZE);
    memcpy(dest + EMAIL_OFFSET, source->email, EMAIL_SIZE);
}

void deserialize_row(const void *source, Row *destination) {
    const char *src = source;
    memcpy(&destination->id, src + ID_OFFSET, ID_SIZE);
    memcpy(destination->username, src + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(destination->email, src + EMAIL_OFFSET, EMAIL_SIZE);
}

void print_row(const Row *row) {
    printf("(%u, %s, %s)\n", row->id, row->username, row->email);
}
