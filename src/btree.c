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

/* ---- Common header ---------------------------------------------------- */

NodeType get_node_type(const void *node) {
    uint8_t value = *((const uint8_t *)node + NODE_TYPE_OFFSET);
    return (NodeType)value;
}

void set_node_type(void *node, NodeType type) {
    *((uint8_t *)node + NODE_TYPE_OFFSET) = (uint8_t)type;
}

bool is_node_root(const void *node) {
    return *((const uint8_t *)node + IS_ROOT_OFFSET) != 0;
}

void set_node_root(void *node, bool is_root) {
    *((uint8_t *)node + IS_ROOT_OFFSET) = (uint8_t)is_root;
}

/* ---- Internal nodes --------------------------------------------------- */

uint32_t internal_node_num_keys(const void *node) {
    return read_u32(node, INTERNAL_NODE_NUM_KEYS_OFFSET);
}

void set_internal_node_num_keys(void *node, uint32_t num_keys) {
    write_u32(node, INTERNAL_NODE_NUM_KEYS_OFFSET, num_keys);
}

uint32_t internal_node_right_child(const void *node) {
    return read_u32(node, INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

void set_internal_node_right_child(void *node, uint32_t page_num) {
    write_u32(node, INTERNAL_NODE_RIGHT_CHILD_OFFSET, page_num);
}

static size_t internal_node_cell_offset(uint32_t cell_num) {
    return INTERNAL_NODE_HEADER_SIZE + (size_t)cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t internal_node_child(void *node, uint32_t child_num) {
    uint32_t num_keys = internal_node_num_keys(node);
    if (child_num > num_keys) {
        fprintf(stderr, "Error: tried to access child_num %u > num_keys %u\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    if (child_num == num_keys) {
        return internal_node_right_child(node);
    }
    return read_u32(node, internal_node_cell_offset(child_num));
}

void set_internal_node_child(void *node, uint32_t child_num, uint32_t page_num) {
    if (child_num == internal_node_num_keys(node)) {
        set_internal_node_right_child(node, page_num);
    } else {
        write_u32(node, internal_node_cell_offset(child_num), page_num);
    }
}

uint32_t internal_node_key(void *node, uint32_t key_num) {
    return read_u32(node, internal_node_cell_offset(key_num) + INTERNAL_NODE_CHILD_SIZE);
}

void set_internal_node_key(void *node, uint32_t key_num, uint32_t key) {
    write_u32(node, internal_node_cell_offset(key_num) + INTERNAL_NODE_CHILD_SIZE, key);
}

void initialize_internal_node(void *node) {
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    set_internal_node_num_keys(node, 0);
}

uint32_t internal_node_find_child(void *node, uint32_t key) {
    /* Binary search for the first key >= `key`; the right child holds everything larger. */
    uint32_t min_index = 0;
    uint32_t max_index = internal_node_num_keys(node);
    while (min_index != max_index) {
        uint32_t index = min_index + (max_index - min_index) / 2;
        if (internal_node_key(node, index) >= key) {
            max_index = index;
        } else {
            min_index = index + 1;
        }
    }
    return min_index;
}

/* ---- Leaf nodes ------------------------------------------------------- */

uint32_t leaf_node_num_cells(const void *node) {
    return read_u32(node, LEAF_NODE_NUM_CELLS_OFFSET);
}

void set_leaf_node_num_cells(void *node, uint32_t num_cells) {
    write_u32(node, LEAF_NODE_NUM_CELLS_OFFSET, num_cells);
}

uint32_t leaf_node_next_leaf(const void *node) {
    return read_u32(node, LEAF_NODE_NEXT_LEAF_OFFSET);
}

void set_leaf_node_next_leaf(void *node, uint32_t page_num) {
    write_u32(node, LEAF_NODE_NEXT_LEAF_OFFSET, page_num);
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
    set_node_root(node, false);
    set_leaf_node_num_cells(node, 0);
    set_leaf_node_next_leaf(node, 0);
}

uint32_t leaf_node_find(void *node, uint32_t key) {
    uint32_t min_index = 0;
    uint32_t one_past_max_index = leaf_node_num_cells(node);
    while (min_index != one_past_max_index) {
        uint32_t index = min_index + (one_past_max_index - min_index) / 2;
        uint32_t key_at_index = leaf_node_key(node, index);
        if (key == key_at_index) {
            return index;
        }
        if (key < key_at_index) {
            one_past_max_index = index;
        } else {
            min_index = index + 1;
        }
    }
    return min_index;
}

uint32_t get_node_max_key(Pager *pager, void *node) {
    if (get_node_type(node) == NODE_LEAF) {
        return leaf_node_key(node, leaf_node_num_cells(node) - 1);
    }
    void *right_child = get_page(pager, internal_node_right_child(node));
    return get_node_max_key(pager, right_child);
}

/* ---- Tree growth ------------------------------------------------------ */

/*
 * Splits the root. The old root's contents move to a new left child, the
 * already-populated `right_child_page_num` becomes the right child, and the
 * root page is reinitialized as an internal node pointing at both.
 */
static void create_new_root(Table *table, uint32_t right_child_page_num) {
    Pager *pager = table->pager;
    void *root = get_page(pager, table->root_page_num);
    void *right_child = get_page(pager, right_child_page_num);
    (void)right_child;

    uint32_t left_child_page_num = pager_unused_page_num(pager);
    void *left_child = get_page(pager, left_child_page_num);

    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);

    initialize_internal_node(root);
    set_node_root(root, true);
    set_internal_node_num_keys(root, 1);
    set_internal_node_child(root, 0, left_child_page_num);
    set_internal_node_key(root, 0, get_node_max_key(pager, left_child));
    set_internal_node_right_child(root, right_child_page_num);
}

/*
 * Creates a new leaf and moves the upper half of the cells into it, inserting
 * the new cell into whichever half it belongs to.
 */
static void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, const Row *row) {
    Pager *pager = cursor->table->pager;
    void *old_node = get_page(pager, cursor->page_num);
    uint32_t new_page_num = pager_unused_page_num(pager);
    void *new_node = get_page(pager, new_page_num);
    initialize_leaf_node(new_node);

    /* The new leaf slots in to the right of the old one in the sibling chain. */
    set_leaf_node_next_leaf(new_node, leaf_node_next_leaf(old_node));
    set_leaf_node_next_leaf(old_node, new_page_num);

    /* Walk from the top down so cells can be moved within old_node in place. */
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
        uint32_t index = (uint32_t)i;
        void *destination_node = index >= LEAF_NODE_LEFT_SPLIT_COUNT ? new_node : old_node;
        uint32_t index_within_node = index % LEAF_NODE_LEFT_SPLIT_COUNT;
        void *destination = leaf_node_cell(destination_node, index_within_node);

        if (index == cursor->cell_num) {
            set_leaf_node_key(destination_node, index_within_node, key);
            serialize_row(row, leaf_node_value(destination_node, index_within_node));
        } else if (index > cursor->cell_num) {
            memcpy(destination, leaf_node_cell(old_node, index - 1), LEAF_NODE_CELL_SIZE);
        } else {
            memcpy(destination, leaf_node_cell(old_node, index), LEAF_NODE_CELL_SIZE);
        }
    }

    set_leaf_node_num_cells(old_node, LEAF_NODE_LEFT_SPLIT_COUNT);
    set_leaf_node_num_cells(new_node, LEAF_NODE_RIGHT_SPLIT_COUNT);

    if (is_node_root(old_node)) {
        create_new_root(cursor->table, new_page_num);
    } else {
        fprintf(stderr, "Error: updating the parent after a split is not implemented.\n");
        exit(EXIT_FAILURE);
    }
}

void leaf_node_insert(Cursor *cursor, uint32_t key, const Row *row) {
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        leaf_node_split_and_insert(cursor, key, row);
        return;
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

/* ---- Debugging -------------------------------------------------------- */

void print_constants(void) {
    printf("ROW_SIZE: %zu\n", (size_t)ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %zu\n", (size_t)COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %zu\n", (size_t)LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %zu\n", (size_t)LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %zu\n", (size_t)LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %zu\n", (size_t)LEAF_NODE_MAX_CELLS);
}

static void indent(uint32_t level) {
    for (uint32_t i = 0; i < level; i++) {
        printf("  ");
    }
}

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level) {
    void *node = get_page(pager, page_num);

    switch (get_node_type(node)) {
    case NODE_LEAF: {
        uint32_t num_cells = leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %u)\n", num_cells);
        for (uint32_t i = 0; i < num_cells; i++) {
            indent(indentation_level + 1);
            printf("- %u\n", leaf_node_key(node, i));
        }
        break;
    }
    case NODE_INTERNAL: {
        uint32_t num_keys = internal_node_num_keys(node);
        indent(indentation_level);
        printf("- internal (size %u)\n", num_keys);
        if (num_keys > 0) {
            for (uint32_t i = 0; i < num_keys; i++) {
                print_tree(pager, internal_node_child(node, i), indentation_level + 1);
                indent(indentation_level + 1);
                printf("- key %u\n", internal_node_key(node, i));
            }
            print_tree(pager, internal_node_right_child(node), indentation_level + 1);
        }
        break;
    }
    }
}
