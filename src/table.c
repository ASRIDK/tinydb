#include "table.h"

#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

Table *db_open(const char *filename) {
    Pager *pager = pager_open(filename);
    Table *table = malloc(sizeof(Table));
    if (table == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    table->pager = pager;
    table->root_page_num = 0;

    if (pager->num_pages == 0) {
        /* New database file: page 0 becomes an empty leaf node. */
        void *root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
    }
    return table;
}

void db_close(Table *table) {
    Pager *pager = table->pager;
    for (uint32_t i = 0; i < pager->num_pages; i++) {
        if (pager->pages[i] != NULL) {
            pager_flush(pager, i);
        }
    }
    pager_close(pager);
    free(table);
}
