"""End-to-end tests that drive the tinydb REPL through stdin."""

import os
import random
import subprocess
import tempfile
import unittest
from pathlib import Path

BINARY = Path(__file__).resolve().parent.parent / "build" / "tinydb"


class TestTinyDB(unittest.TestCase):
    def setUp(self):
        fd, self.db_path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        os.remove(self.db_path)

    def tearDown(self):
        if os.path.exists(self.db_path):
            os.remove(self.db_path)

    def run_script(self, commands):
        """Feeds `commands` to a fresh process and returns its stdout split into lines."""
        script = "\n".join(commands) + "\n"
        result = subprocess.run(
            [str(BINARY), self.db_path], input=script, capture_output=True, text=True, timeout=30
        )
        return result.stdout.split("\n")

    def test_inserts_and_retrieves_a_row(self):
        output = self.run_script(["insert 1 user1 person1@example.com", "select", ".exit"])
        self.assertEqual(
            output,
            ["db > Executed.", "db > (1, user1, person1@example.com)", "Executed.", "db > "],
        )

    def test_keeps_data_after_closing_connection(self):
        output = self.run_script(["insert 1 user1 person1@example.com", ".exit"])
        self.assertEqual(output, ["db > Executed.", "db > "])
        output = self.run_script(["select", ".exit"])
        self.assertEqual(output, ["db > (1, user1, person1@example.com)", "Executed.", "db > "])

    def test_rejects_duplicate_id(self):
        output = self.run_script(
            [
                "insert 1 user1 person1@example.com",
                "insert 1 user1 person1@example.com",
                "select",
                ".exit",
            ]
        )
        self.assertEqual(
            output,
            [
                "db > Executed.",
                "db > Error: Duplicate key.",
                "db > (1, user1, person1@example.com)",
                "Executed.",
                "db > ",
            ],
        )

    def test_prints_constants(self):
        output = self.run_script([".constants", ".exit"])
        self.assertEqual(
            output,
            [
                "db > Constants:",
                "ROW_SIZE: 293",
                "COMMON_NODE_HEADER_SIZE: 6",
                "LEAF_NODE_HEADER_SIZE: 14",
                "LEAF_NODE_CELL_SIZE: 297",
                "LEAF_NODE_SPACE_FOR_CELLS: 4082",
                "LEAF_NODE_MAX_CELLS: 13",
                "db > ",
            ],
        )

    def test_prints_structure_of_one_node_btree(self):
        commands = [f"insert {i} user{i} person{i}@example.com" for i in (3, 1, 2)]
        commands += [".btree", ".exit"]
        output = self.run_script(commands)
        self.assertEqual(
            output,
            [
                "db > Executed.",
                "db > Executed.",
                "db > Executed.",
                "db > Tree:",
                "- leaf (size 3)",
                "  - 1",
                "  - 2",
                "  - 3",
                "db > ",
            ],
        )

    def test_allows_strings_of_maximum_length(self):
        username = "a" * 32
        email = "a" * 255
        output = self.run_script([f"insert 1 {username} {email}", "select", ".exit"])
        self.assertEqual(
            output, ["db > Executed.", f"db > (1, {username}, {email})", "Executed.", "db > "]
        )

    def test_rejects_strings_that_are_too_long(self):
        username = "a" * 33
        email = "a" * 256
        output = self.run_script([f"insert 1 {username} {email}", "select", ".exit"])
        self.assertEqual(output, ["db > String is too long.", "db > Executed.", "db > "])

    def test_rejects_negative_id(self):
        output = self.run_script(["insert -1 cstack foo@bar.com", "select", ".exit"])
        self.assertEqual(output, ["db > ID must be positive.", "db > Executed.", "db > "])

    def test_rejects_malformed_insert(self):
        output = self.run_script(["insert 1 onlyuser", "insert abc user email", ".exit"])
        self.assertEqual(
            output,
            [
                "db > Syntax error. Could not parse statement.",
                "db > Syntax error. Could not parse statement.",
                "db > ",
            ],
        )

    def test_prints_structure_of_three_leaf_node_btree(self):
        commands = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 15)]
        commands += [".btree", "insert 15 user15 person15@example.com", ".exit"]
        output = self.run_script(commands)
        self.assertEqual(
            output[14:],
            [
                "db > Tree:",
                "- internal (size 1)",
                "  - leaf (size 7)",
                *[f"    - {i}" for i in range(1, 8)],
                "  - key 7",
                "  - leaf (size 7)",
                *[f"    - {i}" for i in range(8, 15)],
                "db > Executed.",
                "db > ",
            ],
        )

    def test_prints_all_rows_in_a_multi_level_tree(self):
        commands = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 16)]
        commands += ["select", ".exit"]
        output = self.run_script(commands)
        expected_rows = ["(1, user1, person1@example.com)"]
        expected_rows += [f"({i}, user{i}, person{i}@example.com)" for i in range(2, 16)]
        self.assertEqual(
            output[15:], ["db > " + expected_rows[0], *expected_rows[1:], "Executed.", "db > "]
        )

    def test_returns_rows_sorted_after_random_inserts(self):
        ids = list(range(1, 31))
        random.Random(42).shuffle(ids)
        commands = [f"insert {i} user{i} person{i}@example.com" for i in ids]
        commands += ["select", ".exit"]
        output = self.run_script(commands)
        rows = [line.removeprefix("db > ") for line in output[len(ids) : len(ids) + 30]]
        self.assertEqual(rows, [f"({i}, user{i}, person{i}@example.com)" for i in range(1, 31)])

    def test_prints_structure_of_four_leaf_node_btree(self):
        commands = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 31)]
        commands += [".btree", ".exit"]
        output = self.run_script(commands)
        expected = ["db > Tree:", "- internal (size 3)"]
        for low, high, separator in ((1, 7, 7), (8, 14, 14), (15, 21, 21), (22, 30, None)):
            expected.append(f"  - leaf (size {high - low + 1})")
            expected += [f"    - {i}" for i in range(low, high + 1)]
            if separator is not None:
                expected.append(f"  - key {separator}")
        expected.append("db > ")
        self.assertEqual(output[30:], expected)


if __name__ == "__main__":
    unittest.main()
