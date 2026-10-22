#ifndef TINYDB_TABLE_H
#define TINYDB_TABLE_H

#include <stdint.h>

#include "pager.h"
#include "row.h"

/* A table is a B-tree of rows keyed by id, rooted at `root_page_num`. */
typedef struct {
    Pager *pager;
    uint32_t root_page_num;
} Table;

/* Opens the database file, initializing an empty root leaf for new files. */
Table *db_open(const char *filename);

/* Flushes every cached page to disk and releases all resources. */
void db_close(Table *table);

#endif
