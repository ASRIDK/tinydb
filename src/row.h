#ifndef TINYDB_ROW_H
#define TINYDB_ROW_H

#include <stdint.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

/* The single hard-coded table schema: (id INTEGER, username VARCHAR(32), email VARCHAR(255)). */
typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

#define SIZE_OF_ATTRIBUTE(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

#define ID_SIZE SIZE_OF_ATTRIBUTE(Row, id)
#define USERNAME_SIZE SIZE_OF_ATTRIBUTE(Row, username)
#define EMAIL_SIZE SIZE_OF_ATTRIBUTE(Row, email)
#define ID_OFFSET 0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

/* Compact on-disk encoding of a row. */
void serialize_row(const Row *source, void *destination);
void deserialize_row(const void *source, Row *destination);
void print_row(const Row *row);

#endif
