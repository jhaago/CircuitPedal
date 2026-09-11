from pathlib import Path
import re

p = Path('ui-prototype/app.html')
s = p.read_text()

extra_css = r'''
/* Full signal-routing editor */
.center.editing{grid-template-rows:minmax(0,1fr)}
.center.editing>.chain,.center.editing>.hero{display:none}
.routing-editor{display:none;min-height:0;border:1px solid var(--line);border-radius:13px;background:#090e14;box-shadow:var(--shadow);overflow:hidden;position:relative}
.center.editing>.routing-editor{display:grid;grid-template-rows:48px minmax(0,1fr)}
.route-editor-bar{display:flex;align-items:center;justify-content:space-between;padding:0 12px;border-bottom:1px solid var(--line);background:linear-gradient(180deg,#101821,#0b1118)}
.route-editor-title{display:flex;align-items:center;gap:10px}.route-editor-title strong{font-size:10px;letter-spacing:1.1px}.route-editor-title span{font-size:7px;color:#64727f;letter-spacing:.6px}
.route-editor-actions{display:flex;gap:6px}.route-editor-actions button{height:29px;border:1px solid var(--line);border-radius:7px;background:#0b1219;color:#7d8b97;padding:0 10px;font-size:7px}.route-editor-actions button.done{border-color:#6e352d;color:#ef9b89}
.route-canvas{position:relative;overflow:hidden;background-color:#080d12;background-image:linear-gradient(rgba(92,120,140,.075) 1px,transparent 1px),linear-gradient(90deg,rgba(92,120,140,.075) 1px,transparent 1px),linear-gradient(rgba(92,120,140,.026) 1px,transparent 1px),linear-gradient(90deg,rgba(92,120,140,.026) 1px,transparent 1px);background-size:48px 48px,48px 48px,12px 12px,12px 12px}
.route-canvas:after{content:"DRAG NODES  ·  TAP OUT THEN IN TO DRAW CABLES  ·  TAP A CABLE TO REMOVE";position:absolute;left:14px;bottom:10px;font-size:7px;letter-spacing:1px;color:#3f5262;pointer-events:none}
.route-svg{position:absolute;inset:0;width:100%;height:100%;z-index:1;overflow:visible}.route-wire{fill:none;stroke:#668da5;stroke-width:3;filter:drop-shadow(0 0 4px rgba(100,185,231,.18))}.route-wire-hit{fill:none;stroke:transparent;stroke-width:16;pointer-events:stroke;cursor:pointer}
.route-node{--pc1:#29343c;--pc2:#0c1115;position:absolute;z-index:3;width:108px;height:112px;margin:-56px 0 0 -54px;border:1px solid rgba(255,255,255,.16);border-radius:12px;background:linear-gradient(145deg,var(--pc1),var(--pc2));box-shadow:inset 0 1px rgba(255,255,255,.1),0 12px 22px rgba(0,0,0,.5);touch-action:none;color:#d8e0e6;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px}.route-node.selected{border-color:#ff8c6f;box-shadow:0 0 0 2px rgba(230,92,67,.4),0 0 20px rgba(230,92,67,.27),0 12px 22px rgba(0,0,0,.5)}.route-node.io{width:82px;height:58px;margin:-29px 0 0 -41px;background:linear-gradient(145deg,#1b252d,#0a0f13);border-radius:29px}.route-node strong{font-size:9px;letter-spacing:.8px}.route-node small{font-size:6px;letter-spacing:1.1px;color:#778692}.route-node .node-knobs{display:flex;gap:8px}.route-node .node-knobs i{width:18px;height:18px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#72797d,#262c30 48%,#080b0d 71%);border:1px solid #767e83}.route-node .port{position:absolute;top:50%;width:17px;height:17px;margin-top:-8px;border-radius:50%;border:2px solid #778a98;background:#0a0f13;box-shadow:0 0 0 3px rgba(8,13,18,.85);z-index:5}.route-node .port.in{left:-10px}.route-node .port.out{right:-10px}.route-node .port.armed{border-color:#ff8c6f;background:#7a2d22;box-shadow:0 0 11px rgba(230,92,67,.7)}.route-node.io.input-node .port.in,.route-node.io.output-node .port.out{display:none}.route-node .node-remove{position:absolute;right:5px;top:4px;border:0;background:none;color:#657684;font-size:13px;line-height:1}.route-node.io .node-remove{display:none}
.route-editor-empty{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);z-index:0;color:#445462;font-size:9px;letter-spacing:1px}
.chain-tools .route-summary-top{font-size:7px;color:#62717e;letter-spacing:.8px;margin-right:3px}.mini-pedal .remove-pedal,.mini-pedal .lane-toggle{display:none}
/* Master I/O controls */
.bottom{grid-template-columns:265px minmax(180px,1fr) 96px minmax(180px,1fr) 265px}
.master-meter{display:grid;grid-template-columns:72px minmax(70px,1fr);gap:10px;align-items:center}.master-meter.output{grid-template-columns:minmax(70px,1fr) 72px}.master-control{display:flex;flex-direction:column;align-items:center;gap:2px}.master-knob{--value:50;width:45px;height:45px;border-radius:50%;position:relative;background:radial-gradient(circle at 36% 30%,#777b7e,#343a3d 36%,#15191c 67%,#050607 71%);border:1px solid #7d858a;box-shadow:0 6px 10px rgba(0,0,0,.45),inset 0 1px rgba(255,255,255,.2);touch-action:none}.master-knob:after{content:"";position:absolute;left:21px;top:5px;width:2px;height:15px;border-radius:2px;background:#eee7dc;transform:rotate(calc(-132deg + var(--value)*2.64deg));transform-origin:1px 18px}.master-control span{font-size:6px;color:#75828e;letter-spacing:.8px}.master-control b{font-size:7px;color:#b3bec7;font-weight:500}.meter-stack{display:grid;grid-template-columns:auto 1fr;gap:7px;align-items:center}.meter-stack.output{grid-template-columns:1fr auto}.meter-stack .meter-name{font-size:7px;color:#687580}.meter-stack.output .meter-name{text-align:right}.master-meter .vmeters{height:43px}.master-meter .meter-db{font-size:7px;color:#6f7c88;display:block;margin-top:3px}
@media(max-width:1180px){.bottom{grid-template-columns:220px 1fr 78px 1fr 220px}.master-meter{grid-template-columns:60px 1fr}.master-meter.output{grid-template-columns:1fr 60px}.master-knob{width:40px;height:40px}.master-knob:after{left:18px;top:4px;height:14px;transform-origin:1px 17px}.route-node{width:94px;height:100px;margin:-50px 0 0 -47px}}
@media(max-height:700px){.master-knob{width:38px;height:38px}.master-knob:after{left:17px;top:4px;height:12px;transform-origin:1px 15px}.center.editing>.routing-editor{grid-template-rows:42px minmax(0,1fr)}}
'''
s = s.replace('</style>', extra_css + '\n</style>', 1)
s = s.replace('<section class="center">', '<section class="center" id="center">', 1)

old_head = '<div class="chain-head"><strong>☷ &nbsp; SIGNAL CHAIN</strong><div class="chain-tools"><button class="primary" id="addPedal">＋ ADD PEDAL</button><span class="route-label">ROUTING</span><select id="routing"><option value="series">Series</option><option value="parallel">Parallel A / B</option></select></div></div>'
new_head = '<div class="chain-head"><strong>☷ &nbsp; SIGNAL CHAIN</strong><div class="chain-tools"><span class="route-summary-top" id="routeSummaryTop">CUSTOM ROUTING · 6 PEDALS</span><button class="primary" id="editChain">✎ EDIT SIGNAL CHAIN</button></div></div>'
if old_head not in s:
    raise SystemExit('chain header anchor not found')
s = s.replace(old_head, new_head, 1)

anchor = '        <div class="art-note">BEAUTY<br>IN<br>DISTORTION<br>—</div>\n      </section>\n    </section>\n\n    <aside class="right">'
editor = r'''        <div class="art-note">BEAUTY<br>IN<br>DISTORTION<br>—</div>
      </section>
      <section class="routing-editor" id="routingEditor">
        <div class="route-editor-bar">
          <div class="route-editor-title"><strong>SIGNAL CHAIN EDITOR</strong><span>FREEFORM ROUTING CANVAS</span></div>
          <div class="route-editor-actions"><button id="routeLibrary">PEDAL LIBRARY</button><button id="routeReset">RESET LAYOUT</button><button id="routeClear">CLEAR CABLES</button><button class="done" id="routeDone">DONE</button></div>
        </div>
        <div class="route-canvas" id="routeCanvas"><svg class="route-svg" id="routeSvg"></svg><div id="routeNodes"></div><div class="route-editor-empty" id="routeEmpty">Build the signal path by connecting pedal ports</div></div>
      </section>
    </section>

    <aside class="right">'''
if anchor not in s:
    raise SystemExit('hero/editor insertion anchor not found')
s = s.replace(anchor, editor, 1)

bottom_pat = re.compile(r'  <footer class="bottom">.*?  </footer>', re.S)
bottom_new = r'''  <footer class="bottom">
    <div class="master-meter input">
      <div class="master-control"><div class="master-knob" id="inputTrim" data-min="-18" data-max="18" data-value="50" style="--value:50"></div><span>INPUT TRIM</span><b id="inputTrimRead">0.0 dB</b></div>
      <div class="meter-stack"><span class="meter-name">INPUT</span><div><div class="vmeters"><i class="vm" id="in1"></i><i class="vm" id="in2"></i></div><span class="meter-db" id="inDb">-6.2 dB</span></div></div>
    </div>
    <div class="route-summary"><span>SIGNAL PATH</span><br><b id="routeSummary">CUSTOM · 6 PEDALS</b></div>
    <div class="bypass-wrap"><button class="bypassbtn" id="bypass"></button><span>BYPASS ALL</span></div>
    <div class="bottom-context"><strong id="bottomContext">RUST · FUZZ</strong><span id="bottomPreset">01 · VELVET COLLAPSE</span></div>
    <div class="master-meter output">
      <div class="meter-stack output"><div><div class="vmeters"><i class="vm" id="out1"></i><i class="vm" id="out2"></i></div><span class="meter-db" id="outDb">-6.0 dB</span></div><span class="meter-name">OUTPUT</span></div>
      <div class="master-control"><div class="master-knob" id="outputLevel" data-min="-18" data-max="6" data-value="75" style="--value:75"></div><span>OUTPUT LEVEL</span><b id="outputLevelRead">0.0 dB</b></div>
    </div>
  </footer>'''
s, n = bottom_pat.subn(bottom_new, s, count=1)
if n != 1:
    raise SystemExit('bottom replacement failed')

old_state = "let presetIndex=0,selectedEffect=2,leftTab='presets',bypassed=false,routingMode='series';\nlet chain=[{id:'tuner',lane:0},{id:'comp',lane:0},{id:'rust',lane:0},{id:'mod',lane:0},{id:'delay',lane:0},{id:'reverb',lane:0}];"
new_state = "let presetIndex=0,selectedEffect=2,leftTab='presets',bypassed=false,editChainMode=false,pendingPort=null,inputTrimDb=0,outputLevelDb=0;\nlet chain=[{id:'tuner'},{id:'comp'},{id:'rust'},{id:'mod'},{id:'delay'},{id:'reverb'}];\nlet routeNodes={INPUT:{x:7,y:50,io:true},tuner:{x:20,y:50},comp:{x:34,y:50},rust:{x:48,y:37},mod:{x:48,y:67},delay:{x:65,y:50},reverb:{x:78,y:50},OUTPUT:{x:92,y:50,io:true}};\nlet routeEdges=[['INPUT','tuner'],['tuner','comp'],['comp','rust'],['comp','mod'],['rust','delay'],['mod','delay'],['delay','reverb'],['reverb','OUTPUT']];"
if old_state not in s:
    raise SystemExit('state anchor not found')
s = s.replace(old_state, new_state, 1)

chain_block = re.compile(r"function toggleLibraryPedal\(id\)\{.*?\nfunction controlHtml", re.S)
chain_new = r'''function defaultRoutePosition(id){const used=Object.values(routeNodes).filter(n=>!n.io).length;return{x:22+(used%5)*14,y:32+((used%2)*34)}}
function toggleLibraryPedal(id){const i=chain.findIndex(c=>c.id===id);if(i>=0){if(chain.length===1)return toast('Keep at least one pedal in the chain');chain.splice(i,1);delete routeNodes[id];routeEdges=routeEdges.filter(e=>!e.includes(id));toast(`${effect(id).name} removed`)}else{chain.push({id});routeNodes[id]=defaultRoutePosition(id);toast(`${effect(id).name} added — connect it in the routing canvas`)}renderChain();renderBrowser();if(editChainMode)renderRoutingEditor()}
function miniHtml(item,index){const e=effect(item.id),ctrl=e.controls.slice(0,2);return `<button class="mini-pedal ${effects[selectedEffect].id===e.id?'active':''}" data-chain-id="${e.id}" style="--pc1:${e.c1};--pc2:${e.c2}"><div class="mini-knobs">${ctrl.map(()=>'<i class="mk"></i>').join('')}</div><div class="sig">${e.id==='tuner'?'│':e.type==='FUZZ'?'◌':e.type==='MOD'?'∿':e.type==='DELAY'?'···':'△'}</div><strong>${e.name}</strong><i class="fs"></i></button>`}
function renderChain(){$('#chainStage').className='chain-stage series';$('#laneA').innerHTML=chain.length?chain.map((c,i)=>miniHtml(c,i)).join(''):'<span class="chain-empty">Open Edit Signal Chain to add pedals</span>';$('#laneB').innerHTML='';const branchCount=routeEdges.reduce((m,e)=>(m[e[0]]=(m[e[0]]||0)+1,m),{});const split=Object.values(branchCount).some(v=>v>1);const label=`${split?'CUSTOM / SPLIT':'CUSTOM'} · ${chain.length} PEDAL${chain.length===1?'':'S'}`;$('#routeSummary').textContent=label;$('#routeSummaryTop').textContent=label;$$('[data-chain-id]').forEach(b=>b.onclick=()=>selectEffect(effects.findIndex(x=>x.id===b.dataset.chainId)));if(editChainMode)renderRoutingEditor()}
function routeNodeHtml(id,n){if(n.io){const input=id==='INPUT';return `<div class="route-node io ${input?'input-node':'output-node'}" data-route-node="${id}" style="left:${n.x}%;top:${n.y}%"><strong>${id}</strong>${input?'':'<i class="port in" data-port-in="OUTPUT"></i>'}${input?'<i class="port out" data-port-out="INPUT"></i>':''}</div>`}const e=effect(id);if(!e)return'';return `<div class="route-node ${effects[selectedEffect].id===id?'selected':''}" data-route-node="${id}" style="left:${n.x}%;top:${n.y}%;--pc1:${e.c1};--pc2:${e.c2}"><button class="node-remove" data-route-remove="${id}">×</button><i class="port in" data-port-in="${id}"></i><i class="port out" data-port-out="${id}"></i><div class="node-knobs">${e.controls.slice(0,2).map(()=>'<i></i>').join('')}</div><strong>${e.name}</strong><small>${e.type}</small></div>`}
function renderRoutingEditor(){if(!editChainMode)return;$('#routeNodes').innerHTML=Object.entries(routeNodes).map(([id,n])=>routeNodeHtml(id,n)).join('');$('#routeEmpty').style.display=chain.length?'none':'block';wireRouteNodes();requestAnimationFrame(drawRouteEdges)}
function drawRouteEdges(){const canvas=$('#routeCanvas'),svg=$('#routeSvg');if(!canvas||!svg)return;const cr=canvas.getBoundingClientRect();let html='';routeEdges.forEach((edge,i)=>{const a=canvas.querySelector(`[data-route-node="${edge[0]}"] .port.out`),b=canvas.querySelector(`[data-route-node="${edge[1]}"] .port.in`);if(!a||!b)return;const ar=a.getBoundingClientRect(),br=b.getBoundingClientRect(),x1=ar.left+ar.width/2-cr.left,y1=ar.top+ar.height/2-cr.top,x2=br.left+br.width/2-cr.left,y2=br.top+br.height/2-cr.top,dx=Math.max(45,Math.abs(x2-x1)*.42),d=`M${x1},${y1} C${x1+dx},${y1} ${x2-dx},${y2} ${x2},${y2}`;html+=`<path class="route-wire" d="${d}"/><path class="route-wire-hit" data-edge="${i}" d="${d}"/>`});svg.innerHTML=html;$$('[data-edge]').forEach(p=>p.onclick=()=>{routeEdges.splice(+p.dataset.edge,1);drawRouteEdges();renderChain()})}
function armOutput(id){pendingPort=id;$$('.port.out').forEach(p=>p.classList.toggle('armed',p.dataset.portOut===id));toast(`Connect ${id} output to an input`)}
function connectInput(id){if(!pendingPort)return toast('Tap an output port first');if(pendingPort===id)return toast('Choose a different destination');if(routeEdges.some(e=>e[0]===pendingPort&&e[1]===id))return toast('That cable already exists');routeEdges.push([pendingPort,id]);pendingPort=null;$$('.port.out').forEach(p=>p.classList.remove('armed'));renderRoutingEditor();renderChain()}
function wireRouteNodes(){$$('[data-port-out]').forEach(p=>p.onclick=e=>{e.stopPropagation();armOutput(p.dataset.portOut)});$$('[data-port-in]').forEach(p=>p.onclick=e=>{e.stopPropagation();connectInput(p.dataset.portIn)});$$('[data-route-remove]').forEach(b=>b.onclick=e=>{e.stopPropagation();toggleLibraryPedal(b.dataset.routeRemove)});$$('[data-route-node]').forEach(node=>{const id=node.dataset.routeNode;if(id!=='INPUT'&&id!=='OUTPUT')node.onclick=e=>{if(e.target.closest('.port,.node-remove'))return;selectEffect(effects.findIndex(x=>x.id===id))};let moving=false,sx=0,sy=0,ox=0,oy=0;node.onpointerdown=e=>{if(e.target.closest('.port,.node-remove'))return;moving=true;sx=e.clientX;sy=e.clientY;ox=routeNodes[id].x;oy=routeNodes[id].y;node.setPointerCapture(e.pointerId)};node.onpointermove=e=>{if(!moving||!node.hasPointerCapture(e.pointerId))return;const r=$('#routeCanvas').getBoundingClientRect();routeNodes[id].x=Math.max(5,Math.min(95,ox+(e.clientX-sx)/r.width*100));routeNodes[id].y=Math.max(8,Math.min(92,oy+(e.clientY-sy)/r.height*100));node.style.left=routeNodes[id].x+'%';node.style.top=routeNodes[id].y+'%';drawRouteEdges()};node.onpointerup=()=>moving=false})}
function setChainEdit(v){editChainMode=v;$('#center').classList.toggle('editing',v);pendingPort=null;if(v){leftTab='pedals';$$('[data-ltab]').forEach(x=>x.classList.toggle('active',x.dataset.ltab==='pedals'));$('#search').placeholder='Search pedals…';renderBrowser();renderRoutingEditor()}else{renderChain()}}
function resetRouteLayout(){const ids=chain.map(c=>c.id);routeNodes={INPUT:{x:7,y:50,io:true},OUTPUT:{x:92,y:50,io:true}};ids.forEach((id,i)=>routeNodes[id]={x:20+(i%5)*14,y:ids.length>5?32+(i%2)*36:50});routeEdges=[];let prev='INPUT';ids.forEach(id=>{routeEdges.push([prev,id]);prev=id});routeEdges.push([prev,'OUTPUT']);renderRoutingEditor();renderChain();toast('Routing reset to a clean series path')}
function controlHtml'''
s, n = chain_block.subn(chain_new, s, count=1)
if n != 1:
    raise SystemExit('chain JS block replacement failed')

old_handlers = "$('#routing').onchange=e=>{routingMode=e.target.value;if(routingMode==='parallel'){chain.forEach((c,i)=>c.lane=i%2)}else chain.forEach(c=>c.lane=0);renderChain()};$('#addPedal').onclick=()=>{leftTab='pedals';$$('[data-ltab]').forEach(x=>x.classList.toggle('active',x.dataset.ltab==='pedals'));$('#search').placeholder='Search pedals…';renderBrowser();toast('Choose a pedal from the library')};"
new_handlers = "$('#editChain').onclick=()=>setChainEdit(true);$('#routeDone').onclick=()=>setChainEdit(false);$('#routeLibrary').onclick=()=>{leftTab='pedals';$$('[data-ltab]').forEach(x=>x.classList.toggle('active',x.dataset.ltab==='pedals'));$('#search').placeholder='Search pedals…';renderBrowser();toast('Use + in the Pedals library to place a pedal on the canvas')};$('#routeReset').onclick=resetRouteLayout;$('#routeClear').onclick=()=>{routeEdges=[];renderRoutingEditor();renderChain();toast('All routing cables cleared')};"
if old_handlers not in s:
    raise SystemExit('old routing handlers not found')
s = s.replace(old_handlers, new_handlers, 1)

bypass_anchor = 'function setBypass(v){'
master_js = r'''function masterDb(k){const min=+k.dataset.min,max=+k.dataset.max,v=+k.dataset.value;return min+(max-min)*(v/100)}
function wireMasterKnob(id,readout,onChange){const k=$(id);let sy=0,sv=0;const set=v=>{v=Math.max(0,Math.min(100,v));k.dataset.value=v;k.style.setProperty('--value',v);const db=masterDb(k);$(readout).textContent=`${db>=0?'+':''}${db.toFixed(1)} dB`;onChange(db)};k.onpointerdown=e=>{sy=e.clientY;sv=+k.dataset.value;k.setPointerCapture(e.pointerId)};k.onpointermove=e=>{if(k.hasPointerCapture(e.pointerId))set(Math.round(sv+(sy-e.clientY)*.65))};set(+k.dataset.value)}
'''
if bypass_anchor not in s:
    raise SystemExit('bypass anchor missing')
s = s.replace(bypass_anchor, master_js + bypass_anchor, 1)

meters_pat = re.compile(r"function meters\(\)\{.*?\}\nrenderCats", re.S)
meters_new = r'''function meters(){const base=50+Math.random()*26+inputTrimDb*.72,out=(bypassed?base-4:base+Math.random()*6)+outputLevelDb*.82;[['in1',base],['in2',base-4],['out1',out],['out2',out-3]].forEach(([id,v])=>$('#'+id).style.setProperty('--level',`${Math.max(7,Math.min(97,v))}%`));$('#inDb').textContent=`${(-9+inputTrimDb+Math.random()*3).toFixed(1)} dB`;$('#outDb').textContent=`${(-8+outputLevelDb+Math.random()*3).toFixed(1)} dB`}
renderCats'''
s, n = meters_pat.subn(meters_new, s, count=1)
if n != 1:
    raise SystemExit('meters replacement failed')

init_old = "renderCats();renderBrowser();syncPreset();renderChain();selectEffect(2);setInterval(meters,220);meters();"
init_new = "renderCats();renderBrowser();syncPreset();renderChain();selectEffect(2);wireMasterKnob('#inputTrim','#inputTrimRead',db=>inputTrimDb=db);wireMasterKnob('#outputLevel','#outputLevelRead',db=>outputLevelDb=db);setInterval(meters,220);meters();"
if init_old not in s:
    raise SystemExit('init anchor not found')
s = s.replace(init_old, init_new, 1)

p.write_text(s)

sw = Path('ui-prototype/sw.js')
if sw.exists():
    w = sw.read_text()
    w = re.sub(r"const CACHE = 'circuitpedal-ui-sandbox-v\d+';", "const CACHE = 'circuitpedal-ui-sandbox-v7';", w, count=1)
    sw.write_text(w)
