#include "pager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void fatal(const char *message) {
    fprintf(stderr, "Error: %s (%s)\n", message, strerror(errno));
    exit(EXIT_FAILURE);
}

Pager *pager_open(const char *filename) {
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        fatal("unable to open database file");
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1) {
        fatal("unable to seek database file");
    }

    Pager *pager = calloc(1, sizeof(Pager));
    if (pager == NULL) {
        fatal("out of memory");
    }
    pager->file_descriptor = fd;
    pager->file_length = (uint32_t)file_length;
    pager->num_pages = (uint32_t)(file_length / PAGE_SIZE);

    if (file_length % PAGE_SIZE != 0) {
        fprintf(stderr, "Error: database file is not a whole number of pages. Corrupt file.\n");
        exit(EXIT_FAILURE);
    }
    return pager;
}

void *get_page(Pager *pager, uint32_t page_num) {
    if (page_num >= TABLE_MAX_PAGES) {
        fprintf(stderr, "Error: page number out of bounds: %u >= %d\n", page_num,
                TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        void *page = calloc(1, PAGE_SIZE);
        if (page == NULL) {
            fatal("out of memory");
        }

        uint32_t pages_on_disk = pager->file_length / PAGE_SIZE;
        if (page_num < pages_on_disk) {
            if (lseek(pager->file_descriptor, (off_t)page_num * PAGE_SIZE, SEEK_SET) == -1 ||
                read(pager->file_descriptor, page, PAGE_SIZE) == -1) {
                fatal("unable to read database file");
            }
        }
        pager->pages[page_num] = page;

        if (page_num >= pager->num_pages) {
            pager->num_pages = page_num + 1;
        }
    }
    return pager->pages[page_num];
}

uint32_t pager_unused_page_num(const Pager *pager) {
    return pager->num_pages;
}

void pager_flush(Pager *pager, uint32_t page_num) {
    if (pager->pages[page_num] == NULL) {
        fprintf(stderr, "Error: tried to flush a page that is not cached.\n");
        exit(EXIT_FAILURE);
    }
    if (lseek(pager->file_descriptor, (off_t)page_num * PAGE_SIZE, SEEK_SET) == -1) {
        fatal("unable to seek database file");
    }
    if (write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE) == -1) {
        fatal("unable to write database file");
    }
}

void pager_close(Pager *pager) {
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        free(pager->pages[i]);
    }
    if (close(pager->file_descriptor) == -1) {
        fatal("unable to close database file");
    }
    free(pager);
}
