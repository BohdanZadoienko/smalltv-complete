// Settings navigation and display controls share the main panel's theme and request guards.
const SETTINGS_CATEGORIES=[
 ['screens','Display & screens','Screen choice, rotation and AI usage','<rect x="3" y="4" width="18" height="14" rx="3"/><path d="M8 21h8M12 18v3M7 9h4M7 13h9"/>'],
 ['weather','Weather & location','City, temperature and rain alerts','<path d="M7 16H6a4 4 0 1 1 2-7 5 5 0 0 1 10 2 3 3 0 0 1 0 6h-2M9 19v2M13 18v3"/>'],
 ['brightness','Brightness & night','Daylight, schedules and warm colors','<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M5 5l1.5 1.5M17.5 17.5L19 19M5 19l1.5-1.5M17.5 6.5L19 5"/>'],
 ['time','Date & time','Time zone and daylight saving','<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>'],
 ['language','Language','Panel and clock language','<path d="M3 5h12M9 3v2M5 5c1 5 5 8 9 10M13 5c-1 5-5 8-9 10M14 21l4-10 4 10M15 18h6"/>'],
 ['integrations','Alexa & notifications','Voice controls and timer announcements','<path d="M8 10V7a4 4 0 0 1 8 0v3M5 10v2a7 7 0 0 0 14 0v-2M12 19v3M8 22h8"/>'],
 ['panel','Screen calibration','Color, orientation and panel tuning','<rect x="3" y="3" width="18" height="18" rx="3"/><path d="M7 8h10M7 12h10M7 16h10M10 6v4M14 10v4M10 14v4"/>'],
 ['system','Device & system','Password, updates and backups','<path d="M12 3v9M7 5a9 9 0 1 0 10 0"/>'],
 ['diagnostics','Diagnostics','Wi-Fi, memory and restart history','<path d="M3 12h4l3-8 4 16 3-8h4"/>']
];
let settingsBootstrapped=false;
let settingsCategory='',displayLoaded=false,displayDirty=false,displayUsage=null;
const displayNames=['Matrix','Tomato','Analog','Weather','Neon','Flip','Retro LCD','Words','5-day forecast','Forecast cards','Forecast LCD','World clock','World watch','Claude Code','OpenAI Codex','Combined dashboard','Classic weather clock'];
for(const [id,title,description,icon] of SETTINGS_CATEGORIES){
 const button=document.createElement('button');button.type='button';button.className='settings-category';
 button.innerHTML='<svg viewBox="0 0 24 24" aria-hidden="true">'+icon+'</svg><span><b></b><small></small></span><i aria-hidden="true">›</i>';
 button.querySelector('b').textContent=title;button.querySelector('small').textContent=description;button.onclick=()=>settingsOpen(id);$('settingsMenu').append(button);
}
for(const item of displayNames.map((name,id)=>({name,id})).concat([{name:'Photo / GIF album',id:200}])){
 const option=document.createElement('option');option.value=item.id;option.textContent=item.name;$('displayFixed').append(option);
 const label=document.createElement('label'),box=document.createElement('input');box.type='checkbox';box.value=item.id;label.append(document.createTextNode(item.name),box);$('displayScreens').append(label);
}
const completeOriginalTab=tab;
function settingsRoute(hash){history.replaceState(null,'',location.pathname+location.search+hash)}
function settingsOpen(id,route=true){
 const category=SETTINGS_CATEGORIES.find(x=>x[0]===id);if(!category)return settingsBack(route);
 completeOriginalTab(3);settingsCategory=id;$('settingsHome').hidden=true;$('settingsDetail').hidden=false;
 $('settingsTitle').textContent=category[1];$('settingsDescription').textContent=category[2];
 document.querySelectorAll('#settingsPanels>.grp').forEach(x=>x.hidden=x.dataset.category!==id);
 if(route)settingsRoute('#settings/'+id);
 if(id==='screens'){if(!displayDirty)displayLoad();displayRefreshUsage()}
 window.scrollTo(0,0);
}
function settingsBack(route=true){
 completeOriginalTab(3);settingsCategory='';$('settingsHome').hidden=false;$('settingsDetail').hidden=true;
 if(route)settingsRoute('#settings');window.scrollTo(0,0);
}
tab=function(i){completeOriginalTab(i);if(!settingsBootstrapped)return;if(i===3)settingsBack();else if(location.hash.startsWith('#settings'))settingsRoute('')};
function completeRoute(){const match=location.hash.match(/^#settings(?:\/([a-z]+))?$/);if(match){if(match[1])settingsOpen(match[1],false);else settingsBack(false)}}
window.addEventListener('hashchange',completeRoute);
function displayBehaviour(){$('displayFixedRow').hidden=$('displayPolicy').value!=='fixed';$('displayRotationRow').hidden=$('displayPolicy').value!=='rotate'}
function displayMessage(text,error=false){$('displayMessage').textContent=text;$('displayMessage').className='display-message'+(error?' error':'')}
async function displayLoad(){
 try{
  const response=await fetch('/api/display');if(!response.ok)throw Error('Could not load display settings.');const d=await response.json();
  if(displayDirty)return;
  $('displayPolicy').value=d.rotate?'rotate':'fixed';$('displayFixed').value=d.screens[0];$('displayDelay').value=d.delay;
  $('displayAlbumAuto').checked=d.albumAuto;$('displayRemaining').checked=!!d.remaining;
  document.querySelectorAll('#displayScreens input').forEach(x=>x.checked=d.screens.includes(+x.value));
  document.querySelectorAll('#displayParts input').forEach(x=>x.checked=!!(d.parts&+x.value));
  const filesResponse=await fetch('/api/files');if(!filesResponse.ok)throw Error('Could not load uploaded animations.');const files=await filesResponse.json();
  $('displayWeatherGif').replaceChildren(new Option('Weather icon',''));
  for(const file of files.files||[])if(file.n.toLowerCase().endsWith('.gif'))$('displayWeatherGif').append(new Option(file.n.split('/').pop(),file.n));
  if(d.weatherGif&&!Array.from($('displayWeatherGif').options).some(x=>x.value===d.weatherGif))$('displayWeatherGif').append(new Option(d.weatherGif.split('/').pop()+' (missing)',d.weatherGif));
  $('displayWeatherGif').value=d.weatherGif||'';displayLoaded=true;$('displayFields').disabled=false;$('displaySave').disabled=false;displayBehaviour();displayRenderUsage();
  displayMessage(d.enabled?'':'A face is active from Clock. Save here to activate these display selections.');
 }catch(error){displayMessage(error.message,true)}
}
$('displayPolicy').onchange=displayBehaviour;
$('displayForm').addEventListener('change',()=>{displayDirty=true;if(displayLoaded)displayMessage('Unsaved display changes.');displayRenderUsage()});
function displayRenderUsage(){
 const remaining=$('displayRemaining').checked;
 $('displayMetricLabel').textContent='Percentages show capacity '+(remaining?'remaining.':'used.');
 for(const [key,id] of [['claude','displayClaude'],['codex','displayCodex']]){
  const u=displayUsage&&displayUsage[key];
  $(id).textContent=u&&u.fresh?[['s','5 hours'],['w','Weekly']].map(([field,name])=>name+': '+(u[field]<0?'unavailable':(remaining?100-u[field]:u[field])+'% '+(remaining?'left':'used'))).join('\n'):'Waiting for fresh Mac usage';
 }
}
async function displayRefreshUsage(){try{const response=await fetch('/api/status');if(response.ok){displayUsage=await response.json();displayRenderUsage()}}catch(error){}}
setInterval(()=>{if(curTab===3&&settingsCategory==='screens')displayRefreshUsage()},5000);
$('displayForm').onsubmit=async event=>{
 event.preventDefault();if(!displayLoaded)return;
 const rotate=$('displayPolicy').value==='rotate',screens=rotate?Array.from(document.querySelectorAll('#displayScreens input:checked'),x=>+x.value):[+$('displayFixed').value];
 const parts=Array.from(document.querySelectorAll('#displayParts input:checked')).reduce((a,x)=>a|+x.value,0);
 if(!screens.length||!parts){displayMessage('Choose at least one screen and one combined-screen component.',true);return}
 $('displaySave').disabled=true;
 try{
  const response=await fetch('/api/display',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rotate,screens,delay:+$('displayDelay').value,parts,albumAuto:$('displayAlbumAuto').checked,weatherGif:$('displayWeatherGif').value,remaining:$('displayRemaining').checked})});
  if(!response.ok)throw Error(await response.text());displayDirty=false;displayMessage('Saved. Your SmallTV is using these display settings.');
 }catch(error){displayMessage(error.message,true)}finally{$('displaySave').disabled=false}
};
