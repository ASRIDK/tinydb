#include "table.h"

#include <stdlib.h>

Table *table_new(void) {
    Table *table = calloc(1, sizeof(Table));
    return table;
}

void table_free(Table *table) {
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        free(table->pages[i]);
    }
    free(table);
}

void *row_slot(Table *table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    if (table->pages[page_num] == NULL) {
        table->pages[page_num] = malloc(PAGE_SIZE);
        if (table->pages[page_num] == NULL) {
            return NULL;
        }
    }
    uint32_t byte_offset = (row_num % ROWS_PER_PAGE) * ROW_SIZE;
    return (char *)table->pages[page_num] + byte_offset;
}
