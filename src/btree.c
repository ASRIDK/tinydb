#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Nodes are raw pages, so fields may be unaligned: always go through memcpy. */
static uint32_t read_u32(const void *node, size_t offset) {
    uint32_t value;
    memcpy(&value, (const char *)node + offset, sizeof(value));
    return value;
}

static void write_u32(void *node, size_t offset, uint32_t value) {
    memcpy((char *)node + offset, &value, sizeof(value));
}

NodeType get_node_type(const void *node) {
    uint8_t value = *((const uint8_t *)node + NODE_TYPE_OFFSET);
    return (NodeType)value;
}

void set_node_type(void *node, NodeType type) {
    *((uint8_t *)node + NODE_TYPE_OFFSET) = (uint8_t)type;
}

uint32_t leaf_node_num_cells(const void *node) {
    return read_u32(node, LEAF_NODE_NUM_CELLS_OFFSET);
}

void set_leaf_node_num_cells(void *node, uint32_t num_cells) {
    write_u32(node, LEAF_NODE_NUM_CELLS_OFFSET, num_cells);
}

void *leaf_node_cell(void *node, uint32_t cell_num) {
    return (char *)node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t leaf_node_key(void *node, uint32_t cell_num) {
    return read_u32(leaf_node_cell(node, cell_num), LEAF_NODE_KEY_OFFSET);
}

void set_leaf_node_key(void *node, uint32_t cell_num, uint32_t key) {
    write_u32(leaf_node_cell(node, cell_num), LEAF_NODE_KEY_OFFSET, key);
}

void *leaf_node_value(void *node, uint32_t cell_num) {
    return (char *)leaf_node_cell(node, cell_num) + LEAF_NODE_VALUE_OFFSET;
}

void initialize_leaf_node(void *node) {
    set_node_type(node, NODE_LEAF);
    set_leaf_node_num_cells(node, 0);
}

void leaf_node_insert(Cursor *cursor, uint32_t key, const Row *row) {
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        fprintf(stderr, "Error: leaf node splitting is not implemented.\n");
        exit(EXIT_FAILURE);
    }

    if (cursor->cell_num < num_cells) {
        /* Make room for the new cell. */
        memmove(leaf_node_cell(node, cursor->cell_num + 1), leaf_node_cell(node, cursor->cell_num),
                (size_t)(num_cells - cursor->cell_num) * LEAF_NODE_CELL_SIZE);
    }

    set_leaf_node_num_cells(node, num_cells + 1);
    set_leaf_node_key(node, cursor->cell_num, key);
    serialize_row(row, leaf_node_value(node, cursor->cell_num));
}

void print_constants(void) {
    printf("ROW_SIZE: %zu\n", (size_t)ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %zu\n", (size_t)COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %zu\n", (size_t)LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %zu\n", (size_t)LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %zu\n", (size_t)LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %zu\n", (size_t)LEAF_NODE_MAX_CELLS);
}

void print_leaf_node(void *node) {
    uint32_t num_cells = leaf_node_num_cells(node);
    printf("leaf (size %u)\n", num_cells);
    for (uint32_t i = 0; i < num_cells; i++) {
        printf("  - %u : %u\n", i, leaf_node_key(node, i));
    }
}
