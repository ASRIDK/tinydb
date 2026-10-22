#ifndef TINYDB_CURSOR_H
#define TINYDB_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "table.h"

/* A position within the B-tree: a cell inside a leaf page. */
typedef struct {
    Table *table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table; /* one position past the last row */
} Cursor;

Cursor *table_start(Table *table);
Cursor *table_end(Table *table);

/* Returns the serialized row under the cursor. */
void *cursor_value(Cursor *cursor);
void cursor_advance(Cursor *cursor);

#endif
