#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Assert native session restoration loads only the current page/split."""
import argparse
import http.server
import json
from pathlib import Path
import runpy
import subprocess
import tempfile
import threading
import time

HELPERS = runpy.run_path(str(Path(__file__).with_name('thoth_e2e_test.py')))

class Fixture(http.server.BaseHTTPRequestHandler):
    hits = []
    def do_GET(self):
        if self.path.startswith('/page-'):
            self.hits.append(self.path)
        body = ('<!doctype html><title>'+self.path+'</title><h1>Lazy restore</h1>').encode()
        self.send_response(200)
        self.send_header('Content-Type','text/html')
        self.send_header('Content-Length',str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self,*_):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('webdriver',type=Path)
    args=parser.parse_args()
    for split in (False, True):
        Fixture.hits=[]
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Fixture)
        threading.Thread(target=server.serve_forever,daemon=True).start()
        base=f'http://127.0.0.1:{server.server_port}'
        with tempfile.TemporaryDirectory(prefix='thoth-restore-') as temp:
            profile=Path(temp)
            (profile/'data').mkdir();(profile/'config').mkdir()
            (profile/'config/Settings.json').write_text(json.dumps({'firstRunComplete':True,'newTabPageURL':'about:blank'}))
            pages=[{'id':str(i),'url':base+f'/page-{i}','title':f'Page {i}'} for i in range(8)]
            items=[{'id':f'item-{i}','pages':[str(i)],'active':str(i),'workspace':'test','section':'temporary','ratio':.5} for i in range(8)]
            if split:
                items[0]['pages']=['0','1'];items.pop(1)
            window={'schema':2,'workspaces':[{'id':'test','name':'Test','color':'#b9a5df','symbol':'book','active':'item-0'}], 'active_workspace':'test','pages':pages,'items':items,'active':'item-0','width':280}
            state_path=profile/'data/thoth-sidebar.json'
            state_path.write_text(json.dumps({'schema':1,'windows':{'fixture':window}}))
            port=HELPERS['HELPERS']['unused_port']()
            session=None
            with (profile/'runtime.log').open('w') as log:
                process=subprocess.Popen([str(args.webdriver.resolve()),'-l','127.0.0.1','-p',str(port),'--profile-path',str(profile)],stdout=log,stderr=subprocess.STDOUT)
                try:
                    HELPERS['HELPERS']['wait_for_port'](port)
                    session=HELPERS['create_graphical_session'](port)
                    expected={'/page-0','/page-1'} if split else {'/page-0'}
                    deadline=time.monotonic()+20
                    while not expected.issubset(set(Fixture.hits)) and time.monotonic()<deadline:
                        time.sleep(.1)
                    time.sleep(2)
                    assert set(Fixture.hits)==expected, (split,Fixture.hits)
                    status,payload,raw=HELPERS['HELPERS']['request'](port,'GET',f'/session/{session}/window/handles')
                    assert status==200 and len(payload['value'])==len(expected),raw
                    state=json.loads(state_path.read_text())
                    assert len(next(iter(state['windows'].values()))['pages'])==8
                    print('PASS:', 'split' if split else 'single', 'requests',Fixture.hits,'; all 8 sidebar entries retained',flush=True)
                finally:
                    if session:
                        HELPERS['HELPERS']['request'](port,'DELETE',f'/session/{session}')
                    process.terminate();process.wait(timeout=15)
        server.shutdown();server.server_close()

if __name__=='__main__':
    main()
