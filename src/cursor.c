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

Cursor *table_find(Table *table, uint32_t key) {
    uint32_t page_num = table->root_page_num;
    void *node = get_page(table->pager, page_num);

    while (get_node_type(node) == NODE_INTERNAL) {
        page_num = internal_node_child(node, internal_node_find_child(node, key));
        node = get_page(table->pager, page_num);
    }
    return cursor_new(table, page_num, leaf_node_find(node, key));
}

Cursor *table_start(Table *table) {
    /* The smallest possible key always lands on the leftmost leaf. */
    Cursor *cursor = table_find(table, 0);
    cursor->cell_num = 0;
    void *node = get_page(table->pager, cursor->page_num);
    cursor->end_of_table = leaf_node_num_cells(node) == 0;
    return cursor;
}

void *cursor_value(Cursor *cursor) {
    void *page = get_page(cursor->table->pager, cursor->page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor *cursor) {
    void *node = get_page(cursor->table->pager, cursor->page_num);
    cursor->cell_num++;
    if (cursor->cell_num >= leaf_node_num_cells(node)) {
        /* Hop to the right sibling, if there is one. */
        uint32_t next_page_num = leaf_node_next_leaf(node);
        if (next_page_num == 0) {
            cursor->end_of_table = true;
        } else {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }
}
