#!/usr/bin/env node
const fs=require('node:fs'),path=require('node:path'),zlib=require('node:zlib');
const root=path.resolve(__dirname,'..');
let html=fs.readFileSync(path.join(root,'web/index.html'),'utf8');
const previews={};
for(let i=13;i<17;i++)previews[i]='data:image/png;base64,'+fs.readFileSync(path.join(root,'docs/complete/face-'+i+'.png')).toString('base64');
html=html.replace("const faceSrc=f=>f.src||",'const COMPLETE_PREVIEWS='+JSON.stringify(previews)+';\nconst faceSrc=f=>f.src||COMPLETE_PREVIEWS[f.t]||');
html=html.replaceAll('SmallTV Pomodoro','SmallTV Complete');
const dictionary=fs.readFileSync(path.join(root,'web/i18n.json'),'utf8');
// Replace the long settings page with category submenus, keeping every upstream control.
const settingsStart=html.indexOf('<section class="tab" id="t3">'),settingsEnd=html.indexOf('</section>',settingsStart);
if(settingsStart<0||settingsEnd<0)throw Error('Settings section missing');
const oldSettings=html.slice(settingsStart,settingsEnd);
const groups=oldSettings.split(' <div class="grp">').slice(1);
const categories=['language','integrations','weather','brightness','panel','time','system','diagnostics'];
if(groups.length!==categories.length)throw Error('Unexpected settings categories');
const upstreamGroups=groups.map((group,i)=>'<div class="grp" data-category="'+categories[i]+'" hidden>'+group).join('\n');
const settings=fs.readFileSync(path.join(root,'web/complete-settings.html'),'utf8').replace('{{UPSTREAM_GROUPS}}',upstreamGroups);
html=html.slice(0,settingsStart)+'<section class="tab" id="t3">'+settings+html.slice(settingsEnd);
// One display policy lives in Settings; gallery faces remain selectable in Clock.
const appearanceStart=html.indexOf(' <div class="side2">'),appearanceEnd=html.indexOf('</section>',appearanceStart);
if(appearanceStart<0||appearanceEnd<0)throw Error('Appearance section missing');
html=html.slice(0,appearanceStart)+'<p class="clock-settings-link"><a href="#settings/screens">Screen rotation, AI limits and appearance →</a></p>\n'+html.slice(appearanceEnd);
html=html.replace('</style>','\n'+fs.readFileSync(path.join(root,'web/complete-settings.css'),'utf8')+'\n</style>');
html=html.replace('Fotos são recortadas para 240×240 no seu navegador antes de enviar. GIFs vão como estão (ideal 240×240). Para exibir, escolha "Álbum" ou "Alternar" em Relógio → Aparência.','Photos are cropped to 240×240 in your browser. GIFs keep their original size. Choose the photo/GIF album in Settings → Display & screens.');

html=html.replace('let LG=0;','let LG=1;');
html=html.replace(/function checkUpdate\(\)\{[\s\S]*?\n(?=function showUpdate)/,'function checkUpdate(){}\n');
const begin=html.indexOf('function loadI18n(){'),end=html.indexOf('// chamado por load()',begin);
if(begin<0||end<0)throw Error('i18n integration point missing');
html=html.slice(0,begin)+`const LOCAL_I18N=${dictionary};\nfunction loadI18n(){if(!LG||I18N._v==='local-complete')return;Object.assign(I18N,LOCAL_I18N.dict,{_v:'local-complete'});BUILTIN=LOCAL_I18N.builtin;i18n();}\n`+html.slice(end);
html=html.replace(/function poll\(\)\{[\s\S]*?\n(?=i18n\(\);loadI18n)/,`function poll(){if(updating)return setTimeout(poll,1000);fetch('/api/state').then(r=>r.json()).then(load).catch(()=>$('net').className='off').finally(()=>setTimeout(poll,3000))}\n`);
html=html.replace('i18n();loadI18n();loadFaces();let t0=',fs.readFileSync(path.join(root,'web/complete-settings.js'),'utf8')+'\ni18n();loadI18n();loadFaces();let t0=');
html=html.replace('tab(t0);poll();','tab(t0);poll();completeRoute();settingsBootstrapped=true;');
// This fork uses explicit signed updates; do not offer upstream beta checks.
html=html.replace(/  <label>Receber versões de teste[\s\S]*?<\/small>\n/,'');
const output=path.join(root,'web/out');fs.mkdirSync(output,{recursive:true});fs.writeFileSync(path.join(output,'complete.html'),html);
let header='#pragma once\n#include <pgmspace.h>\n';
for(const [symbol,file] of [['COMPLETE_PANEL_GZ',null],['COMPLETE_DISPLAY_GZ','display.html']]){
 const source=file?fs.readFileSync(path.join(root,'web',file)):Buffer.from(html),gz=zlib.gzipSync(source,{level:9,mtime:0});
 header+=`static const uint8_t ${symbol}[${gz.length}] PROGMEM={\n`;
 for(let i=0;i<gz.length;i++)header+=(i%24?'':'\n')+gz[i]+',';
 header+='\n};\n';console.log(symbol+': '+gz.length+' bytes');
}
fs.writeFileSync(path.join(root,'smalltv-pomodoro/complete_panel.h'),header);
