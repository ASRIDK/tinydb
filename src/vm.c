#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "cursor.h"

MetaCommandResult do_meta_command(const InputBuffer *input, Table *table) {
    if (strcmp(input->buffer, ".exit") == 0) {
        return META_COMMAND_EXIT;
    }
    if (strcmp(input->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_tree(table->pager, table->root_page_num, 0);
        return META_COMMAND_SUCCESS;
    }
    if (strcmp(input->buffer, ".constants") == 0) {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    }
    return META_COMMAND_UNRECOGNIZED_COMMAND;
}

static ExecuteResult execute_insert(const Statement *statement, Table *table) {
    /* Worst case, an insert splits every level and grows a new root. */
    uint32_t pages_needed = tree_height(table->pager, table->root_page_num) + 1;
    if (table->pager->num_pages + pages_needed > TABLE_MAX_PAGES) {
        return EXECUTE_TABLE_FULL;
    }

    const Row *row = &statement->row_to_insert;
    Cursor *cursor = table_find(table, row->id);
    void *node = get_page(table->pager, cursor->page_num);

    if (cursor->cell_num < leaf_node_num_cells(node) &&
        leaf_node_key(node, cursor->cell_num) == row->id) {
        free(cursor);
        return EXECUTE_DUPLICATE_KEY;
    }

    leaf_node_insert(cursor, row->id, row);
    free(cursor);
    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_select(Table *table) {
    Cursor *cursor = table_start(table);
    Row row;
    while (!cursor->end_of_table) {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(const Statement *statement, Table *table) {
    switch (statement->type) {
    case STATEMENT_INSERT:
        return execute_insert(statement, table);
    case STATEMENT_SELECT:
        return execute_select(table);
    }
    return EXECUTE_SUCCESS;
}
