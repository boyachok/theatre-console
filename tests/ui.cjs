const fs=require('fs'),path=require('path'),http=require('http'),assert=require('assert');
const {chromium}=require(process.env.PLAYWRIGHT_PATH||'playwright');
const root=path.resolve(__dirname,'../control'),sleep=ms=>new Promise(r=>setTimeout(r,ms)),epoch=Date.now(),now=()=>Date.now()-epoch;
function asset(file,name){const s=fs.readFileSync(path.join(root,file),'utf8'),start=s.indexOf('R"',s.indexOf(name)),open=s.indexOf('(',start),tag=s.slice(start+2,open);return s.slice(open+1,s.indexOf(')'+tag+'"',open))}
const assets={'/':['dashboard.h','DASHBOARD','text/html'],'/ui.css':['ui_common.h','UI_CSS','text/css'],'/ui.js':['ui_logic.h','UI_JS','text/javascript'],'/diagnostics':['diagnostics.h','DIAGNOSTICS','text/html'],'/uart':['uart_page.h','UART_PAGE','text/html'],'/matrix':['matrix.h','MATRIX','text/html']};
function makeState(){
 const uart={txFrames:30,txBytes:140,rxBytes:120,validFrames:24,replies:24,checksumErrors:0,lengthErrors:0,frameTimeouts:0,loopbacks:0,txFailures:0,txPin:17,rxPin:18,rxLevel:1,polling:true,txAgeMs:50,rxAgeMs:40,replyAgeMs:40,lastTx:'AA 01 00 AB',lastRx:'AA 01 01 00 AC'};
 return {version:'0.6.0',role:'master',connected:true,ssid:'YOUR_WIFI_SSID',ip:'192.168.10.50',channel:6,token:'test-token',rxDrops:0,txErrors:0,testSeconds:60,measuring:true,resetting:false,pwmPins:[4,5,6,7,9,10,11,12],nodes:Array.from({length:10},(_,i)=>({id:i+1,name:'Позиция '+(i+1),ip:'192.168.10.'+(60+i),mac:'A0:85:E3:00:00:'+String(i+1).padStart(2,'0'),protocol:i===1?3:4,pwmChannels:i===1?4:8,pwmHz:i===1?null:25000,pwmConfigSupported:i!==1,pwmConfig:i===1?null:{levels:Array.from({length:8},(_,ch)=>i===0&&ch===0?42:255),inverted:0,onMask:0,revision:1},health:'excellent',windowN:120,deliveryPct:100,deadlinePct:100,totalDeliveryPct:100,totalDeadlinePct:100,p50:3,p95:5,maxRtt:8,spread:2,rssiMaster:-50,rssiSlave:-51,worstRssi:-55,ageMs:100,streak:0,maxStreak:0,sent:200,received:200,timeouts:0,late:0,duplicates:0,reportAgeMs:100,control:{ready:true,pwm:Array(8).fill(0),relay:false,relayMode:i===1||i===2?0:1,fault:0,audioState:0,audioVolume:10,audioAgeMs:40,audioTrack:1,audioTracks:2},commandStatus:'none',commandPending:false,requestId:0,commandAckMs:null,uart:{...uart},uartReportAgeMs:100}))};
}
let state=makeState(),requests=[],postDelay=0,rejected=[];
const server=http.createServer(async(req,res)=>{
 const u=new URL(req.url,'http://localhost');res.setHeader('Cache-Control','no-store');
 if(req.method==='POST'){
  let body='';for await(const c of req)body+=c;const a=Object.fromEntries(new URLSearchParams(body));requests.push({url:u.pathname,...a});
  const fail=(code,text)=>{rejected.push({code,...a});res.statusCode=code;res.end(text)};
  if(a.token!==state.token){fail(403,'Token');return}
  let n;if(['/command','/identify','/name','/uart/request'].includes(u.pathname)){n=state.nodes.find(n=>n.id===Number(a.id)&&n.mac===a.mac);if(!n){fail(409,'Wrong target');return}}
  if(u.pathname==='/command'){
   await sleep(postDelay);const op=Number(a.op),v=Number(a.value),ch=Number(a.channel),urgent=[10,18].includes(op)||([3,4,21].includes(op)&&v===0);
   if(!urgent&&(Number(a.deadline)<now()||Number(a.deadline)>now()+1000)){fail(408,'Expired');return}
   if(n.uiClient===a.client&&Number(a.seq)<=n.uiSeq){fail(409,'Stale order');return}n.uiClient=a.client;n.uiSeq=Number(a.seq);
   if([1,2,6].includes(op)||([3,21,22].includes(op)&&ch>n.pwmChannels)){fail(400,'Unsupported');return}
   n.requestId++;n.commandAckMs=12;n.commandStatus='accepted';const c=n.control,p=n.pwmConfig;
   if(op===3){c.pwm[ch-1]=v;if(p){if(v)p.onMask|=1<<(ch-1);else p.onMask&=~(1<<(ch-1))}}
   if(op===21){c.pwm[ch-1]=v?(p?p.levels[ch-1]:255):0;if(p){if(v)p.onMask|=1<<(ch-1);else p.onMask&=~(1<<(ch-1))}}
   if(op===22){if(!p){fail(409,'Update slave');return}const i=ch-1,level=v&255,on=p.onMask&(1<<i);if(on&&p.levels[i]!==level)c.pwm[i]=level;p.levels[i]=level;if(v&256)p.inverted|=1<<i;else p.inverted&=~(1<<i);p.revision++}
   if(op===4)c.relay=!!v;if(op===5)c.relayMode=v;if(op===7){c.audioTrack=v;c.audioState=1}
   if(op===18){c.pwm.fill(0);c.relay=false;c.audioState=0;if(p)p.onMask=0}if(op===20)n.uart.polling=!!v;
   res.end(JSON.stringify({requestId:n.requestId,serverMs:now()}));return;
  }
  if(u.pathname==='/measure')state.measuring=a.running==='1';if(u.pathname==='/name')n.name=a.name;
  if(u.pathname==='/stop-all')for(const n of state.nodes){n.control.pwm.fill(0);n.control.relay=false;n.control.audioState=0;if(n.pwmConfig)n.pwmConfig.onMask=0}
  if(u.pathname==='/network/reset'){state.resetting=true;setTimeout(()=>{state.nodes=state.nodes.slice().reverse().map((n,i)=>({...n,id:i+1,name:'Ведомый '+(i+1)}));state.resetting=false},600)}
  res.end('OK');return;
 }
 if(u.pathname==='/api/status'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({...state,serverMs:now(),loopGapMaxMs:2}));return}
 const a=assets[u.pathname];if(a){res.setHeader('Content-Type',a[2]+'; charset=utf-8');res.end(asset(a[0],a[1]));return}res.statusCode=404;res.end('Not found');
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const base='http://127.0.0.1:'+server.address().port;
 const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 try{for(const [w,h] of [[390,844],[375,667],[320,568]]){
  state=makeState();requests=[];rejected=[];postDelay=0;
  const ctx=await browser.newContext({viewport:{width:w,height:h},isMobile:true,hasTouch:true,deviceScaleFactor:1}),p=await ctx.newPage(),errors=[];p.on('pageerror',e=>errors.push(e.message));
  await p.goto(base);await p.waitForFunction(()=>document.getElementById('connection').title==='Онлайн');
  const dims=await p.evaluate(()=>({width:document.documentElement.scrollWidth,height:document.documentElement.scrollHeight}));console.log('Viewport',w,h,dims);assert(dims.width<=w&&dims.height<=h+2);
  assert(!/\bARM\b|DISARM|Разрешить/.test(await p.locator('body').innerText()));assert.equal(await p.locator('#dimmerChannel').count(),0);
  await p.selectOption('#target','2');assert(await p.locator('#bank1').isDisabled());assert.equal(await p.locator('#relayOn').textContent(),'Настроить');
  const before=requests.length;await p.locator('#relayOn').click();await p.waitForSelector('#settings[open]');assert.equal(requests.length,before);
  await p.selectOption('#relayMode','2');await p.getByRole('button',{name:'Сохранить',exact:true}).click();await p.waitForFunction(()=>document.getElementById('relayOn').textContent==='ВКЛ');await p.getByRole('button',{name:'Готово',exact:true}).click();
  await p.locator('#pOn0').click();await p.waitForFunction(()=>document.getElementById('hdrPWM').textContent.includes('255'));
  await p.selectOption('#target','1');await p.locator('#pOn0').click();await p.waitForFunction(()=>document.getElementById('hdrPWM').textContent.includes('42'));
  await p.locator('#bank1').click();await p.locator('#pOn3').click();await p.waitForFunction(()=>document.getElementById('hdrPWM').textContent.includes('255'));
  await p.locator('#track').fill('2');await p.getByRole('button',{name:'▶ Играть'}).click();await p.waitForFunction(()=>document.getElementById('hdrAudio').textContent.includes('PLAY'));
  await p.getByRole('button',{name:'Настройки',exact:true}).click();await p.locator('#level7').fill('80');await p.locator('#inv7').check();await p.locator('#pwmSettings .row').nth(7).getByRole('button',{name:'✓',exact:true}).click();
  await p.waitForFunction(()=>document.getElementById('hdrPWM').textContent.includes('80'));assert.equal(state.nodes[0].control.pwm[0],42);assert.equal(state.nodes[0].control.audioState,1);assert.equal(state.nodes[0].pwmConfig.inverted,128);
  await p.getByRole('button',{name:'Готово',exact:true}).click();await p.goto(base+'/matrix');await p.waitForSelector('#pwm-1-8-on');assert.equal(await p.locator('#pwm-2-5-on').count(),0);assert.equal(await p.locator('input[data-op="3"]').count(),0);assert(!(await p.locator('body').innerText()).includes('Диммер PWM'));
  await p.locator('#pwm-1-8-off').click();await p.waitForFunction(()=>document.getElementById('pwm-1-8-off').classList.contains('on'));await p.locator('#pwm-1-8-on').click();await p.waitForFunction(()=>document.getElementById('pwm-1-8-on').classList.contains('on'));assert.equal(state.nodes[0].control.pwm[7],80);
  await p.locator('#pwm-1-7-on').click();await p.waitForFunction(()=>document.getElementById('pwm-1-7-on').classList.contains('on'));assert.equal(state.nodes[0].control.pwm[6],255);
  await p.getByRole('link',{name:'Настроить GPIO8',exact:true}).click();await p.waitForSelector('#settings[open]');assert.equal(await p.locator('#target').inputValue(),'3');await p.getByRole('button',{name:'Готово',exact:true}).click();
  await p.goto(base+'/matrix');await p.waitForSelector('#pwm-1-8-on');await p.getByRole('button',{name:'ALL OFF',exact:true}).first().click();await sleep(200);assert(state.nodes.every(n=>n.control.pwm.every(v=>v===0)));
  if(w===390){postDelay=3000;await p.locator('#pwm-1-7-on').click();await p.waitForFunction(()=>document.getElementById('cmd-1').textContent.includes('Отправка'));await sleep(3400);assert.equal(state.nodes[0].control.pwm[6],0);assert(rejected.some(x=>x.code===408));postDelay=0;}
  await p.goto(base+'/diagnostics');await p.waitForSelector('#rows tr');await p.locator('#rows tr').first().getByRole('button',{name:'Мигать',exact:true}).click();await sleep(50);assert(requests.some(x=>x.url==='/identify'&&x.id==='1'));
  p.once('dialog',d=>d.accept('Этаж 2'));await p.locator('#rows tr').first().getByRole('button',{name:'Имя',exact:true}).click();await p.waitForFunction(()=>document.getElementById('rows').textContent.includes('Этаж 2'));
  await p.goto(base+'/uart?id=1');await p.waitForFunction(()=>document.getElementById('tx').textContent.includes('30'));await p.getByRole('button',{name:'Очистить счётчики',exact:true}).click();await sleep(150);assert(requests.some(x=>x.url==='/command'&&x.op==='19'&&x.mac===state.nodes[0].mac));
  await p.goto(base+'/diagnostics');await p.waitForSelector('#forget');p.once('dialog',d=>d.accept());await p.locator('#forget').click();await sleep(1300);assert(!state.resetting);await p.goto(base);await p.waitForFunction(()=>document.getElementById('target').value==='10');assert.equal(state.nodes[9].pwmConfig.levels[7],80);
  const stale=await p.evaluate(async()=>{const t=(await(await fetch('/api/status')).json()).token;return(await fetch('/command',{method:'POST',body:new URLSearchParams({token:t,id:1,mac:'A0:85:E3:00:00:01',op:21,channel:1,value:1})})).status});assert.equal(stale,409);
  assert(!requests.some(x=>x.url==='/command'&&['1','2','6'].includes(x.op)));assert.equal(errors.length,0,errors.join('\n'));await ctx.close();
 }
 console.log('UI: binary PWM matrix, slave presets/inversion without reset, 255 fallback, no alias, expired 3-second HTTP rejected, MAC targeting and mobile layouts passed');
 }finally{await browser.close();server.close()}
})().catch(e=>{console.error(e);server.close();process.exitCode=1});
