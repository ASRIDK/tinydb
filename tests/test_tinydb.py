"""End-to-end tests that drive the tinydb REPL through stdin."""

import os
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

    def test_prints_error_when_table_is_full(self):
        commands = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 1402)]
        commands.append(".exit")
        output = self.run_script(commands)
        self.assertEqual(output[-2], "db > Error: Table full.")

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


if __name__ == "__main__":
    unittest.main()
