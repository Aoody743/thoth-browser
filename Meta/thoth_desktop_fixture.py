#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Launch a disposable profile for native desktop feature acceptance checks."""
import argparse
import http.server
import json
import os
import io
import math
import struct
import wave
from pathlib import Path
import subprocess
import tempfile
import threading
import time


class Fixture(http.server.BaseHTTPRequestHandler):
    download_prefix = ""
    download_names = []
    def do_GET(self):
        if self.path == "/tone.wav":
            buffer = io.BytesIO()
            with wave.open(buffer, "wb") as output:
                output.setnchannels(1); output.setsampwidth(2); output.setframerate(8000)
                output.writeframes(b"".join(struct.pack("<h", int(250 * math.sin(2 * math.pi * 220 * i / 8000))) for i in range(8000)))
            body = buffer.getvalue()
            self.send_response(200); self.send_header("Content-Type", "audio/wav")
            self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
            return
        download = self.path == "/download"
        body = b"thoth fixture download\n" if download else f"""<!doctype html>
<meta charset="utf-8"><title>Desktop QA {self.path}</title>
<main><h1>Desktop QA {self.path}</h1><p>Disposable local browser fixture.</p>
<label>Draft <input id="draft" autocomplete="off"></label>
<p><a href="/download">Download fixture</a></p>
<button onclick="localStorage.setItem('fixture','saved');document.cookie='fixture=saved;path=/';document.querySelector('#status').textContent='saved'">Set site data</button>
<p><button onclick="document.getElementById('tone').play()">Play quiet test audio</button>
<button onclick="document.getElementById('tone').pause()">Stop test audio</button>
<audio id="tone" loop src="/tone.wav"></audio></p><p id="status"></p></main>""".encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain" if download else "text/html; charset=utf-8")
        if download:
            name = f"{self.download_prefix}-{time.time_ns()}.txt"
            self.download_names.append(name)
            self.send_header("Content-Disposition", f'attachment; filename="{name}"')
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path, help="Thoth Browser.app bundle")
    parser.add_argument("--language", choices=["en", "zh-CN"], default="en")
    args = parser.parse_args()
    binary = args.app.resolve() / "Contents/MacOS/Thoth Browser"
    with tempfile.TemporaryDirectory(prefix="thoth-desktop-ui-") as root:
        root = Path(root)
        Fixture.download_prefix = f"thoth-desktop-qa-{os.getpid()}"
        Fixture.download_names = []
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{server.server_port}"
        pages = [{"id": name, "url": base + "/" + name, "title": "Desktop QA " + name} for name in ("one", "two", "three")]
        workspaces = [{"id": name, "name": title, "color": color, "symbol": "book", "active": active} for name, title, color, active in (("personal", "Personal", "#b9a5df", "item-one"), ("research", "Research", "#9fbbe0", "item-two"))]
        items = [{"id": "item-" + name, "pages": [name], "active": name, "workspace": workspace, "section": "temporary", "ratio": 0.5} for name, workspace in (("one", "personal"), ("two", "research"), ("three", "research"))]
        window = {"schema": 2, "workspaces": workspaces, "active_workspace": "personal", "pages": pages, "items": items, "active": "item-one", "width": 280}
        (root / "data").mkdir()
        (root / "config").mkdir()
        (root / "config/Settings.json").write_text(json.dumps({"interfaceLanguage":args.language,"firstRunComplete":True,"newTabPageURL":"about:blank"}))
        (root / "data/thoth-sidebar.json").write_text(json.dumps({"schema": 1, "windows": {"fixture": window}}))
        (root / "config/Bookmarks.json").write_text(json.dumps({"version": 2, "items": [{"id": "fixture-bookmark", "type": "bookmark", "title": "Fixture Bookmark", "url": base + "/bookmark"}]}))
        print(json.dumps({"profile": str(root), "fixture": base}), flush=True)
        try:
            subprocess.run([str(binary), "--profile-path", str(root)], check=True)
        finally:
            server.shutdown()
            server.server_close()
            for name in Fixture.download_names:
                downloaded = Path.home() / "Downloads" / name
                if downloaded.is_file() and downloaded.read_bytes() == b"thoth fixture download\n":
                    downloaded.unlink()


if __name__ == "__main__":
    main()
