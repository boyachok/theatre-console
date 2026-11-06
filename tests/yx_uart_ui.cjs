// Console UART page on a MOCK server only; no real ESP/serial/audio.
const fs=require('fs'),path=require('path'),http=require('http'),assert=require('assert');
const {chromium}=require(process.env.PLAYWRIGHT_PATH||'playwright');
const root=path.resolve(__dirname,'../console'),epoch=Date.now(),now=()=>Date.now()-epoch;
function raw(file,name){const s=fs.readFileSync(path.join(root,file),'utf8'),start=s.indexOf('R"',s.indexOf(name)),open=s.indexOf('(',start),tag=s.slice(start+2,open);return s.slice(open+1,s.indexOf(')'+tag+'"',open));}
const node={id:1,mac:'C0:4E:30:27:69:98',name:'Bench YX',pwmChannels:12,reportAgeMs:20,commandPending:false,commandStatus:'none',requestId:0,commandAckMs:null,
 control:{ready:true,audioState:255,audioVolume:255,audioTrack:0,audioAgeMs:null},uartReportAgeMs:20,
 uart:{player:'YX6300',txPin:2,rxPin:1,rxLevel:1,polling:true,txFrames:8,txBytes:80,rxBytes:40,validFrames:4,replies:4,checksumErrors:0,lengthErrors:0,frameTimeouts:0,loopbacks:0,txFailures:0,txAgeMs:20,rxAgeMs:20,replyAgeMs:20,lastTx:'7E FF 06 42 01 00 00 FE B8 EF',lastRx:'7E FF 06 41 00 00 00 FE BA EF',playerAcks:3,playerErrors:1,lastPlayerError:5,playerEnds:0}};
const state={role:'master',token:'mock-only',measuring:true,resetting:false,console:{running:true,livePreview:true},nodes:[node]};const calls=[],errors=[];
const assets={'/uart':['uart_page.h','UART_PAGE','text/html'],'/ui.js':['ui_logic.h','UI_JS','text/javascript'],'/ui.css':['ui_common.h','UI_CSS','text/css']};
const server=http.createServer(async(req,res)=>{
 try{const u=new URL(req.url,'http://localhost');res.setHeader('Cache-Control','no-store');
 if(req.method==='POST'){let body='';for await(const c of req)body+=c;const a=Object.fromEntries(new URLSearchParams(body));assert.equal(a.token,state.token);calls.push({url:u.pathname,...a});
  if(u.pathname==='/stop-all'){state.console.running=state.console.livePreview=false;res.end('ALL OFF');return;}
  assert.equal(a.mac,node.mac);assert.equal(Number(a.id),1);
  if(u.pathname==='/command'){const op=Number(a.op);if(op!==10){assert(Number(a.deadline)>=now());assert(!state.console.running);}
   node.requestId++;node.commandStatus='accepted';node.commandAckMs=8;res.end(JSON.stringify({requestId:node.requestId,serverMs:now()}));return;}
  res.end('Requested');return;
 }
 if(u.pathname==='/api/status'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({...state,serverMs:now()}));return;}
 const asset=assets[u.pathname];if(!asset){res.writeHead(404).end();return;}res.setHeader('Content-Type',asset[2]+'; charset=utf-8');res.end(raw(asset[0],asset[1]));
 }catch(e){errors.push(String(e));res.writeHead(500).end(String(e));}
});
(async()=>{await new Promise(r=>server.listen(0,'127.0.0.1',r));const base='http://127.0.0.1:'+server.address().port;
 const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 try{const page=await browser.newPage({viewport:{width:390,height:844}});page.on('pageerror',e=>errors.push(e.message));await page.goto(base+'/uart');
 await page.waitForFunction(()=>document.getElementById('family').textContent.includes('YX6300'));
 assert((await page.locator('#pins').textContent()).includes('TX GPIO2'));assert((await page.locator('#pins').textContent()).includes('RX GPIO1'));
 assert((await page.locator('#lastTx').textContent()).endsWith('EF'));assert((await page.locator('#playerReplies').textContent()).includes('NACK: 1'));
 assert(await page.getByRole('button',{name:'Играть трек 1',exact:true}).isDisabled());assert(!calls.some(c=>c.url==='/command'));
 await page.getByRole('button',{name:'Остановить ВСЕ сцены/выходы',exact:true}).click();
 await page.waitForFunction(()=>!document.querySelector('[data-audio-test][onclick="send(7,1)"]').disabled);
 await page.getByRole('button',{name:'Громкость 10 из 30',exact:true}).click();await page.waitForTimeout(200);
 await page.getByRole('button',{name:'Играть трек 1',exact:true}).click();await page.waitForTimeout(200);
 await page.getByRole('button',{name:'STOP плеера',exact:true}).click();await page.waitForTimeout(150);
 assert.deepEqual(calls.filter(c=>c.url==='/command').map(c=>[Number(c.op),Number(c.value)]),[[13,10],[7,1],[10,0]]);
 assert((await page.locator('#playback').textContent()).includes('Нет свежего состояния')); // HTTP accepted cannot fabricate playback.
 node.uart.player='XY-V17B (legacy)';await page.waitForTimeout(700);assert(await page.getByRole('button',{name:'Играть трек 1',exact:true}).isDisabled());
 for(const width of [320,375,390]){await page.setViewportSize({width,height:844});assert((await page.evaluate(()=>document.documentElement.scrollWidth))<=width);}
 assert.deepEqual(errors,[]);console.log('YX UART MOCK UI: GPIO directions/full10B TX, ACK/NACK counters, no fabricated audio, active-scene block, explicit ALL OFF + volume/play/stop, legacy-driver block, mobile passed');
 }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);server.close();process.exitCode=1;});
