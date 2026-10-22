"""End-to-end tests that drive the tinydb REPL through stdin."""

import os
import random
import subprocess
import tempfile
import unittest
from pathlib import Path

BINARY = Path(__file__).resolve().parent.parent / "build" / "tinydb"


def parse_tree(lines):
    """Parses `.btree` output into nested dicts: {"type", "keys", "children"}."""
    root = None
    stack = []  # (depth, node)
    for line in lines:
        stripped = line.lstrip(" ")
        depth = (len(line) - len(stripped)) // 2
        text = stripped.removeprefix("- ")
        while stack and stack[-1][0] >= depth:
            stack.pop()
        parent = stack[-1][1] if stack else None
        if text.startswith(("internal", "leaf")):
            node = {"type": text.split()[0], "keys": [], "children": []}
            if parent is None:
                root = node
            else:
                parent["children"].append(node)
            stack.append((depth, node))
        elif text.startswith("key "):
            parent["keys"].append(int(text[4:]))
        else:
            parent["keys"].append(int(text))
    return root


def check_btree(node, depth=0, leaf_depths=None):
    """Asserts B-tree invariants and returns (min_key, max_key) of the subtree."""
    if leaf_depths is None:
        leaf_depths = set()
    keys = node["keys"]
    assert keys == sorted(keys) and len(set(keys)) == len(keys), f"unsorted keys {keys}"
    if node["type"] == "leaf":
        leaf_depths.add(depth)
        assert len(leaf_depths) == 1, "leaves at different depths"
        return keys[0], keys[-1]

    assert len(node["children"]) == len(keys) + 1, "child/key count mismatch"
    lowest = highest = None
    for i, child in enumerate(node["children"]):
        low, high = check_btree(child, depth + 1, leaf_depths)
        if i < len(keys):
            assert high == keys[i], f"separator {keys[i]} != child max {high}"
        if i > 0:
            assert low > keys[i - 1], "child overlaps left sibling"
        lowest = low if lowest is None else lowest
        highest = high
    return lowest, highest


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

    def test_survives_many_random_inserts_and_reopen(self):
        ids = list(range(1, 501))
        random.Random(7).shuffle(ids)
        commands = [f"insert {i} user{i} person{i}@example.com" for i in ids]
        output = self.run_script(commands + [".exit"])
        self.assertEqual(output.count("db > Executed."), 500)
        self.assertNotIn("Error", "\n".join(output))

        output = self.run_script(["select", ".exit"])
        rows = [line.removeprefix("db > ") for line in output[:500]]
        self.assertEqual(rows, [f"({i}, user{i}, person{i}@example.com)" for i in range(1, 501)])

    def test_prints_error_when_table_is_full(self):
        commands = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 2001)]
        output = self.run_script(commands + [".exit"])
        joined = "\n".join(output)
        self.assertIn("Error: Table full.", joined)

        inserted = joined.count("Executed.")
        output = self.run_script(["select", ".exit"])
        self.assertEqual(len(output) - 2, inserted)  # every accepted row is persisted

    def test_tree_invariants_hold_after_internal_splits(self):
        for seed in range(5):
            with self.subTest(seed=seed):
                if os.path.exists(self.db_path):
                    os.remove(self.db_path)
                ids = list(range(1, 301))
                random.Random(seed).shuffle(ids)
                commands = [f"insert {i} user{i} person{i}@example.com" for i in ids]
                self.run_script(commands + [".exit"])

                output = self.run_script([".btree", ".exit"])
                tree_lines = [output[0].removeprefix("db > Tree:")] + output[1:-1]
                tree = parse_tree([line for line in tree_lines if line])
                self.assertEqual(tree["type"], "internal")
                self.assertEqual(check_btree(tree), (1, 300))


if __name__ == "__main__":
    unittest.main()
