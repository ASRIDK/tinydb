#ifndef TINYDB_BTREE_H
#define TINYDB_BTREE_H

#include <stdbool.h>
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
 * Internal node layout
 *   | common header | num_keys (4) | right_child (4) | cell 0 | cell 1 | ... |
 *   where each cell is | child page (4) | key (4) | and key i is the max key of child i.
 */
#define INTERNAL_NODE_NUM_KEYS_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_NUM_KEYS_OFFSET COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE                                                                  \
    (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

#define INTERNAL_NODE_KEY_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)

/*
 * Leaf node layout
 *   | common header | num_cells (4) | next_leaf (4) | cell 0 | cell 1 | ... |
 *   where each cell is | key (4) | serialized row (ROW_SIZE) |
 *   next_leaf is the page of the right sibling, or 0 for the rightmost leaf
 *   (page 0 is always the root, so it can never be a sibling).
 */
#define LEAF_NODE_NUM_CELLS_SIZE sizeof(uint32_t)
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_NEXT_LEAF_SIZE sizeof(uint32_t)
#define LEAF_NODE_NEXT_LEAF_OFFSET (LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_HEADER_SIZE                                                                      \
    (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE)

#define LEAF_NODE_KEY_SIZE sizeof(uint32_t)
#define LEAF_NODE_KEY_OFFSET 0
#define LEAF_NODE_VALUE_SIZE ROW_SIZE
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

/* When a full leaf splits, its MAX_CELLS + 1 cells are divided evenly between two leaves. */
#define LEAF_NODE_RIGHT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) / 2)
#define LEAF_NODE_LEFT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT)

/* Common header accessors. */
NodeType get_node_type(const void *node);
void set_node_type(void *node, NodeType type);
bool is_node_root(const void *node);
void set_node_root(void *node, bool is_root);

/* Internal node accessors. */
uint32_t internal_node_num_keys(const void *node);
void set_internal_node_num_keys(void *node, uint32_t num_keys);
uint32_t internal_node_right_child(const void *node);
void set_internal_node_right_child(void *node, uint32_t page_num);
uint32_t internal_node_child(void *node, uint32_t child_num);
void set_internal_node_child(void *node, uint32_t child_num, uint32_t page_num);
uint32_t internal_node_key(void *node, uint32_t key_num);
void set_internal_node_key(void *node, uint32_t key_num, uint32_t key);
void initialize_internal_node(void *node);

/* Index of the child that should contain `key`. */
uint32_t internal_node_find_child(void *node, uint32_t key);

/* Leaf node accessors. */
uint32_t leaf_node_num_cells(const void *node);
void set_leaf_node_num_cells(void *node, uint32_t num_cells);
uint32_t leaf_node_next_leaf(const void *node);
void set_leaf_node_next_leaf(void *node, uint32_t page_num);
void *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t leaf_node_key(void *node, uint32_t cell_num);
void set_leaf_node_key(void *node, uint32_t cell_num, uint32_t key);
void *leaf_node_value(void *node, uint32_t cell_num);
void initialize_leaf_node(void *node);

/* Binary search: index of `key`, or the index where it would be inserted. */
uint32_t leaf_node_find(void *node, uint32_t key);

/* Inserts (key, row) at the cursor position, splitting the leaf if it is full. */
void leaf_node_insert(Cursor *cursor, uint32_t key, const Row *row);

/* Largest key stored in the subtree rooted at `node`. */
uint32_t get_node_max_key(Pager *pager, void *node);

/* Debug output for the .constants and .btree meta-commands. */
void print_constants(void);
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level);

#endif
