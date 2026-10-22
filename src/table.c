#include "table.h"

#include <stdio.h>
#include <stdlib.h>

Table *db_open(const char *filename) {
    Pager *pager = pager_open(filename);
    Table *table = malloc(sizeof(Table));
    if (table == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    table->pager = pager;
    table->num_rows = pager->file_length / ROW_SIZE;
    return table;
}

void db_close(Table *table) {
    Pager *pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;

    for (uint32_t i = 0; i < num_full_pages; i++) {
        if (pager->pages[i] != NULL) {
            pager_flush(pager, i, PAGE_SIZE);
        }
    }

    /* The last page may be partially filled; only write the used bytes. */
    uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
    if (num_additional_rows > 0 && pager->pages[num_full_pages] != NULL) {
        pager_flush(pager, num_full_pages, num_additional_rows * ROW_SIZE);
    }

    pager_close(pager);
    free(table);
}

void *row_slot(Table *table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    char *page = get_page(table->pager, page_num);
    uint32_t byte_offset = (row_num % ROWS_PER_PAGE) * ROW_SIZE;
    return page + byte_offset;
}
