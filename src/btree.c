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

static void internal_node_split_and_insert(Table *table, uint32_t parent_page_num,
                                           uint32_t child_page_num);
static void create_new_root(Table *table, uint32_t right_child_page_num);

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

uint32_t node_parent(const void *node) {
    return read_u32(node, PARENT_POINTER_OFFSET);
}

void set_node_parent(void *node, uint32_t page_num) {
    write_u32(node, PARENT_POINTER_OFFSET, page_num);
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

static void *internal_node_cell(void *node, uint32_t cell_num) {
    return (char *)node + internal_node_cell_offset(cell_num);
}

uint32_t internal_node_child(void *node, uint32_t child_num) {
    uint32_t num_keys = internal_node_num_keys(node);
    if (child_num > num_keys) {
        fprintf(stderr, "Error: tried to access child_num %u > num_keys %u\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    uint32_t page_num = child_num == num_keys ? internal_node_right_child(node)
                                              : read_u32(node, internal_node_cell_offset(child_num));
    if (page_num == INVALID_PAGE_NUM) {
        fprintf(stderr, "Error: tried to access child %u of node, but it is an invalid page\n",
                child_num);
        exit(EXIT_FAILURE);
    }
    return page_num;
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
    set_internal_node_right_child(node, INVALID_PAGE_NUM);
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

/* After a child's max key changes, rewrite the separator key that pointed at it. */
static void update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key) {
    uint32_t old_child_index = internal_node_find_child(node, old_key);
    if (old_child_index < internal_node_num_keys(node)) {
        set_internal_node_key(node, old_child_index, new_key);
    }
}

void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num) {
    Pager *pager = table->pager;
    void *parent = get_page(pager, parent_page_num);
    void *child = get_page(pager, child_page_num);
    uint32_t child_max_key = get_node_max_key(pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key);

    uint32_t original_num_keys = internal_node_num_keys(parent);
    if (original_num_keys >= INTERNAL_NODE_MAX_KEYS) {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = internal_node_right_child(parent);
    if (right_child_page_num == INVALID_PAGE_NUM) {
        /* An empty internal node: the first child becomes the right child. */
        set_internal_node_right_child(parent, child_page_num);
        return;
    }
    void *right_child = get_page(pager, right_child_page_num);

    set_internal_node_num_keys(parent, original_num_keys + 1);

    if (child_max_key > get_node_max_key(pager, right_child)) {
        /* The new child becomes the right child; the old one moves into the cell array. */
        set_internal_node_child(parent, original_num_keys, right_child_page_num);
        set_internal_node_key(parent, original_num_keys, get_node_max_key(pager, right_child));
        set_internal_node_right_child(parent, child_page_num);
    } else {
        /* Shift cells right to make room for the new one. */
        for (uint32_t i = original_num_keys; i > index; i--) {
            memcpy(internal_node_cell(parent, i), internal_node_cell(parent, i - 1),
                   INTERNAL_NODE_CELL_SIZE);
        }
        set_internal_node_child(parent, index, child_page_num);
        set_internal_node_key(parent, index, child_max_key);
    }
}

/*
 * Splits a full internal node: the upper half of its children move to a new
 * sibling, then `child_page_num` is inserted into whichever half it belongs to
 * and the new sibling is registered with the parent (splitting it in turn if
 * needed, up to and including the root).
 */
static void internal_node_split_and_insert(Table *table, uint32_t parent_page_num,
                                           uint32_t child_page_num) {
    Pager *pager = table->pager;
    uint32_t old_page_num = parent_page_num;
    void *old_node = get_page(pager, old_page_num);
    uint32_t old_max = get_node_max_key(pager, old_node);

    void *child = get_page(pager, child_page_num);
    uint32_t child_max = get_node_max_key(pager, child);

    uint32_t new_page_num = pager_unused_page_num(pager);
    bool splitting_root = is_node_root(old_node);

    void *parent;
    void *new_node = NULL;
    if (splitting_root) {
        /* create_new_root moves the old root's contents into a fresh left child. */
        create_new_root(table, new_page_num);
        parent = get_page(pager, table->root_page_num);
        old_page_num = internal_node_child(parent, 0);
        old_node = get_page(pager, old_page_num);
    } else {
        parent = get_page(pager, node_parent(old_node));
        new_node = get_page(pager, new_page_num);
        initialize_internal_node(new_node);
    }

    /* Move the right child, then every child above the middle, into the new node. */
    uint32_t cur_page_num = internal_node_right_child(old_node);
    internal_node_insert(table, new_page_num, cur_page_num);
    set_node_parent(get_page(pager, cur_page_num), new_page_num);
    set_internal_node_right_child(old_node, INVALID_PAGE_NUM);

    for (uint32_t i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--) {
        cur_page_num = internal_node_child(old_node, i);
        internal_node_insert(table, new_page_num, cur_page_num);
        set_node_parent(get_page(pager, cur_page_num), new_page_num);
        set_internal_node_num_keys(old_node, internal_node_num_keys(old_node) - 1);
    }

    /* The child left of the middle key becomes the old node's right child. */
    uint32_t old_num_keys = internal_node_num_keys(old_node);
    set_internal_node_right_child(old_node, internal_node_child(old_node, old_num_keys - 1));
    set_internal_node_num_keys(old_node, old_num_keys - 1);

    /* Insert the pending child into whichever half covers its key range. */
    uint32_t max_after_split = get_node_max_key(pager, old_node);
    uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;
    internal_node_insert(table, destination_page_num, child_page_num);
    set_node_parent(child, destination_page_num);

    update_internal_node_key(parent, old_max, get_node_max_key(pager, old_node));

    if (!splitting_root) {
        /* Set the parent first: if the parent splits, it will re-point new_node itself. */
        uint32_t grandparent_page_num = node_parent(old_node);
        set_node_parent(new_node, grandparent_page_num);
        internal_node_insert(table, grandparent_page_num, new_page_num);
    }
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

uint32_t tree_height(Pager *pager, uint32_t root_page_num) {
    uint32_t height = 1;
    void *node = get_page(pager, root_page_num);
    while (get_node_type(node) == NODE_INTERNAL) {
        node = get_page(pager, internal_node_child(node, 0));
        height++;
    }
    return height;
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
 * already-allocated `right_child_page_num` becomes the right child, and the
 * root page is reinitialized as an internal node pointing at both. The root
 * therefore always stays on page 0.
 */
static void create_new_root(Table *table, uint32_t right_child_page_num) {
    Pager *pager = table->pager;
    void *root = get_page(pager, table->root_page_num);
    void *right_child = get_page(pager, right_child_page_num);

    uint32_t left_child_page_num = pager_unused_page_num(pager);
    void *left_child = get_page(pager, left_child_page_num);

    if (get_node_type(root) == NODE_INTERNAL) {
        initialize_internal_node(right_child);
        initialize_internal_node(left_child);
    }

    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);

    if (get_node_type(left_child) == NODE_INTERNAL) {
        /* The root's children now live under the left child. */
        uint32_t num_keys = internal_node_num_keys(left_child);
        for (uint32_t i = 0; i <= num_keys; i++) {
            void *child = get_page(pager, internal_node_child(left_child, i));
            set_node_parent(child, left_child_page_num);
        }
    }

    initialize_internal_node(root);
    set_node_root(root, true);
    set_internal_node_num_keys(root, 1);
    set_internal_node_child(root, 0, left_child_page_num);
    set_internal_node_key(root, 0, get_node_max_key(pager, left_child));
    set_internal_node_right_child(root, right_child_page_num);

    set_node_parent(left_child, table->root_page_num);
    set_node_parent(right_child, table->root_page_num);
}

/*
 * Creates a new leaf and moves the upper half of the cells into it, inserting
 * the new cell into whichever half it belongs to.
 */
static void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, const Row *row) {
    Pager *pager = cursor->table->pager;
    void *old_node = get_page(pager, cursor->page_num);
    uint32_t old_max = get_node_max_key(pager, old_node);
    uint32_t new_page_num = pager_unused_page_num(pager);
    void *new_node = get_page(pager, new_page_num);
    initialize_leaf_node(new_node);
    set_node_parent(new_node, node_parent(old_node));

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
        return;
    }

    uint32_t parent_page_num = node_parent(old_node);
    uint32_t new_max = get_node_max_key(pager, old_node);
    void *parent = get_page(pager, parent_page_num);
    update_internal_node_key(parent, old_max, new_max);
    internal_node_insert(cursor->table, parent_page_num, new_page_num);
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
