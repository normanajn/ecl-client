#!/usr/bin/env python3
"""Tests for scripts/ecl-track against a local mock ECL server.

Usage: test_ecl_track.py PATH-TO-ecl-post   (or set ECL_POST and run under pytest)
"""

from __future__ import annotations

import http.server
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
import unittest
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
TRACK = ROOT / "scripts" / "ecl-track"
ECL_POST = os.environ.get("ECL_POST") or str(ROOT / "build" / "ecl-post")


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    posts: list = []
    fail = False
    next_id = 100

    def log_message(self, *_args: object) -> None:
        pass

    def do_POST(self) -> None:  # noqa: N802
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        if Handler.fail:
            reply, status = b"Service unavailable\n", 503
        else:
            Handler.next_id += 1
            Handler.posts.append(ET.fromstring(body))
            reply, status = f"Created {Handler.next_id}\n".encode(), 200
        self.send_response(status)
        self.send_header("Content-Length", str(len(reply)))
        self.end_headers()
        self.wfile.write(reply)


class EclTrackTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.url = f"http://127.0.0.1:{cls.server.server_port}/mock"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.shutdown()

    def setUp(self) -> None:
        Handler.posts, Handler.fail = [], False
        self.tmp = tempfile.TemporaryDirectory()
        # Isolate from the developer's real ~/.config/ecl-client/config and ECL_* vars.
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("ECL_")}
        self.env.update(HOME=self.tmp.name, XDG_CONFIG_HOME=self.tmp.name,
                        ECL_POST=ECL_POST, ECL_TRACK_DIR=self.tmp.name,
                        ECL_USERNAME="robot", ECL_PASSWORD="secret")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def run_track(self, *args: str, stdin: str = "") -> subprocess.CompletedProcess:
        return subprocess.run([sys.executable, str(TRACK), *args], input=stdin,
                              capture_output=True, text=True, env=self.env, check=False)

    def state(self, op: str) -> dict:
        return json.loads((pathlib.Path(self.tmp.name) / f"{op}.json").read_text())

    def test_full_operation_links_steps_to_start(self) -> None:
        r = self.run_track("start", "--op", "op1", "--title", "Upgrade", "--url", self.url,
                           "--category", "Sandbox", "--tag", "daq", "-t", "Plan")
        self.assertEqual(r.returncode, 0, r.stderr)
        parent = self.state("op1")["parent_id"]
        self.assertIsNotNone(parent)
        self.assertEqual(self.run_track("step", "--op", "op1", "--name", "Stop run").returncode, 0)
        r = self.run_track("step", "--op", "op1", "--name", "Flash", "--status", "fail",
                           "-T", "-", stdin="firmware log\n")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(self.run_track("end", "--op", "op1", "--status", "partial").returncode, 0)

        self.assertEqual(len(Handler.posts), 4)
        start, step1, step2, end = Handler.posts
        self.assertEqual(start.get("subject"), "[op1] START: Upgrade")
        self.assertEqual(start.get("category"), "Sandbox")
        self.assertEqual([t.get("name") for t in start.iter("tag")], ["daq"])
        for entry in (step1, step2, end):
            self.assertEqual(entry.get("category"), "Sandbox")
            self.assertEqual(entry.get("related"), str(parent))
        self.assertIsNone(start.get("related"))
        self.assertEqual(step2.get("subject"), "[op1] step 2: Flash -- FAIL")
        self.assertIn("firmware log", ET.tostring(step2, encoding="unicode"))
        self.assertIn("host:", ET.tostring(step1, encoding="unicode"))
        self.assertIn("PARTIAL", end.get("subject"))
        self.assertTrue(self.state("op1")["closed"])
        self.assertNotEqual(self.run_track("step", "--op", "op1", "--name", "late").returncode, 0)

    def test_failure_is_queued_then_flushed(self) -> None:
        self.run_track("start", "--op", "op2", "--title", "T", "--url", self.url, "-c", "Sandbox")
        Handler.fail = True
        r = self.run_track("step", "--op", "op2", "--name", "offline")
        self.assertEqual(r.returncode, 0)
        self.assertIn("queued", r.stderr)
        self.assertEqual(self.run_track("step", "--op", "op2", "--name", "x", "--strict").returncode, 1)
        self.assertEqual(len(self.state("op2")["pending"]), 2)
        self.assertEqual(self.run_track("flush", "--op", "op2").returncode, 1)
        Handler.fail = False
        self.assertEqual(self.run_track("flush", "--op", "op2").returncode, 0)
        state = self.state("op2")
        self.assertEqual(state["pending"], [])
        self.assertTrue(all(e["entry_id"] for e in state["entries"]))
        self.assertEqual(len(Handler.posts), 3)

    def test_dry_run_operation_posts_nothing(self) -> None:
        r = self.run_track("start", "--op", "op3", "--title", "T", "-c", "Sandbox", "--dry-run")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("<entry", r.stdout)
        r = self.run_track("step", "--op", "op3", "--name", "s")
        self.assertIn("step 1: s", r.stdout)
        self.assertEqual(self.run_track("end", "--op", "op3").returncode, 0)
        self.assertEqual(Handler.posts, [])

    def test_rejects_managed_and_secret_options(self) -> None:
        for bad in (["--password", "x"], ["--subject", "x"], ["--related", "5"]):
            r = self.run_track("start", "--op", "op4", "--title", "T", "-c", "S", *bad)
            self.assertEqual(r.returncode, 2, bad)
        r = self.run_track("start", "--op", "../evil", "--title", "T", "-c", "S")
        self.assertEqual(r.returncode, 2)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        ECL_POST = sys.argv.pop(1)
    unittest.main()
