#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Run a small real-browser WebDriver smoke scenario against Thoth."""

from __future__ import annotations

import argparse
import http.server
import json
import os
import runpy
import socketserver
import subprocess
import tempfile
import threading
import time
from pathlib import Path

HELPERS = runpy.run_path(str(Path(__file__).resolve().parents[1] / "Tests/LibWebView/test-webdriver-delete-session.py"))


def create_graphical_session(webdriver_port):
    status, payload, response_body = HELPERS["request"](
        webdriver_port,
        "POST",
        "/session",
        {"capabilities": {"alwaysMatch": {"ladybird:headless": False}}},
    )
    value = payload.get("value")
    session_id = value.get("sessionId") if isinstance(value, dict) else None
    if status != 200 or not session_id:
        raise AssertionError(f"New graphical session failed with HTTP {status}: {response_body}")
    return session_id


def wait_for_sidebar_url(state_path: Path, expected_url: str) -> dict:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            state = json.loads(state_path.read_text())
            urls = [
                page.get("url")
                for window in state.get("windows", {}).values()
                for page in window.get("pages", [])
            ]
            if expected_url in urls:
                return state
        except (OSError, json.JSONDecodeError):
            pass
        time.sleep(0.1)
    raise AssertionError(f"Thoth sidebar did not persist the current URL: {expected_url}")


def sidebar_item_urls(state: dict) -> list[str]:
    windows = state.get("windows", {})
    if len(windows) != 1:
        return []
    window = next(iter(windows.values()))
    urls_by_page = {page.get("id"): page.get("url") for page in window.get("pages", [])}
    return [
        urls_by_page[page_id]
        for item in window.get("items", [])
        if not item.get("folder")
        for page_id in item.get("pages", [])
        if page_id in urls_by_page
    ]


def wait_for_sidebar_item_order(state_path: Path, expected_urls: list[str]) -> dict:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            state = json.loads(state_path.read_text())
            if sidebar_item_urls(state) == expected_urls:
                return state
        except (OSError, json.JSONDecodeError):
            pass
        time.sleep(0.1)
    raise AssertionError(f"Thoth sidebar item order did not restore: {expected_urls}")


def prepare_split_workspace_snapshot(state_path: Path, page_urls: list[str], split_ratio: float) -> dict:
    state = json.loads(state_path.read_text())
    windows = state.get("windows", {})
    if len(windows) != 1:
        raise AssertionError(f"expected one window before preparing split restore, got {len(windows)}")

    window = next(iter(windows.values()))
    page_ids = {page.get("url"): page.get("id") for page in window.get("pages", [])}
    if any(url not in page_ids or not page_ids[url] for url in page_urls):
        raise AssertionError(f"cannot find pages for split restore: {page_ids}")

    page_to_item = {
        page_id: item
        for item in window.get("items", [])
        for page_id in item.get("pages", [])
    }
    first_page_id, second_page_id = (page_ids[url] for url in page_urls)
    first_item = page_to_item.get(first_page_id)
    second_item = page_to_item.get(second_page_id)
    if not first_item or not second_item or first_item is second_item:
        raise AssertionError("saved pages do not belong to two distinct sidebar items")

    first_item["pages"] = [first_page_id, second_page_id]
    first_item["active"] = second_page_id
    first_item["ratio"] = split_ratio
    window["items"] = [item for item in window["items"] if item is not second_item]
    window["active"] = first_item["id"]

    workspace_id = "thoth-e2e-research"
    workspaces = window.get("workspaces", [])
    if not workspaces:
        raise AssertionError("saved window has no workspace")
    workspaces[0]["active"] = ""
    workspaces.append({
        "id": workspace_id,
        "name": "E2E Research",
        "color": "#5865D6",
        "symbol": "book",
        "active": first_item["id"],
    })
    window["active_workspace"] = workspace_id
    first_item["workspace"] = workspace_id

    state_path.write_text(json.dumps(state, separators=(",", ":")))
    return {
        "active_page": second_page_id,
        "active_url": page_urls[1],
        "item_id": first_item["id"],
        "page_ids": [first_page_id, second_page_id],
        "ratio": split_ratio,
        "workspace_id": workspace_id,
    }


def wait_for_webdriver_url(webdriver_port: int, session_id: str, expected_url: str) -> None:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        status, payload, _ = HELPERS["request"](webdriver_port, "GET", f"/session/{session_id}/url")
        if status == 200 and payload.get("value") == expected_url:
            return
        time.sleep(0.1)
    raise AssertionError(f"WebDriver did not attach to the restored page: {expected_url}")


class FixtureHandler(http.server.BaseHTTPRequestHandler):
    download_name = ""
    download_body = b""
    downloads = []

    def do_GET(self):
        if self.path == "/download.txt":
            body = self.download_body
            self.downloads.append((self.download_name, self.headers.get("Host", "")))
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Disposition", f"attachment; filename={self.download_name}")
        else:
            title = "Thoth E2E second" if self.path == "/second" else "Thoth E2E"
            body = f"""<!doctype html><title>{title}</title>
<main id="status">ready</main>
<a id="download" href="/download.txt">Download fixture</a>
<script>document.getElementById("status").dataset.checked = "true";</script>""".encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("webdriver")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="thoth-e2e-") as profile:
        FixtureHandler.download_name = f"thoth-e2e-{os.getpid()}-{time.time_ns()}.txt"
        FixtureHandler.download_body = b"thoth-download-ok\n"
        FixtureHandler.downloads = []
        expected_download = Path.home() / "Downloads" / FixtureHandler.download_name
        if expected_download.exists():
            raise RuntimeError(f"refusing to overwrite existing download: {expected_download}")
        server = socketserver.TCPServer(("127.0.0.1", 0), FixtureHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        port = HELPERS["unused_port"]()
        process = subprocess.Popen(
            [args.webdriver, "-l", "127.0.0.1", "-p", str(port), "--profile-path", profile],
        )
        session = None
        try:
            HELPERS["wait_for_port"](port)
            session = create_graphical_session(port)
            url = f"http://127.0.0.1:{server.server_address[1]}/"
            sidebar_state = Path(profile) / "data" / "thoth-sidebar.json"
            status, payload, raw = HELPERS["request"](port, "POST", f"/session/{session}/url", {"url": url})
            if status != 200:
                raise AssertionError(f"navigation failed: {raw}")
            wait_for_sidebar_url(sidebar_state, url)
            status, payload, raw = HELPERS["request"](port, "GET", f"/session/{session}/title")
            if status != 200 or payload.get("value") != "Thoth E2E":
                raise AssertionError(f"unexpected title: {raw}")
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/execute/sync",
                {"script": "return document.querySelector('#status').dataset.checked;", "args": []},
            )
            if status != 200 or payload.get("value") != "true":
                raise AssertionError(f"unexpected DOM state: {raw}")

            status, payload, raw = HELPERS["request"](port, "GET", f"/session/{session}/window/handles")
            if status != 200 or len(payload.get("value", [])) != 1:
                raise AssertionError(f"unexpected initial browsing contexts: {raw}")
            first_handle = payload["value"][0]
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/window/new",
                {"type": "tab"},
            )
            second_handle = payload.get("value", {}).get("handle")
            if status != 200 or not second_handle or second_handle == first_handle:
                raise AssertionError(f"opening a second tab failed: {raw}")
            second_url = f"http://127.0.0.1:{server.server_address[1]}/second"
            status, _, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/window",
                {"handle": second_handle},
            )
            if status != 200:
                raise AssertionError(f"switching to the second tab failed: {raw}")
            status, _, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/url",
                {"url": second_url},
            )
            if status != 200:
                raise AssertionError(f"second-tab navigation failed: {raw}")
            wait_for_sidebar_url(sidebar_state, second_url)
            status, payload, raw = HELPERS["request"](port, "GET", f"/session/{session}/title")
            if status != 200 or payload.get("value") != "Thoth E2E second":
                raise AssertionError(f"unexpected second-tab title: {raw}")
            status, _, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/window",
                {"handle": first_handle},
            )
            if status != 200:
                raise AssertionError(f"switching back to the first tab failed: {raw}")
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/execute/sync",
                {"script": "document.querySelector('#download').click(); return document.title;", "args": []},
            )
            if status != 200 or payload.get("value") != "Thoth E2E":
                raise AssertionError(f"download trigger failed: {raw}")
            deadline = time.monotonic() + 10
            expected_host = f"127.0.0.1:{server.server_address[1]}"
            while time.monotonic() < deadline:
                if any(name == FixtureHandler.download_name and host == expected_host for name, host in FixtureHandler.downloads):
                    break
                time.sleep(0.05)
            if not any(name == FixtureHandler.download_name and host == expected_host for name, host in FixtureHandler.downloads):
                raise AssertionError("browser did not request the download fixture")
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and not expected_download.is_file():
                time.sleep(0.1)
            if not expected_download.is_file() or expected_download.read_bytes() != FixtureHandler.download_body:
                raise AssertionError(f"download did not complete at {expected_download}")
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/url",
                {"url": "about:downloads"},
            )
            if status != 200:
                raise AssertionError(f"downloads navigation failed: {raw}")
            wait_for_sidebar_url(sidebar_state, "about:downloads")
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/execute/sync",
                {"script": "return document.querySelector('h1')?.textContent;", "args": []},
            )
            if status != 200 or payload.get("value") != "Downloads":
                raise AssertionError(f"downloads page failed to render: {raw}")
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/url",
                {"url": "about:history"},
            )
            if status != 200:
                raise AssertionError(f"history navigation failed: {raw}")
            saved_state = wait_for_sidebar_item_order(sidebar_state, ["about:history", second_url])
            saved_windows = list(saved_state.get("windows", {}).values())
            if len(saved_windows) != 1:
                raise AssertionError(f"expected one saved browser window, got {len(saved_windows)}")
            saved_page_urls = sidebar_item_urls(saved_state)
            if len(saved_page_urls) != 2 or set(saved_page_urls) != {"about:history", second_url}:
                raise AssertionError(f"saved pages do not match the two open tabs: {saved_page_urls}")
            status, payload, raw = HELPERS["request"](
                port,
                "POST",
                f"/session/{session}/execute/sync",
                {"script": "return document.querySelector('h1')?.textContent;", "args": []},
            )
            if status != 200 or payload.get("value") != "History":
                raise AssertionError(f"history page failed to render: {raw}")
            expected_download.unlink()
            status, _, raw = HELPERS["request"](port, "DELETE", f"/session/{session}")
            if status != 200:
                raise AssertionError(f"closing the first browser session failed: {raw}")
            session = None
            process.terminate()
            process.wait(timeout=10)

            split_snapshot = prepare_split_workspace_snapshot(
                sidebar_state,
                ["about:history", second_url],
                0.68,
            )

            port = HELPERS["unused_port"]()
            process = subprocess.Popen(
                [args.webdriver, "-l", "127.0.0.1", "-p", str(port), "--profile-path", profile],
            )
            HELPERS["wait_for_port"](port)
            session = create_graphical_session(port)
            wait_for_webdriver_url(port, session, split_snapshot["active_url"])
            status, payload, raw = HELPERS["request"](port, "GET", f"/session/{session}/title")
            if status != 200 or payload.get("value") != "Thoth E2E second":
                raise AssertionError(f"restored active split page failed to render: {raw}")

            # Inspect the restored active pane before switching WebDriver between
            # handles below; each explicit handle switch changes the browser's
            # active page and would overwrite the state being tested.
            restored_state = wait_for_sidebar_item_order(sidebar_state, saved_page_urls)
            restored_window = next(iter(restored_state["windows"].values()))
            restored_item = next((item for item in restored_window["items"] if item["id"] == split_snapshot["item_id"]), None)
            restored_workspace = next((space for space in restored_window["workspaces"] if space["id"] == split_snapshot["workspace_id"]), None)
            if not restored_item or restored_item["pages"] != split_snapshot["page_ids"]:
                raise AssertionError("split page membership did not survive browser restart")
            if restored_item["active"] != split_snapshot["active_page"]:
                raise AssertionError("active split pane did not survive browser restart")
            if abs(restored_item["ratio"] - split_snapshot["ratio"]) > 0.01:
                raise AssertionError(f"split ratio did not survive browser restart: {restored_item['ratio']}")
            if restored_window["active_workspace"] != split_snapshot["workspace_id"] or not restored_workspace:
                raise AssertionError("active workspace did not survive browser restart")
            if restored_workspace["name"] != "E2E Research" or restored_workspace["color"] != "#5865D6":
                raise AssertionError("workspace settings did not survive browser restart")

            status, payload, raw = HELPERS["request"](port, "GET", f"/session/{session}/window/handles")
            restored_handles = payload.get("value", [])
            if status != 200 or len(restored_handles) != 2:
                raise AssertionError(f"unexpected restored window set: {raw}")
            restored_urls = []
            for handle in restored_handles:
                status, _, raw = HELPERS["request"](
                    port,
                    "POST",
                    f"/session/{session}/window",
                    {"handle": handle},
                )
                if status != 200:
                    raise AssertionError(f"switching to a restored tab failed: {raw}")
                status, payload, raw = HELPERS["request"](port, "GET", f"/session/{session}/url")
                if status != 200:
                    raise AssertionError(f"reading a restored tab URL failed: {raw}")
                restored_urls.append(payload.get("value"))
            if set(restored_urls) != set(saved_page_urls):
                raise AssertionError(f"restored pages do not match the saved pages: {restored_urls}")
            restored_state = wait_for_sidebar_item_order(sidebar_state, saved_page_urls)
            if sidebar_item_urls(restored_state) != saved_page_urls:
                raise AssertionError(f"restored sidebar order changed: {sidebar_item_urls(restored_state)}")
            print("PASS: Thoth navigation, DOM, downloads, internal pages, split/workspace and browser restart restore")
        finally:
            if session is not None and process.poll() is None:
                HELPERS["request"](port, "DELETE", f"/session/{session}")
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            server.shutdown()
            server.server_close()
            expected_download.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
