#!/usr/bin/env node
const fs=require('node:fs'),path=require('node:path'),zlib=require('node:zlib');
const root=path.resolve(__dirname,'..');
let html=fs.readFileSync(path.join(root,'web/index.html'),'utf8');
const previews={};
for(let i=13;i<17;i++)previews[i]='data:image/png;base64,'+fs.readFileSync(path.join(root,'docs/complete/face-'+i+'.png')).toString('base64');
html=html.replace("const faceSrc=f=>f.src||",'const COMPLETE_PREVIEWS='+JSON.stringify(previews)+';\nconst faceSrc=f=>f.src||COMPLETE_PREVIEWS[f.t]||');
html=html.replaceAll('SmallTV Pomodoro','SmallTV Complete');
const dictionary=fs.readFileSync(path.join(root,'web/i18n.json'),'utf8');
html=html.replace('<main>','<main><section class="card"><h2>Screens and AI limits</h2><p>Choose a fixed screen, timed rotation, or the combined Claude and Codex dashboard.</p><a class="btn p" href="/display">Configure screens and AI limits</a></section>');
html=html.replace('let LG=0;','let LG=1;');
html=html.replace(/function checkUpdate\(\)\{[\s\S]*?\n(?=function showUpdate)/,'function checkUpdate(){}\n');
const begin=html.indexOf('function loadI18n(){'),end=html.indexOf('// chamado por load()',begin);
if(begin<0||end<0)throw Error('i18n integration point missing');
html=html.slice(0,begin)+`const LOCAL_I18N=${dictionary};\nfunction loadI18n(){if(!LG||I18N._v==='local-complete')return;Object.assign(I18N,LOCAL_I18N.dict,{_v:'local-complete'});BUILTIN=LOCAL_I18N.builtin;i18n();}\n`+html.slice(end);
html=html.replace(/function poll\(\)\{[\s\S]*?\n(?=i18n\(\);loadI18n)/,`function poll(){if(updating)return setTimeout(poll,1000);fetch('/api/state').then(r=>r.json()).then(load).catch(()=>$('net').className='off').finally(()=>setTimeout(poll,3000))}\n`);
const output=path.join(root,'web/out');fs.mkdirSync(output,{recursive:true});fs.writeFileSync(path.join(output,'complete.html'),html);
let header='#pragma once\n#include <pgmspace.h>\n';
for(const [symbol,file] of [['COMPLETE_PANEL_GZ',null],['COMPLETE_DISPLAY_GZ','display.html']]){
 const source=file?fs.readFileSync(path.join(root,'web',file)):Buffer.from(html),gz=zlib.gzipSync(source,{level:9,mtime:0});
 header+=`static const uint8_t ${symbol}[${gz.length}] PROGMEM={\n`;
 for(let i=0;i<gz.length;i++)header+=(i%24?'':'\n')+gz[i]+',';
 header+='\n};\n';console.log(symbol+': '+gz.length+' bytes');
}
fs.writeFileSync(path.join(root,'smalltv-pomodoro/complete_panel.h'),header);
