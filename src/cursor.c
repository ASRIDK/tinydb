#include "cursor.h"

#include <stdio.h>
#include <stdlib.h>

static Cursor *cursor_new(Table *table, uint32_t row_num) {
    Cursor *cursor = malloc(sizeof(Cursor));
    if (cursor == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    cursor->table = table;
    cursor->row_num = row_num;
    cursor->end_of_table = row_num >= table->num_rows;
    return cursor;
}

Cursor *table_start(Table *table) {
    return cursor_new(table, 0);
}

Cursor *table_end(Table *table) {
    return cursor_new(table, table->num_rows);
}

void *cursor_value(Cursor *cursor) {
    uint32_t page_num = cursor->row_num / ROWS_PER_PAGE;
    char *page = get_page(cursor->table->pager, page_num);
    uint32_t byte_offset = (cursor->row_num % ROWS_PER_PAGE) * ROW_SIZE;
    return page + byte_offset;
}

void cursor_advance(Cursor *cursor) {
    cursor->row_num++;
    if (cursor->row_num >= cursor->table->num_rows) {
        cursor->end_of_table = true;
    }
}
