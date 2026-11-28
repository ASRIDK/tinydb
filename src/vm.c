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
    if (strcmp(input->buffer, ".help") == 0) {
        printf("Commands:\n");
        printf("  insert <id> <username> <email>  Insert a row\n");
        printf("  select [<id> [<high>]]          Print all rows, the row with that id, or ids in a range\n");
        printf("  count                           Print the number of rows\n");
        printf("  .btree                          Print the B+ tree structure\n");
        printf("  .constants                      Print the binary layout constants\n");
        printf("  .help                           Show this message\n");
        printf("  .exit                           Flush to disk and exit\n");
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

static ExecuteResult execute_select(const Statement *statement, Table *table) {
    if (statement->select_by_id) {
        Cursor *found = table_find(table, statement->select_id);
        void *node = get_page(table->pager, found->page_num);
        if (found->cell_num < leaf_node_num_cells(node) &&
            leaf_node_key(node, found->cell_num) == statement->select_id) {
            Row row;
            deserialize_row(cursor_value(found), &row);
            print_row(&row);
        }
        free(found);
        return EXECUTE_SUCCESS;
    }

    Cursor *cursor = statement->select_range ? table_find(table, statement->select_id)
                                             : table_start(table);
    if (statement->select_range) {
        /* The search can land one past the last cell of a leaf; hop to the next row. */
        void *node = get_page(table->pager, cursor->page_num);
        uint32_t cells = leaf_node_num_cells(node);
        if (cursor->cell_num >= cells) {
            if (cells == 0) {
                cursor->end_of_table = true;
            } else {
                cursor->cell_num = cells - 1;
                cursor_advance(cursor);
            }
        }
    }
    Row row;
    while (!cursor->end_of_table) {
        deserialize_row(cursor_value(cursor), &row);
        if (statement->select_range && row.id > statement->select_high) {
            break;
        }
        print_row(&row);
        cursor_advance(cursor);
    }
    free(cursor);
    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_count(Table *table) {
    Cursor *cursor = table_start(table);
    unsigned long count = 0;
    while (!cursor->end_of_table) {
        count++;
        cursor_advance(cursor);
    }
    free(cursor);
    printf("Count: %lu\n", count);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(const Statement *statement, Table *table) {
    switch (statement->type) {
    case STATEMENT_INSERT:
        return execute_insert(statement, table);
    case STATEMENT_SELECT:
        return execute_select(statement, table);
    case STATEMENT_COUNT:
        return execute_count(table);
    }
    return EXECUTE_SUCCESS;
}
