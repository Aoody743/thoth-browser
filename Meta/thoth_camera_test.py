#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Exercise camera denial safely, or run an explicitly requested hardware check.

Default WebDriver checks seed a BLOCK decision in a disposable profile, never
approve a website/system dialog, and never request OS camera access. --hardware
opens a visible window and waits for the human to approve both permission prompts;
it captures camera frames for verification, then stops every track. --serve-only
prints a local fixture URL; optional --app launches a disposable native profile.
"""

from __future__ import annotations

import argparse
import http.server
import json
from pathlib import Path
import runpy
import subprocess
import tempfile
import threading
import time
from urllib.parse import urlsplit

HELPERS = runpy.run_path(str(Path(__file__).resolve().parents[1] / "Tests/LibWebView/test-webdriver-delete-session.py"))

PAGE = r'''<!doctype html><meta charset="utf-8"><title>Thoth camera acceptance</title>
<style>body{font:17px system-ui;max-width:900px;margin:32px auto;padding:0 20px}button{padding:10px;margin:4px}video{display:block;width:640px;max-width:100%;background:#222}pre{white-space:pre-wrap;overflow-wrap:anywhere}label{display:block;margin:12px 0}</style>
<h1>Camera acceptance</h1>
<p>Start requests website and macOS permission. Approve these yourself only when
you intend to test your camera. Stop releases both the original and cloned tracks.</p>
<button id="start" onclick="startCamera()">Start camera</button>
<button id="stop" onclick="stopCamera()">Stop all tracks</button>
<button id="devices" onclick="showDevices()">List devices</button>
<button id="clone" onclick="cloneCamera()">Clone and stop original</button>
<button id="frame" onclick="inspectFrame()">Check video frame</button>
<button onclick="video.pause();inspectFrame()">Pause preview</button>
<button onclick="video.play()">Resume preview</button>
<button onclick="setCameraEnabled(false)">Disable video track</button>
<button onclick="setCameraEnabled(true)">Enable video track</button>
<video id="preview" muted autoplay playsinline></video><canvas id="canvas" hidden></canvas>
<pre id="status">ready</pre><pre id="device-list"></pre>
<script>
window.cameraResult = {state:'ready'};
window.cameraStreams = [];
window.cameraEvents = [];
const video = document.getElementById('preview');
for(const name of ['loadedmetadata','loadeddata','canplay','playing','pause'])
    video.addEventListener(name,()=>window.cameraEvents.push(name));
function report(result) {
    window.cameraResult = result;
    document.getElementById('status').textContent = JSON.stringify(result, null, 2);
    return result;
}
function errorResult(error) { return {state:'error',name:error.name,message:error.message}; }
async function showDevices() {
    try {
        const devices = (await navigator.mediaDevices.enumerateDevices()).map(d => ({kind:d.kind,deviceId:d.deviceId,label:d.label,groupId:d.groupId}));
        document.getElementById('device-list').textContent = JSON.stringify(devices,null,2);
        return devices;
    } catch(error) { report(errorResult(error)); return []; }
}
async function startCamera() {
    stopCamera();
    window.cameraEvents = [];
    report({state:'requesting'});
    try {
        const stream = await navigator.mediaDevices.getUserMedia({video:true,audio:false});
        window.cameraStreams.push(stream);
        video.srcObject = stream;
        try { await video.play(); } catch(error) { /* metadata polling reports playback failure */ }
        return report({state:'live',tracks:stream.getTracks().map(t=>({kind:t.kind,readyState:t.readyState,label:t.label}))});
    } catch(error) { return report(errorResult(error)); }
}
function stopCamera() {
    for(const stream of window.cameraStreams) for(const track of stream.getTracks()) track.stop();
    video.srcObject = null;
    return report({state:'stopped',tracks:window.cameraStreams.flatMap(s=>s.getTracks().map(t=>({kind:t.kind,readyState:t.readyState})))});
}
function setCameraEnabled(enabled) {
    const stream = window.cameraStreams.at(-1);
    if(stream)for(const track of stream.getVideoTracks())track.enabled = enabled;
}
async function cloneCamera() {
    try {
        const original = window.cameraStreams.at(-1);
        if(!original) throw new Error('Start a camera first');
        const clone = original.clone();
        window.cameraStreams.push(clone);
        for(const track of original.getTracks()) track.stop();
        video.srcObject = clone;
        try { await video.play(); } catch(error) {}
        return report({state:'cloned',original:original.getTracks().map(t=>t.readyState),clone:clone.getTracks().map(t=>t.readyState)});
    } catch(error) { return report(errorResult(error)); }
}
function inspectFrame() {
    try {
        const width = video.videoWidth, height = video.videoHeight;
        if(!width || !height) return report({state:'waiting-for-frame',width,height,readyState:video.readyState});
        const canvas = document.getElementById('canvas');
        canvas.width = 32; canvas.height = 24;
        const context = canvas.getContext('2d');
        context.drawImage(video,0,0,32,24);
        const bytes = context.getImageData(0,0,32,24).data;
        let rgbTotal=0, opaquePixels=0, frameHash=2166136261;
        for(let i=0;i<bytes.length;i++)frameHash = Math.imul(frameHash ^ bytes[i],16777619) >>> 0;
        for(let i=0;i<bytes.length;i+=4){rgbTotal+=bytes[i]+bytes[i+1]+bytes[i+2];if(bytes[i+3]===255)opaquePixels++;}
        return report({state:'frame',width,height,readyState:video.readyState,rgbTotal,opaquePixels,frameHash,paused:video.paused,currentTime:video.currentTime,events:window.cameraEvents.slice()});
    } catch(error) { return report(errorResult(error)); }
}
window.addEventListener('pagehide',stopCamera);
</script>'''


class Fixture(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        path = urlsplit(self.path).path
        if path not in ("/", "/camera", "/policy-blocked"):
            self.send_error(404)
            return
        body = PAGE.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        if path == "/policy-blocked":
            self.send_header("Permissions-Policy", "camera=(), microphone=()")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def command(port, session, endpoint, body=None, method="POST"):
    status, payload, raw = HELPERS["request"](port, method, f"/session/{session}/{endpoint}", body)
    if status != 200:
        raise AssertionError(f"WebDriver {endpoint} failed: {raw}")
    return payload.get("value")


def script(port, session, source):
    return command(port, session, "execute/sync", {"script": source, "args": []})


def wait_script(port, session, source, predicate, timeout):
    deadline = time.monotonic() + timeout
    result = None
    while time.monotonic() < deadline:
        result = script(port, session, source)
        if predicate(result):
            return result
        time.sleep(.2)
    raise AssertionError(f"Timed out waiting for browser state; last result: {result}")


def rejection(port, session, expression):
    script(port, session, "window.testResult=null; (async()=>{try {const stream=await (" + expression + ");if(stream&&stream.getTracks)for(const track of stream.getTracks())track.stop();window.testResult={resolved:true};}catch(error){window.testResult={name:error.name,message:error.message};}})();")
    return wait_script(port, session, "return window.testResult;", lambda value: isinstance(value, dict), 30)


def enumerate_devices(port, session):
    script(port, session, "window.deviceResult=null; navigator.mediaDevices.enumerateDevices().then(devices=>{window.deviceResult={devices:devices.map(d=>({kind:d.kind,deviceId:d.deviceId,label:d.label}))};},error=>{window.deviceResult={error:error.name};});return true;")
    result = wait_script(port,session,"return window.deviceResult;",lambda x:isinstance(x,dict),15)
    if not isinstance(result.get("devices"),list):
        raise AssertionError(f"Device enumeration failed: {result}")
    return result["devices"]


def run_checks(args, profile, base):
    if not args.hardware:
        config = profile / "config"
        config.mkdir()
        (config / "thoth-site-permissions.json").write_text(json.dumps({"schema":1,"camera":{base:False},"microphone":{base:False}}))
    port = HELPERS["unused_port"]()
    log_path = profile / "webdriver.log"
    with log_path.open("w") as log:
        process = subprocess.Popen([str(args.webdriver.resolve()), "-l", "127.0.0.1", "-p", str(port), "--profile-path", str(profile)], stdout=log, stderr=subprocess.STDOUT)
        session = None
        checks = []
        try:
            HELPERS["wait_for_port"](port)
            status, payload, raw = HELPERS["request"](port, "POST", "/session", {"capabilities":{"alwaysMatch":{"ladybird:headless":False}}})
            session = payload.get("value", {}).get("sessionId")
            if status != 200 or not session:
                raise AssertionError(f"Cannot create graphical session: {raw}")
            command(port, session, "url", {"url":base + "/camera"})
            available = script(port, session, "return {secure:isSecureContext,mediaDevices:typeof navigator.mediaDevices,getUserMedia:typeof navigator.mediaDevices?.getUserMedia};")
            if available != {"secure":True,"mediaDevices":"object","getUserMedia":"function"}:
                raise AssertionError(f"Secure localhost camera API missing: {available}")
            checks.append({"check":"secure camera API", "result":available})
            devices = enumerate_devices(port,session)
            cameras = [d for d in devices if d["kind"] == "videoinput"]
            if len(cameras) > 1 or any(d["deviceId"] or d["label"] for d in cameras):
                raise AssertionError(f"Camera identity exposed before website permission: {cameras}")
            checks.append({"check":"camera enumeration privacy before grant", "result":cameras})
            for expression in ("navigator.mediaDevices.getUserMedia({})", "navigator.mediaDevices.getUserMedia({audio:false,video:false})"):
                result = rejection(port, session, expression)
                if result.get("name") != "TypeError":
                    raise AssertionError(f"Invalid constraints did not reject with TypeError: {result}")
                checks.append({"check":"invalid constraints", "result":result})
            if not args.hardware:
                result = rejection(port, session, "navigator.mediaDevices.getUserMedia({video:true,audio:false})")
                if result.get("name") != "NotAllowedError":
                    raise AssertionError(f"Blocked site camera request did not reject with NotAllowedError: {result}")
                checks.append({"check":"remembered website denial", "result":result})
            else:
                print("Hardware mode: approve website/macOS camera prompts yourself. No dialog will be accepted automatically.", flush=True)
                script(port, session, "startCamera(); return true;")
                result = wait_script(port, session, "return window.cameraResult;", lambda x:isinstance(x,dict) and x.get("state") in ("live","error"), args.permission_timeout)
                if result.get("state") != "live":
                    raise AssertionError(f"Hardware camera did not start: {result}")
                checks.append({"check":"hardware stream", "result":{"state":result["state"], "tracks":[{"kind":t["kind"], "readyState":t["readyState"]} for t in result["tracks"]]}})
                cameras = [d for d in enumerate_devices(port,session) if d["kind"] == "videoinput"]
                if not cameras or not any(d["deviceId"] and d["label"] for d in cameras):
                    raise AssertionError(f"Granted camera identity missing from enumerateDevices: {cameras}")
                # Device IDs and labels are not printed; they can identify hardware.
                checks.append({"check":"camera enumeration after grant", "result":{"cameraCount":len(cameras)}})
                frame = wait_script(port, session, "return inspectFrame();", lambda x:isinstance(x,dict) and x.get("state") in ("frame","error"), 20)
                if frame.get("state") != "frame" or frame.get("opaquePixels",0) == 0:
                    raise AssertionError(f"Video did not produce canvas pixels: {frame}")
                checks.append({"check":"hardware preview pixels", "result":frame})
                metadata = wait_script(port,session,"return window.cameraEvents;",lambda x:isinstance(x,list) and "loadedmetadata" in x and "loadeddata" in x,10)
                checks.append({"check":"metadata and first-frame events", "result":metadata})
                script(port,session,"video.pause();return true;")
                # Two event-loop-separated samples verify the presented frame,
                # rather than merely checking the paused property.
                time.sleep(.3)
                frozen = script(port,session,"return inspectFrame();")
                time.sleep(.6)
                paused = script(port,session,"return inspectFrame();")
                if not paused.get("paused") or paused.get("frameHash") != frozen.get("frameHash"):
                    raise AssertionError(f"Paused preview did not retain its presented frame: {frozen}, {paused}")
                checks.append({"check":"paused preview freezes", "result":{"paused":True,"unchangedFrame":True}})
                script(port,session,"video.play();setCameraEnabled(false);return true;")
                black = wait_script(port,session,"return inspectFrame();",lambda x:isinstance(x,dict) and x.get("state")=="frame" and x.get("rgbTotal")==0 and x.get("opaquePixels")==32*24,10)
                checks.append({"check":"disabled video track produces opaque black", "result":{"rgbTotal":black["rgbTotal"],"opaquePixels":black["opaquePixels"]}})
                script(port,session,"setCameraEnabled(true);return true;")
                # A physically covered camera may also be black. Do not claim
                # motion/nonblack pixels are guaranteed when restoring a track.
                wait_script(port,session,"return inspectFrame();",lambda x:isinstance(x,dict) and x.get("state")=="frame" and not x.get("paused"),10)
                clone = script(port, session, "cloneCamera(); return true;")
                clone = wait_script(port, session, "return window.cameraResult;", lambda x:isinstance(x,dict) and x.get("state") in ("cloned","error"), 10)
                if clone.get("state") != "cloned" or clone.get("original") != ["ended"] or clone.get("clone") != ["live"]:
                    raise AssertionError(f"Cloned track lifetime failed: {clone}")
                checks.append({"check":"clone survives original stop", "result":clone})
                frame = wait_script(port, session, "return inspectFrame();", lambda x:isinstance(x,dict) and x.get("state") in ("frame","error"), 20)
                if frame.get("state") != "frame":
                    raise AssertionError(f"Clone preview failed: {frame}")
                stopped = script(port, session, "return stopCamera();")
                if not stopped.get("tracks") or any(t["readyState"] != "ended" for t in stopped["tracks"]):
                    raise AssertionError(f"Camera tracks did not stop: {stopped}")
                checks.append({"check":"all hardware tracks stopped", "result":stopped})
            # Separate origin has no seeded site block: success demonstrates the
            # Permissions-Policy check, rather than the remembered-denial path.
            policy_base = base.replace("127.0.0.1", "localhost")
            command(port, session, "url", {"url":policy_base + "/policy-blocked"})
            result = rejection(port, session, "navigator.mediaDevices.getUserMedia({video:true,audio:false})")
            if result.get("name") != "NotAllowedError":
                raise AssertionError(f"Camera Permissions-Policy rejection failed: {result}")
            checks.append({"check":"camera Permissions-Policy", "result":result})
            result_document = {"passed":True,"hardware":args.hardware,"checks":checks}
            if args.output:
                args.output.write_text(json.dumps(result_document,indent=2))
            print(json.dumps(result_document,indent=2), flush=True)
        except Exception as error:
            if args.output:
                args.output.write_text(json.dumps({"passed":False,"hardware":args.hardware,"checks":checks,"error":str(error)},indent=2))
            print(log_path.read_text(errors="replace")[-10000:], flush=True)
            raise
        finally:
            if session:
                try:
                    script(port, session, "if(window.stopCamera)stopCamera();return true;")
                    HELPERS["request"](port,"DELETE",f"/session/{session}")
                except Exception:
                    pass
            process.terminate()
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("webdriver", nargs="?", type=Path, help="WebDriver binary for automated checks")
    parser.add_argument("--output", type=Path, help="Write a JSON acceptance record (no camera images)")
    parser.add_argument("--hardware", action="store_true", help="Explicitly request real camera capture; human must approve permission prompts")
    parser.add_argument("--permission-timeout", type=int, default=120, help="Seconds to allow human approval in --hardware mode")
    parser.add_argument("--serve-only", action="store_true", help="Serve manual fixture until interrupted, without running WebDriver tests")
    parser.add_argument("--app", type=Path, help="With --serve-only, launch this .app bundle in a disposable profile")
    parser.add_argument("--bind", default="127.0.0.1", help="Fixture bind address (non-loopback addresses allow manual insecure-context checks)")
    args = parser.parse_args()
    if not args.serve_only and not args.webdriver:
        parser.error("provide WebDriver binary or --serve-only")
    if args.app and not args.serve_only:
        parser.error("--app requires --serve-only")
    if args.hardware and args.serve_only:
        parser.error("manual Start button requests camera in --serve-only; --hardware is for WebDriver")
    if not args.serve_only and args.bind != "127.0.0.1":
        parser.error("automated checks require the default loopback bind")
    server = http.server.ThreadingHTTPServer((args.bind,0),Fixture)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    host = "127.0.0.1" if args.bind == "0.0.0.0" else args.bind
    base = f"http://{host}:{server.server_port}"
    try:
        with tempfile.TemporaryDirectory(prefix="thoth-camera-") as root:
            profile = Path(root)
            if args.serve_only:
                print(json.dumps({"fixture":base + "/camera","policy_blocked":base + "/policy-blocked","profile":root}),flush=True)
                if args.app:
                    subprocess.run([str(args.app.resolve()/"Contents/MacOS/Thoth Browser"),"--profile-path",root,base+"/camera"],check=True)
                else:
                    while True:
                        time.sleep(1)
            else:
                run_checks(args,profile,base)
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
