"""Smoke tests for the compiled Python interface.

Run after installing the project, or through CTest in a build configured with
both ECL_BUILD_PYTHON and ECL_BUILD_TESTS.
"""

from __future__ import annotations

import importlib
import os
import unittest
import xml.etree.ElementTree as ET


# Prefer the freshly built module on PYTHONPATH (CTest) over any installed copy,
# so CTest never silently tests a stale ecl_client from site-packages.
try:
    ecl = importlib.import_module("_native")
except ImportError:
    ecl = importlib.import_module("ecl_client")


class PythonBindingTests(unittest.TestCase):
    def test_entry_builder_and_binary_data(self) -> None:
        entry = (
            ecl.Entry("Operations/DAQ")
            .subject("Python & XML")
            .text("one < two")
            .format(ecl.TextFormat.PLAIN)
            .tag("automated")
            .field("state", "ready")
            .attachment_data("raw", "bytes.bin", b"\x00\x01\x02\xff")
        )
        root = ET.fromstring(entry.xml())
        self.assertEqual(root.attrib["category"], "Operations/DAQ")
        self.assertEqual(root.attrib["subject"], "Python & XML")
        self.assertEqual(root.find("./form/field").text, "one < two")
        self.assertEqual(root.find("attachment").text, "AAEC/w==")

    def test_helpers(self) -> None:
        self.assertEqual(ecl.instance_url("mu2e"), "https://dbweb0.fnal.gov/ECL/mu2e")
        self.assertEqual(ecl.instance_url("https://example.test/ecl"), "https://example.test/ecl")
        self.assertEqual(ecl.__version__, "1.0.0")


if __name__ == "__main__":
    unittest.main()
