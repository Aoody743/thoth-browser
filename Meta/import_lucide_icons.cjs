// Copyright (c) 2026, Thoth Browser contributors.
// SPDX-License-Identifier: BSD-2-Clause
// Before running from the repository root:
// npm install --prefix Build/icon-tools --ignore-scripts --no-audit --no-fund @resvg/resvg-js@2.6.2 lucide-static@1.61.0
const fs = require('fs');
const path = require('path');
const {Resvg} = require(process.cwd() + '/Build/icon-tools/node_modules/@resvg/resvg-js');
const pkg = process.cwd() + '/Build/icon-tools/node_modules/lucide-static';
if (JSON.parse(fs.readFileSync(path.join(pkg,'package.json'),'utf8')).version !== '1.61.0') throw new Error('Expected lucide-static 1.61.0');
const out = 'UI/Qt/Thoth/Icons/lucide';
fs.mkdirSync(out,{recursive:true});
const names = 'arrow-left arrow-right rotate-cw x plus menu star search globe lock-keyhole-open lock-keyhole folder arrow-down-to-line volume-2 volume-x chevron-up chevron-down chevron-right panel-left-close panel-right-close panel-left-open panel-right-open minus square copy book-open diamond orbit shield-check settings history house link square-arrow-out-up-right pin pin-off moon trash-2 square-split-horizontal camera mic file info folder-plus cloud-download loader-circle circle-help circle check circle-alert external-link'.split(' ');
for (const name of [...names, 'star-filled']) {
 const svg = fs.readFileSync(path.join(pkg,'icons',(name === 'star-filled' ? 'star' : name)+'.svg'),'utf8').replace('fill="none"', name === 'star-filled' ? 'fill="currentColor"' : 'fill="none"');
 fs.writeFileSync(path.join(out,name+'.svg'),svg);
 for (const [weight,stroke] of [['fine','1.6'],['regular','2'],['expressive','2.4']]) {
  const source = svg.replace('stroke-width="2"',`stroke-width="${stroke}"`);
  fs.writeFileSync(path.join(out,`${name}-${weight}.png`),new Resvg(source,{fitTo:{mode:'width',value:96}}).render().asPng());
 }
}
fs.copyFileSync(path.join(pkg,'LICENSE'),path.join(out,'LICENSE'));
fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify({package:'lucide-static',version:'1.61.0',source:'https://github.com/lucide-icons/lucide',license:'ISC',rasterizer:'@resvg/resvg-js@2.6.2',icons:[...names,'star-filled'],derived:{'star-filled':'Upstream star SVG with currentColor fill'}},null,2)+'\n');
