#include "cursor.h"

#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

static Cursor *cursor_new(Table *table, uint32_t page_num, uint32_t cell_num) {
    Cursor *cursor = malloc(sizeof(Cursor));
    if (cursor == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    void *node = get_page(table->pager, page_num);
    cursor->table = table;
    cursor->page_num = page_num;
    cursor->cell_num = cell_num;
    cursor->end_of_table = cell_num >= leaf_node_num_cells(node);
    return cursor;
}

Cursor *table_start(Table *table) {
    return cursor_new(table, table->root_page_num, 0);
}

Cursor *table_end(Table *table) {
    void *root_node = get_page(table->pager, table->root_page_num);
    return cursor_new(table, table->root_page_num, leaf_node_num_cells(root_node));
}

void *cursor_value(Cursor *cursor) {
    void *page = get_page(cursor->table->pager, cursor->page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor *cursor) {
    void *node = get_page(cursor->table->pager, cursor->page_num);
    cursor->cell_num++;
    if (cursor->cell_num >= leaf_node_num_cells(node)) {
        cursor->end_of_table = true;
    }
}
