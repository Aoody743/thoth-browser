#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Prepare signed Sparkle archives/appcasts without publishing them."""
import argparse
import plistlib
import re
import subprocess
from pathlib import Path
from urllib.parse import urlsplit


def https_url(value):
    parsed = urlsplit(value)
    if parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password:
        raise argparse.ArgumentTypeError('a public HTTPS URL is required')
    return value


def run(*command):
    return subprocess.run([str(x) for x in command], check=True, capture_output=True, text=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--download-prefix', required=True, type=https_url)
    parser.add_argument('--sparkle-root', type=Path, default=Path.home()/'.cache/thoth-browser/Sparkle-2.10.0')
    parser.add_argument('--account', default='thoth.aoody')
    args = parser.parse_args()
    app = args.app.resolve()
    info = plistlib.loads((app/'Contents/Info.plist').read_bytes())
    if info.get('CFBundleIdentifier') != 'thoth.aoody':
        parser.error('expected a Thoth Browser application')
    try:
        https_url(info.get('SUFeedURL', ''))
    except argparse.ArgumentTypeError:
        parser.error('configure THOTH_UPDATE_FEED_URL and rebuild before preparing updates')
    build = info['CFBundleVersion']
    if not re.fullmatch(r'\d+(?:\.\d+)*', build):
        parser.error('CFBundleVersion must contain increasing numeric components')
    public_key = run(args.sparkle_root/'bin/generate_keys', '--account', args.account, '-p').strip()
    if public_key != info.get('SUPublicEDKey'):
        parser.error('the signing key does not match the application public key')
    run('codesign', '--verify', '--deep', '--strict', app)
    details = subprocess.run(['codesign','-dv','--verbose=4',str(app)],check=True,capture_output=True,text=True).stderr
    if 'Authority=Developer ID Application:' not in details:
        parser.error('public updates require a Developer ID signed application')
    if not (app/'Contents/Frameworks/Sparkle.framework').is_dir():
        parser.error('Sparkle.framework must be embedded')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive = output/f'Thoth-Browser-{build}.zip'
    if archive.exists():
        parser.error('an archive with this build number already exists; increment the build number')
    run('ditto','-c','-k','--sequesterRsrc','--keepParent',app,archive)
    print(run(args.sparkle_root/'bin/generate_appcast', '--account',args.account,
              '--download-url-prefix',args.download_prefix.rstrip('/')+'/',output))
    print(f'Prepared {archive} and appcast.xml. No files were published.')


if __name__ == '__main__':
    main()
