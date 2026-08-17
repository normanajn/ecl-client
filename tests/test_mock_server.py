#!/usr/bin/env python3
"""End-to-end tests for the CLI using a local ECL-compatible HTTP server."""

from __future__ import annotations

import hashlib
import http.server
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
import urllib.parse
import xml.etree.ElementTree as ET


PASSWORD = "mock-secret"


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    final_request: dict[str, object] | None = None

    def log_message(self, *_args: object) -> None:
        pass

    def _body(self) -> bytes:
        return self.rfile.read(int(self.headers.get("Content-Length", "0")))

    def _send(self, status: int, body: bytes = b"", **headers: str) -> None:
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        for name, value in headers.items():
            self.send_header(name.replace("_", "-"), value)
        self.end_headers()
        if body:
            self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802
        body = self._body()
        parsed = urllib.parse.urlsplit(self.path)
        if parsed.path == "/redirect/E/xml_post":
            location = f"http://127.0.0.1:{self.server.server_port}/final/E/xml_post"
            if parsed.query:
                location += "?" + parsed.query
            self._send(303, Location=location)
            return
        if parsed.path == "/fail/E/xml_post":
            self._send(400, b"Authentication failed: test rejection\n")
            return
        if parsed.path != "/final/E/xml_post":
            self._send(404, b"not found")
            return

        query = parsed.query
        expected = hashlib.md5(query.encode() + b":" + PASSWORD.encode() + b":" + body).hexdigest()
        errors: list[str] = []
        if self.headers.get("X-User") != "robot":
            errors.append("wrong X-User")
        if self.headers.get("X-Signature-Method") != "md5":
            errors.append("wrong signature method")
        if self.headers.get("X-Signature") != expected:
            errors.append("wrong signature")
        try:
            root = ET.fromstring(body)
            if root.attrib.get("category") != "Sandbox":
                errors.append("wrong category")
            if root.attrib.get("subject") != "Mock post":
                errors.append("wrong subject")
            attachment = root.find("attachment")
            if attachment is None or attachment.text != "AAEC/w==":
                errors.append("wrong attachment")
        except ET.ParseError as error:
            errors.append(f"invalid XML: {error}")
        Handler.final_request = {"path": self.path, "body": body, "headers": dict(self.headers)}
        if errors:
            self._send(400, ("; ".join(errors) + "\n").encode())
        else:
            self._send(200, b"Created 12345\n", Content_Type="text/plain")

    def do_GET(self) -> None:  # noqa: N802
        self._send(405, b"redirect incorrectly changed POST to GET\n")


def run(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False, **kwargs)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_mock_server.py ECL_POST_PATH")
    executable = sys.argv[1]
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{server.server_port}"
    environment = os.environ.copy()
    environment["ECL_PASSWORD"] = PASSWORD
    try:
        with tempfile.TemporaryDirectory() as directory:
            attachment = pathlib.Path(directory) / "bytes.bin"
            attachment.write_bytes(b"\x00\x01\x02\xff")
            result = run(
                [
                    executable,
                    "--url", base + "/redirect",
                    "--username", "robot",
                    "--auth", "xml",
                    "--category", "Sandbox",
                    "--subject", "Mock post",
                    "--text", "body < & >",
                    "--tag", "automated",
                    "--field", "state=ready",
                    "--attachment", f"raw={attachment}",
                ],
                env=environment,
            )
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert "Created ECL entry 12345" in result.stdout
        assert "/final/E/xml_post?salt=" in result.stdout
        assert Handler.final_request is not None

        failure = run(
            [
                executable,
                "--url", base + "/fail",
                "--username", "robot",
                "--category", "Sandbox",
                "--text", "rejected",
            ],
            env=environment,
        )
        assert failure.returncode == 1, (failure.stdout, failure.stderr)
        assert "HTTP 400" in failure.stderr
        assert "Authentication failed: test rejection" in failure.stderr

        dry_run = run(
            [executable, "--dry-run", "--category", "A&B", "--text", "<safe>"],
            env={key: value for key, value in environment.items() if key != "ECL_PASSWORD"},
        )
        assert dry_run.returncode == 0, dry_run.stderr
        root = ET.fromstring(dry_run.stdout)
        assert root.attrib["category"] == "A&B"
        assert root.find("./form/field").text == "<safe>"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
    print("mock server tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
