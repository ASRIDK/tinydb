#ifndef TINYDB_PAGER_H
#define TINYDB_PAGER_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 100

/* Page cache sitting between the B-tree and the database file. */
typedef struct {
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[TABLE_MAX_PAGES];
} Pager;

/* Opens (or creates) the database file. Exits on I/O failure or corruption. */
Pager *pager_open(const char *filename);

/* Returns page `page_num`, loading it from disk on a cache miss. */
void *get_page(Pager *pager, uint32_t page_num);

/* Pages are appended at the end of the file; there is no free list yet. */
uint32_t pager_unused_page_num(const Pager *pager);

/* Writes a cached page back to disk. */
void pager_flush(Pager *pager, uint32_t page_num);

/* Releases all cached pages and closes the file (does not flush). */
void pager_close(Pager *pager);

#endif
