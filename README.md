# tinydb

[![CI](https://github.com/ASRIDK/tinydb/actions/workflows/ci.yml/badge.svg)](https://github.com/ASRIDK/tinydb/actions/workflows/ci.yml)
![Language](https://img.shields.io/badge/language-C11-blue)
![License](https://img.shields.io/badge/license-MIT-green)

**tinydb** is a single-file, disk-backed relational storage engine written in C11, modelled on the
architecture of SQLite. It implements a REPL front end, a minimal SQL compiler, a virtual machine,
a page cache and a persistent **B+ tree** — with no dependencies beyond the C standard library and
POSIX.

The project was built to understand, from first principles, how the storage layer of a database
works: how rows are laid out in fixed-size pages, how a B+ tree keeps them sorted and searchable
in `O(log n)`, and how nodes split as the data grows. These are the same mechanics that sit
underneath tick-data stores and order-history databases in trading systems.

---

## Table of contents

- [Features](#features)
- [Quick start](#quick-start)
- [Usage](#usage)
- [Architecture](#architecture)
- [On-disk format](#on-disk-format)
- [Testing](#testing)
- [Limitations](#limitations)
- [Project history](#project-history)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Features

- **Persistent B+ tree** keyed by `id`, with leaf splits, internal-node splits and root growth.
- **Sorted range scans** via sibling pointers between leaves.
- **`O(log n)` point lookups** using binary search inside every node.
- **Page cache** that loads 4 KiB pages lazily and flushes them on exit.
- **Input validation**: negative ids, oversized strings, malformed statements and duplicate keys
  are rejected with clear error messages.
- **Graceful capacity handling**: inserts that could overflow the page budget return
  `Error: Table full.` instead of corrupting the file.
- **Introspection commands** (`.btree`, `.constants`) to inspect the tree and the binary layout.
- **Memory-safe by construction**: node fields are accessed through `memcpy`, never through
  unaligned pointer casts; the test suite runs clean under ASan and UBSan.

## Quick start

Requirements: a C11 compiler (GCC or Clang), `make`, and Python 3.8+ for the test suite.

```bash
git clone https://github.com/ASRIDK/tinydb.git
cd tinydb
make            # builds ./build/tinydb
make test       # runs the end-to-end test suite
```

## Usage

```text
$ ./build/tinydb trades.db
db > insert 3 alice alice@example.com
Executed.
db > insert 1 bob bob@example.com
Executed.
db > insert 1 carol carol@example.com
Error: Duplicate key.
db > select
(1, bob, bob@example.com)
(3, alice, alice@example.com)
Executed.
db > .btree
Tree:
- leaf (size 2)
  - 1
  - 3
db > .exit
```

The table schema is fixed:

| Column     | Type            |
|------------|-----------------|
| `id`       | `uint32` (key)  |
| `username` | `VARCHAR(32)`   |
| `email`    | `VARCHAR(255)`  |

| Command      | Description                                   |
|--------------|-----------------------------------------------|
| `insert <id> <username> <email>` | Inserts a row.            |
| `select`     | Prints every row in key order.                |
| `.btree`     | Prints the structure of the B+ tree.          |
| `.constants` | Prints the binary layout constants.           |
| `.exit`      | Flushes all pages to disk and exits.          |

## Architecture

```text
            ┌──────────┐   ┌─────────────┐   ┌──────────┐
  stdin ──▶ │  REPL    │──▶│ SQL compiler│──▶│    VM    │
            │ main.c   │   │ statement.c │   │  vm.c    │
            │ input.c  │   └─────────────┘   └────┬─────┘
            └──────────┘                          │
                                                  ▼
                               ┌────────────────────────────────┐
                               │ Cursor (cursor.c)              │
                               │ table_find / table_start / next│
                               └───────────────┬────────────────┘
                                               ▼
                               ┌────────────────────────────────┐
                               │ B+ tree (btree.c)              │
                               │ node layout, search, splits    │
                               └───────────────┬────────────────┘
                                               ▼
                               ┌────────────────────────────────┐
                               │ Pager (pager.c)                │
                               │ 4 KiB page cache ↔ file        │
                               └────────────────────────────────┘
```

| Module          | Responsibility                                                       |
|-----------------|----------------------------------------------------------------------|
| `input.c`       | Reads a line of user input.                                          |
| `statement.c`   | Tokenizes and validates SQL into a `Statement`.                      |
| `vm.c`          | Executes statements and meta-commands.                               |
| `cursor.c`      | Locates and iterates over rows in the tree.                          |
| `btree.c`       | Node layout accessors, binary search, leaf/internal splits.          |
| `table.c`       | Opens and closes the database, owns the root page.                   |
| `pager.c`       | Caches pages in memory and performs all file I/O.                    |
| `row.c`         | Serializes rows to and from their compact binary form.               |

## On-disk format

The database file is a sequence of 4096-byte pages. Page 0 is always the root.

**Common header (6 bytes)**

| Offset | Size | Field          |
|--------|------|----------------|
| 0      | 1    | node type      |
| 1      | 1    | is root        |
| 2      | 4    | parent page    |

**Leaf node** — header of 14 bytes followed by up to 13 cells of 297 bytes
(`key: u32` + serialized row of 293 bytes).

| Offset | Size | Field                      |
|--------|------|----------------------------|
| 6      | 4    | number of cells            |
| 10     | 4    | next leaf page (0 = none)  |
| 14     | …    | cells                      |

**Internal node** — header of 14 bytes followed by `(child page: u32, key: u32)` cells, where key
`i` is the largest key in child `i`. The right-most child is stored in the header.

| Offset | Size | Field              |
|--------|------|--------------------|
| 6      | 4    | number of keys     |
| 10     | 4    | right child page   |
| 14     | …    | cells              |

The internal fan-out is intentionally capped at 3 keys (`INTERNAL_NODE_MAX_KEYS`) so that the test
suite can exercise multi-level splits with only a few hundred rows.

## Testing

The test suite (`tests/test_tinydb.py`) drives the compiled binary end to end through stdin and
covers parsing, validation, persistence across restarts, duplicate keys, tree shape after splits,
and capacity limits. A structural checker parses the output of `.btree` and asserts the B+ tree
invariants — sorted keys, separator keys equal to each child's maximum, and all leaves at the same
depth — after hundreds of randomly ordered inserts.

```bash
make test       # regular build
make sanitize   # rebuild with -fsanitize=address,undefined and run the suite
```

Continuous integration runs the suite on Ubuntu (GCC and Clang) and macOS (Clang), plus a
sanitizer job.

## Limitations

tinydb is an educational engine, not a production database. Notably it does not support:

- arbitrary schemas, `UPDATE`, `DELETE` or `WHERE` clauses;
- transactions, journaling or crash recovery — pages are only written on `.exit`;
- page reuse (there is no free list) or concurrent access.

## Project history

The engine was built incrementally; each commit corresponds to one stage and keeps the test
suite green:

1. REPL and meta-commands
2. SQL compiler front end and virtual machine
3. In-memory, append-only table with row serialization
4. End-to-end test suite
5. Persistence through a pager
6. Cursor abstraction
7. B+ tree leaf node format
8. Binary search and duplicate-key detection
9. Leaf splits, root creation and internal-node search
10. Sibling pointers for multi-level scans
11. Parent updates after a leaf split
12. Internal-node splits and graceful capacity handling

## Acknowledgements

The design follows the structure of the excellent tutorial
[Let's Build a Simple Database](https://cstack.github.io/db_tutorial/) by Connor Stack, listed in
[build-your-own-x](https://github.com/codecrafters-io/build-your-own-x). This implementation
reorganizes the code into modules, replaces unaligned pointer casts with `memcpy` accessors,
fixes parent-pointer handling during cascading splits, and adds graceful capacity checks and
invariant-based tests.

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for details.
