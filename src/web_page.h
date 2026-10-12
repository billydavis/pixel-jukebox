#pragma once

#include <Arduino.h>

// The control page served at "/". One self-contained file: no external assets,
// so it works without an internet connection. It is a client of the REST API.
static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#141a2e">
<title>Pixel Jukebox</title>
<style>
:root{
  --bg:oklch(0.22 0.04 270);--panel:oklch(0.28 0.05 270);--edge:oklch(0.15 0.03 270);
  --ink:oklch(0.94 0.015 270);--dim:oklch(0.74 0.04 270);
  --accent:oklch(0.84 0.13 170);--accent-ink:oklch(0.27 0.05 180);
  --held:oklch(0.58 0.2 355);--led:oklch(0.14 0.02 270);
  --mono:ui-monospace,"Cascadia Mono","SF Mono",Menlo,Consolas,monospace;
  --c:clamp(3px,calc((100vw - 52px)/64),6px);
}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--ink);
  font:16px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif;
  display:grid;place-items:start center;padding:20px 16px 28px}
main{display:grid;gap:22px;justify-items:center;width:100%;max-width:900px}
h1{margin:0;font:700 14px/1 var(--mono);letter-spacing:.28em;text-transform:uppercase;color:var(--accent)}
.top{display:flex;justify-content:space-between;align-items:baseline;gap:16px;width:100%}
.link{font:12px/1 var(--mono);letter-spacing:.08em;text-transform:uppercase;color:var(--dim)}
.off .link{color:var(--ink)}
.bezel{padding:10px;background:var(--panel);box-shadow:6px 6px 0 var(--edge)}
.led{position:relative;width:calc(var(--c)*64);height:calc(var(--c)*64);background:var(--led);
  display:grid;place-items:center;overflow:hidden}
.led img{position:absolute;inset:0;width:100%;height:100%;image-rendering:pixelated;object-fit:contain}
.led p{margin:0;padding:0 calc(var(--c)*4);text-align:center;font:700 calc(var(--c)*5)/1.25 var(--mono);color:var(--dim)}
.led p.clock{font-size:calc(var(--c)*13);color:var(--accent)}
.led::after{content:"";position:absolute;inset:0;pointer-events:none;
  background:radial-gradient(circle,transparent 0 40%,var(--led) 58%) 0 0/var(--c) var(--c)}
.pause{position:absolute;top:calc(var(--c)*1);right:calc(var(--c)*1);display:none;gap:var(--c)}
.pause i{width:calc(var(--c)*2);height:calc(var(--c)*5);background:var(--ink)}
.held .pause{display:flex}
.side{display:grid;gap:22px;width:100%;max-width:calc(var(--c)*64 + 20px)}
.who{margin:0;font-size:24px;font-weight:700;line-height:1.15;overflow-wrap:anywhere}
.sub{margin:4px 0 0;font:13px/1.3 var(--mono);color:var(--dim)}
.keys{display:grid;grid-template-columns:1fr 1fr;gap:14px}
button{font:700 15px/1 var(--mono);letter-spacing:.14em;text-transform:uppercase;
  min-height:56px;border:0;border-radius:0;cursor:pointer;color:var(--ink);background:var(--panel);
  box-shadow:5px 5px 0 var(--edge);transition:transform 80ms,box-shadow 80ms}
button.go{background:var(--accent);color:var(--accent-ink)}
button:active,button[aria-pressed=true]{transform:translate(5px,5px);box-shadow:0 0 0 var(--edge)}
button[aria-pressed=true]{background:var(--held)}
button:disabled{opacity:.45;cursor:default;transform:none;box-shadow:5px 5px 0 var(--edge)}
:focus-visible{outline:3px solid var(--accent);outline-offset:4px}
.dim{display:grid;gap:10px}
.dim label{display:flex;justify-content:space-between;font:13px/1 var(--mono);letter-spacing:.14em;text-transform:uppercase;color:var(--dim)}
.dim output{color:var(--ink)}
input[type=range]{appearance:none;-webkit-appearance:none;width:100%;height:44px;margin:0;background:none;cursor:pointer}
input[type=range]::-webkit-slider-runnable-track{height:12px;background:linear-gradient(to right,var(--accent) var(--v,12%),var(--edge) 0)}
input[type=range]::-moz-range-track{height:12px;background:var(--edge)}
input[type=range]::-moz-range-progress{height:12px;background:var(--accent)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:32px;margin-top:-10px;background:var(--ink);box-shadow:3px 3px 0 var(--edge)}
input[type=range]::-moz-range-thumb{width:22px;height:32px;border:0;border-radius:0;background:var(--ink);box-shadow:3px 3px 0 var(--edge)}
input[type=range]:disabled{opacity:.45}
.rot summary{font:13px/1 var(--mono);letter-spacing:.14em;text-transform:uppercase;color:var(--dim);
  cursor:pointer;min-height:44px;display:flex;align-items:center;gap:8px}
.rot ol{list-style:none;margin:6px 0 14px;padding:0;display:grid;gap:10px}
.rot li{display:flex;align-items:center;gap:6px}
.rot .nm{flex:1;min-width:0;font-size:15px;font-weight:700;line-height:1.2}
.rot .nm small{display:block;margin-top:3px;font:400 12px/1.3 var(--mono);color:var(--dim)}
button.sm{min-height:44px;min-width:44px;padding:0 10px;font-size:17px;letter-spacing:0;box-shadow:3px 3px 0 var(--edge)}
button.sm:active{transform:translate(3px,3px)}
button.sm:disabled{box-shadow:3px 3px 0 var(--edge)}
.add{display:flex;gap:10px}
select{flex:1;min-width:0;min-height:44px;padding:0 10px;border:0;border-radius:0;
  font:15px system-ui,-apple-system,"Segoe UI",sans-serif;color:var(--ink);background:var(--panel)}
.note.wifi{margin:0 0 14px}
.note{margin:10px 0 0;min-height:1.4em;font:12px/1.4 var(--mono);color:var(--dim)}
footer{font:12px/1.6 var(--mono);color:var(--dim);text-align:center;overflow-wrap:anywhere}
kbd{font:inherit;color:var(--ink)}
.hint{display:none}
@media (min-width:820px){
  :root{--c:clamp(4px,calc((100vw - 460px)/64),7px)}
  body{place-items:center}
  main{grid-template-columns:auto minmax(260px,340px);justify-items:start;align-items:center;column-gap:48px}
  .top,footer{grid-column:1/-1}
  footer{text-align:left}
  .hint{display:inline}
}
@media (prefers-reduced-motion:reduce){button{transition:none}}
</style>
</head>
<body>
<main>
  <div class="top">
    <h1>Pixel Jukebox</h1>
    <span class="link" id="link" role="status">Connecting</span>
  </div>
  <div class="bezel">
    <div class="led" id="led">
      <img id="art" alt="" hidden>
      <p id="msg">Starting up</p>
      <span class="pause" aria-hidden="true"><i></i><i></i></span>
    </div>
  </div>
  <div class="side">
    <div>
      <p class="who" id="who">&nbsp;</p>
      <p class="sub" id="sub">&nbsp;</p>
    </div>
    <div class="keys">
      <button id="hold" aria-pressed="false" disabled>Hold</button>
      <button id="next" class="go" disabled>Next</button>
    </div>
    <div class="dim">
      <label for="bri"><span>Brightness</span><output id="briv">--</output></label>
      <input id="bri" type="range" min="0" max="255" step="1" value="32" disabled>
    </div>
    <details class="rot" id="rot">
      <summary>Rotation</summary>
      <ol id="rows"></ol>
      <div class="add">
        <select id="kind" aria-label="Slot to add"></select>
        <button id="add" class="sm">Add</button>
      </div>
      <p class="note" id="rotnote" role="status"></p>
    </details>
    <details class="rot">
      <summary>Wi-Fi</summary>
      <p class="note wifi" id="wifinote" role="status">&nbsp;</p>
      <button id="wifi" class="sm">Change Wi-Fi</button>
    </details>
  </div>
  <footer><span id="info">&nbsp;</span><span class="hint"> &middot; keys: <kbd>N</kbd> next, <kbd>H</kbd> hold</span></footer>
</main>
<script>
const $=id=>document.getElementById(id),API='/api/v1';
let st=null,timer=0,dragging=false,sendTimer=0;

async function call(method,path,body){
  const r=await fetch(API+path,{method,headers:body?{'Content-Type':'application/json'}:{},body:body?JSON.stringify(body):undefined});
  if(!r.ok)throw new Error(r.status);
  return r.json();
}

function uptime(s){
  const h=Math.floor(s/3600),m=Math.floor(s%3600/60);
  return h?h+'h '+m+'m':m?m+'m':s+'s';
}

function render(online){
  document.body.classList.toggle('off',!online);
  $('link').textContent=online?'Live':st?'Lost contact, retrying':'Connecting';
  for(const id of['hold','next','bri'])$(id).disabled=!online;
  if(!online||!st)return;

  const art=st.artwork,slot=st.slot.name,img=$('art'),msg=$('msg');
  if(art){
    if(img.dataset.post!=art.postId){img.dataset.post=art.postId;img.src=art.gifUrl;}
    img.alt='Artwork by '+art.owner;
    img.hidden=false;msg.hidden=true;
    $('who').textContent=art.owner?'@'+art.owner:'Unknown artist';
    $('sub').textContent='Makapix post '+art.postId;
  }else{
    img.hidden=true;img.removeAttribute('src');delete img.dataset.post;msg.hidden=false;
    const clock=slot=='clock';
    msg.className=clock?'clock':'';
    msg.textContent=clock?new Date().toLocaleTimeString([],{hour:'numeric',minute:'2-digit'}).replace(/\s?[AP]M/i,''):
      slot=='makapix'?'Code is on the panel':'Nothing showing';
    $('who').textContent=clock?'Clock':slot=='makapix'?'Waiting to be linked':'Between pieces';
    $('sub').textContent=clock?'Back to the art in a moment':slot=='makapix'?'Enter the code at makapix.club':' ';
  }

  const held=st.slot.held;
  $('led').classList.toggle('held',held);
  $('hold').setAttribute('aria-pressed',held);
  $('hold').textContent=held?'Held':'Hold';

  if(!dragging)showBrightness(st.brightness);
  const d=st.device;
  if(!wifiArmed)$('wifinote').textContent='Connected to '+d.ssid+'.';
  $('info').textContent=d.name+'.local · v'+d.firmware+' · '+d.ip+' · '+d.rssi+' dBm · up '+uptime(d.uptimeSeconds);
}

function showBrightness(v){
  $('bri').value=v;$('briv').textContent=v;
  $('bri').style.setProperty('--v',v/255*100+'%');
}

async function poll(){
  clearTimeout(timer);
  try{st=await call('GET','/status');render(true);}catch(e){render(false);}
  if(!document.hidden)timer=setTimeout(poll,2000);
}

async function act(method,path,body){
  try{await call(method,path,body);}catch(e){}
  clearTimeout(timer);timer=setTimeout(poll,200);
}

$('next').onclick=()=>act('POST','/next');
$('hold').onclick=()=>{
  const held=!(st&&st.slot.held);
  if(st){st.slot.held=held;render(true);}
  act('POST','/hold',{held});
};
$('bri').oninput=e=>{
  dragging=true;
  const v=+e.target.value;showBrightness(v);
  clearTimeout(sendTimer);sendTimer=setTimeout(()=>call('PUT','/brightness',{value:v}).catch(()=>{}),120);
};
$('bri').onchange=e=>{
  dragging=false;clearTimeout(sendTimer);
  act('PUT','/brightness',{value:+e.target.value});
};
let wifiArmed=0;
$('wifi').onclick=()=>{
  if(!wifiArmed){
    wifiArmed=setTimeout(()=>{wifiArmed=0;$('wifi').textContent='Change Wi-Fi';},6000);
    $('wifi').textContent='Tap again to confirm';
    $('wifinote').textContent='The panel will restart and offer its own network, Pixel-Jukebox, for setup.';
    return;
  }
  clearTimeout(wifiArmed);
  $('wifi').disabled=true;$('wifi').textContent='Restarting';
  $('wifinote').textContent='Join the Pixel-Jukebox network on your phone to choose a new network.';
  call('POST','/wifi/setup').catch(()=>{});
};

let rot=null;
const copy=()=>rot.rotation.map(r=>({...r}));

async function loadRot(){
  try{rot=await call('GET','/slots');drawRot();}catch(e){$('rotnote').textContent='Could not load the rotation.';}
}

function drawRot(){
  const rows=$('rows'),focus=document.activeElement&&document.activeElement.dataset.k;
  rows.textContent='';
  rot.rotation.forEach((r,i)=>{
    const a=rot.available.find(x=>x.name==r.slot)||{label:r.slot,turn:'turn'};
    const li=document.createElement('li'),nm=document.createElement('span'),sm=document.createElement('small');
    nm.className='nm';nm.textContent=a.label;
    sm.textContent=r.turns+' × '+a.turn;nm.appendChild(sm);li.appendChild(nm);
    const btn=(j,text,label,off,fn)=>{
      const b=document.createElement('button');
      b.className='sm';b.textContent=text;b.dataset.k=i+':'+j;b.disabled=off;b.onclick=fn;
      b.setAttribute('aria-label',label+' '+a.label);li.appendChild(b);
    };
    btn(0,'−','Fewer turns for',r.turns<=1,()=>{const l=copy();l[i].turns--;saveRot(l);});
    btn(1,'+','More turns for',r.turns>=rot.maxTurns,()=>{const l=copy();l[i].turns++;saveRot(l);});
    btn(2,'↑','Move up',i==0,()=>{const l=copy();[l[i-1],l[i]]=[l[i],l[i-1]];saveRot(l);});
    btn(3,'×','Remove',rot.rotation.length<=1,()=>{const l=copy();l.splice(i,1);saveRot(l);});
    rows.appendChild(li);
  });
  const kind=$('kind');
  if(!kind.options.length)rot.available.forEach(a=>kind.add(new Option(a.label,a.name)));
  $('add').disabled=rot.rotation.length>=rot.maxEntries;
  const again=focus&&rows.querySelector('[data-k="'+focus+'"]');
  if(again&&!again.disabled)again.focus();
}

async function saveRot(list){
  try{
    rot=await call('PUT','/slots',{rotation:list});
    $('rotnote').textContent='Saved. Starts after the current piece.';
    drawRot();
  }catch(e){
    $('rotnote').textContent='Could not save. Try again.';
    loadRot();
  }
}

$('add').onclick=()=>{const l=copy();l.push({slot:$('kind').value,turns:1});saveRot(l);};
$('rot').ontoggle=()=>{if($('rot').open)loadRot();};
addEventListener('keydown',e=>{
  if(e.target.tagName=='INPUT'||e.target.tagName=='SELECT'||e.ctrlKey||e.metaKey||e.altKey)return;
  const k=e.key.toLowerCase();
  if(k=='n')$('next').click();
  if(k=='h')$('hold').click();
});
document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll();});
poll();
</script>
</body>
</html>
)HTML";
