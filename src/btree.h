#ifndef TINYDB_BTREE_H
#define TINYDB_BTREE_H

#include <stdint.h>

#include "cursor.h"
#include "pager.h"
#include "row.h"

typedef enum { NODE_INTERNAL, NODE_LEAF } NodeType;

/*
 * Common node header layout
 *   | node_type (1) | is_root (1) | parent_pointer (4) |
 */
#define NODE_TYPE_SIZE sizeof(uint8_t)
#define NODE_TYPE_OFFSET 0
#define IS_ROOT_SIZE sizeof(uint8_t)
#define IS_ROOT_OFFSET (NODE_TYPE_OFFSET + NODE_TYPE_SIZE)
#define PARENT_POINTER_SIZE sizeof(uint32_t)
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

/*
 * Leaf node layout
 *   | common header | num_cells (4) | cell 0 | cell 1 | ... |
 *   where each cell is | key (4) | serialized row (ROW_SIZE) |
 */
#define LEAF_NODE_NUM_CELLS_SIZE sizeof(uint32_t)
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE)

#define LEAF_NODE_KEY_SIZE sizeof(uint32_t)
#define LEAF_NODE_KEY_OFFSET 0
#define LEAF_NODE_VALUE_SIZE ROW_SIZE
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

/* Common header accessors. */
NodeType get_node_type(const void *node);
void set_node_type(void *node, NodeType type);

/* Leaf node accessors. */
uint32_t leaf_node_num_cells(const void *node);
void set_leaf_node_num_cells(void *node, uint32_t num_cells);
void *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t leaf_node_key(void *node, uint32_t cell_num);
void set_leaf_node_key(void *node, uint32_t cell_num, uint32_t key);
void *leaf_node_value(void *node, uint32_t cell_num);
void initialize_leaf_node(void *node);

/* Inserts (key, row) at the cursor position, shifting later cells right. */
void leaf_node_insert(Cursor *cursor, uint32_t key, const Row *row);

/* Debug output for the .constants and .btree meta-commands. */
void print_constants(void);
void print_leaf_node(void *node);

#endif
