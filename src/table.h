#ifndef TINYDB_TABLE_H
#define TINYDB_TABLE_H

#include <stdint.h>

#include "pager.h"
#include "row.h"

#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROWS_PER_PAGE * TABLE_MAX_PAGES)

typedef struct {
    Pager *pager;
    uint32_t num_rows;
} Table;

/* Opens the database file and loads the table metadata. */
Table *db_open(const char *filename);

/* Flushes every cached page to disk and releases all resources. */
void db_close(Table *table);

/* Returns the memory slot where row `row_num` lives. */
void *row_slot(Table *table, uint32_t row_num);

#endif
