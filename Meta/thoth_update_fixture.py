#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Build and serve a disposable native app for Sparkle download/restart QA.

Uses the production Updater.mm with a separate bundle ID and local HTTP feed.
No browser profile is opened. Approve signing-key access yourself if prompted.
Use the fixture's Restart and Install Update button, then inspect the current
bundle's CFBundleVersion (2) and current/launched.txt for both launch versions.
"""
from pathlib import Path
import plistlib, subprocess, json
source=Path(__file__).resolve().parents[1]
root=source/'Build/update-fixture';root.mkdir(parents=True,exist_ok=True)

sparkle=Path.home()/'.cache/thoth-browser/Sparkle-2.10.0'
(root/'main.cpp').write_text('''#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QFile>
#include <QTimer>\n#include <QRegularExpression>
#include <UI/Qt/Thoth/Updater.h>
namespace Thoth { QString localized_text(QString const& x) { return x; } }
int main(int argc,char** argv) {
 QApplication app(argc,argv); QMainWindow window;
 window.setWindowTitle("Thoth Update Fixture"); window.resize(500,300);
 auto* menu=window.menuBar()->addMenu("Browser");
 Thoth::add_update_menu(*menu,[]{ return true; }, []{});
 window.show();
 QFile marker(QApplication::applicationDirPath()+"/../../../launched.txt");
 if(marker.open(QIODevice::WriteOnly|QIODevice::Append)) {
 QFile info(QApplication::applicationDirPath()+"/../Info.plist");
 if(info.open(QIODevice::ReadOnly)) marker.write(info.readAll());
 }
 return app.exec();
}
''')
(root/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(ThothUpdaterFixture LANGUAGES CXX OBJCXX)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(fixture main.cpp "{source}/UI/Qt/Thoth/Updater.mm")
target_include_directories(fixture PRIVATE "{source}")
target_compile_options(fixture PRIVATE -fobjc-arc)
target_link_libraries(fixture PRIVATE Qt6::Widgets "{sparkle}/Sparkle.framework" "-framework Cocoa")
set_property(TARGET fixture APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
''')
subprocess.run(['cmake','-S',str(root),'-B',str(root/'compile'),'-G','Ninja','-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qtbase'],check=True)
subprocess.run(['cmake','--build',str(root/'compile')],check=True)

from pathlib import Path
import plistlib, shutil, subprocess, http.server, threading, json, socket, time
root=Path(__file__).resolve().parents[1]/'Build/update-fixture'
sparkle=Path.home()/'.cache/thoth-browser/Sparkle-2.10.0'
releases=root/'releases';releases.mkdir(exist_ok=True)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),lambda *a,**k:http.server.SimpleHTTPRequestHandler(*a,directory=str(releases),**k))
base=f'http://127.0.0.1:{server.server_port}'
threading.Thread(target=server.serve_forever,daemon=True).start()
public=subprocess.check_output([str(sparkle/'bin/generate_keys'),'--account','thoth.aoody','-p'],text=True).strip()
for version,folder in [('1','current'),('2','candidate')]:
 app=root/folder/'Thoth Update Fixture.app'
 if app.exists():shutil.rmtree(app)
 (app/'Contents/MacOS').mkdir(parents=True)
 (app/'Contents/Frameworks').mkdir()
 shutil.copy2(root/'compile/fixture',app/'Contents/MacOS/fixture')
 subprocess.run(['ditto',str(sparkle/'Sparkle.framework'),str(app/'Contents/Frameworks/Sparkle.framework')],check=True)
 info={'CFBundleExecutable':'fixture','CFBundleIdentifier':'thoth.aoody.updater-fixture','CFBundleName':'Thoth Update Fixture','CFBundlePackageType':'APPL','CFBundleVersion':version,'CFBundleShortVersionString':version,'LSMinimumSystemVersion':'14.4','NSPrincipalClass':'NSApplication','SUFeedURL':base+'/appcast.xml','SUPublicEDKey':public,'SUEnableAutomaticChecks':True,'SUAutomaticallyUpdate':True,'SUVerifyUpdateBeforeExtraction':True,'NSAppTransportSecurity':{'NSAllowsLocalNetworking':True,'NSAllowsArbitraryLoads':True}}
 (app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
 subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True)
 if version=='2':
  archive=releases/'Thoth-Update-Fixture-2.zip'
  if archive.exists():archive.unlink()
  subprocess.run(['ditto','-c','-k','--sequesterRsrc','--keepParent',str(app),str(archive)],check=True)
if (releases/'appcast.xml').exists(): (releases/'appcast.xml').unlink()
# Generate real Sparkle metadata and Ed25519 signature using the configured key.
subprocess.run([str(sparkle/'bin/generate_appcast'),'--account','thoth.aoody','--download-url-prefix',base+'/',str(releases)],check=True)
state={'base':base,'app':str(root/'current/Thoth Update Fixture.app')}
(root/'state.json').write_text(json.dumps(state))
subprocess.run(['defaults','delete','thoth.aoody.updater-fixture'],capture_output=True)
process=subprocess.Popen([state['app']+'/Contents/MacOS/fixture'])
print(json.dumps(state),flush=True)
try:
 while True:time.sleep(1)
finally:server.shutdown()
