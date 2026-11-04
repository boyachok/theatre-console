#pragma once
const char UI_JS[] PROGMEM=R"JS(let state=null,token='',refreshPromise=null,rosterKey='',clockAnchor=null,pollTimer=null,chosenMAC=new URLSearchParams(location.search).get('mac')||'',queryTarget=new URLSearchParams(location.search).get('id');const busy=new Set(),feedback=new Map(),clientId=Math.random().toString(36).slice(2)+Date.now().toString(36);let clientSequence=0;
const $=id=>document.getElementById(id),val=id=>$(id).value,esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])),pct=v=>v==null?'—':v.toFixed(2)+'%',num=v=>v==null?'—':v;
try{if(!queryTarget&&!chosenMAC)chosenMAC=localStorage.getItem('stageTargetMAC')||''}catch{}
function nodeBy(id,mac){return state?.nodes.find(n=>n.id===Number(id)&&(!mac||n.mac===mac))}
function selected(){return $('target')?nodeBy(val('target')):null}
function live(n){return !!n&&n.reportAgeMs!==null&&n.reportAgeMs<3000}
function pwmOn(n,ch){return n.pwmConfig?!!(n.pwmConfig.onMask&(1<<(ch-1))):n.control.pwm[ch-1]>0}
function toast(s){const el=$('notice');el.textContent=s;el.style.display='block';clearTimeout(toast.timer);toast.timer=setTimeout(()=>el.style.display='none',3000)}
function render(){if(state&&window.onState)window.onState(state)}
async function apiFetch(url,options={},timeout=1200){const c=new AbortController(),timer=setTimeout(()=>c.abort(),timeout);try{return await fetch(url,{...options,signal:c.signal})}finally{clearTimeout(timer)}}
function rememberTarget(){const n=selected();if(n){chosenMAC=n.mac;try{localStorage.setItem('stageTargetMAC',chosenMAC)}catch{}}}
function targetChanged(){rememberTarget();if(window.onTarget)window.onTarget(selected());render()}
async function refresh(){if(refreshPromise)return refreshPromise;refreshPromise=(async()=>{const at=performance.now(),r=await apiFetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);state=await r.json();if(token&&token!==state.token){feedback.clear();busy.clear();clockAnchor=null}const received=performance.now();if(typeof state.serverMs==='number'&&received-at<800)clockAnchor={server:state.serverMs,at:received};token=state.token;const key=JSON.stringify(state.nodes.map(n=>[n.id,n.mac,n.name,n.pwmChannels,n.pwmConfigSupported]));if($('target')&&key!==rosterKey){$('target').innerHTML=state.nodes.map(n=>`<option value="${n.id}">${n.id}. ${esc(n.name)}</option>`).join('');if(queryTarget){if(nodeBy(queryTarget))$('target').value=queryTarget;queryTarget=null}else if(chosenMAC){const n=state.nodes.find(n=>n.mac===chosenMAC);$('target').value=n?String(n.id):''}rosterKey=key;rememberTarget();if(window.onTarget)window.onTarget(selected())}render();return state})().finally(()=>refreshPromise=null);return refreshPromise}
async function post(url,args={},timeout=1200){if(!token)throw Error('Нет связи с мастером');const r=await apiFetch(url,{method:'POST',body:new URLSearchParams({...args,token})},timeout),text=await r.text();if(!r.ok)throw Error(text);return text}
function commandLabel(n){if(!n)return '—';const f=feedback.get(n.mac);if(busy.has(n.mac))return 'Отправка…';let text=messages[n.commandStatus]||n.commandStatus;if(f?.error)return f.error;if(f?.requestId!=null&&n.requestId<f.requestId)return 'Ждём статус';if(f&&n.requestId===f.requestId){text+=' · HTTP '+Math.round(f.httpMs)+' мс';if(n.commandAckMs!=null&&!n.commandPending)text+=' · ACK '+n.commandAckMs+' мс'}return text}
async function sendTo(id,op,value=0,channel=0,mac=''){
 const n=nodeBy(id,mac);if(!n||!live(n)){toast('Нет связи с платой');return false}
 const urgent=[10,18].includes(op)||([3,4,21].includes(op)&&value===0);
 if(!urgent&&(busy.has(n.mac)||n.commandPending||state.resetting)){toast('Дождитесь ACK');return false}
 if(!urgent&&(!clockAnchor||performance.now()-clockAnchor.at>10000)){toast('Нет свежего времени мастера');return false}
 const at=performance.now(),f={httpMs:0,requestId:null};feedback.set(n.mac,f);busy.add(n.mac);render();
 try{const deadline=clockAnchor?Math.floor(clockAnchor.server+performance.now()-clockAnchor.at+700)>>>0:0;
 const result=JSON.parse(await post('/command',{id:n.id,mac:n.mac,op,value,channel,deadline,client:clientId,seq:++clientSequence},1000));f.httpMs=performance.now()-at;f.requestId=result.requestId;return true
 }catch(e){f.error=e.name==='AbortError'?'HTTP: нет подтверждения, результат неизвестен':e.message;toast(f.error);return false}
 finally{if(feedback.get(n.mac)===f)busy.delete(n.mac);render();refresh().catch(()=>{});schedulePoll(80)}
}
async function send(op,value=0,channel=0){const n=selected();return n?sendTo(n.id,op,value,channel,n.mac):false}
async function sendAll(op,value=0,channel=0){await Promise.all(state.nodes.filter(n=>live(n)&&(![3,21].includes(op)||n.pwmChannels>=channel)&&(op!==4||n.control.relayMode)).map(n=>sendTo(n.id,op,value,channel,n.mac)))}
async function stopAll(){try{await post('/stop-all');await refresh()}catch(e){toast('ALL OFF: '+e.message)}}
async function toggleMeasurements(){try{await post('/measure',{running:state.measuring?0:1});await refresh()}catch(e){toast(e.message)}}
async function identifyNode(id,mac){const n=nodeBy(id,mac);if(n)try{await post('/identify',{id:n.id,mac:n.mac})}catch(e){toast(e.message)}}
function identify(){const n=selected();if(n)return identifyNode(n.id,n.mac)}
async function renameNodeBy(id,mac){const n=nodeBy(id,mac);if(!n)return;const name=prompt('Имя платы',n.name);if(name!==null)try{await post('/name',{id:n.id,mac:n.mac,name});await refresh()}catch(e){toast(e.message)}}
function renameNode(){const n=selected();if(n)return renameNodeBy(n.id,n.mac)}
async function resetNetwork(){if(!confirm('Забыть все платы и имена, выключить выходы и назначить номера заново?'))return;try{await post('/network/reset');busy.clear();feedback.clear();await refresh()}catch(e){toast(e.message)}}
async function maintenance(){const n=selected();if(n)try{const result=JSON.parse(await post('/maintenance',{id:n.id,mac:n.mac}));location.href=result.url}catch(e){toast(e.message)}}
async function exportData(){try{const r=await apiFetch('/api/status?history=1',{},10000);if(!r.ok)throw Error('HTTP '+r.status);const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(await r.json(),null,2)],{type:'application/json'}));a.download='stage-'+Date.now()+'.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)}catch(e){toast(e.message)}}
const messages={none:'Готов',waiting_ack:'Ждём ACK',preparing:'Подготовка',accepted:'Подтверждено ESP',not_ready:'Нет готовности',expired_before_activation:'Просрочено до включения',network_changed:'Переподключение',session_changed:'Новая сессия',player_offline:'Нет ответа UART',audio_unavailable:'Нет готовности аудио',heartbeat_expired:'Нет heartbeat',invalid:'Неверные параметры',io_or_queue_error:'Ошибка выхода/настроек',stale:'Устаревшая команда'};
function schedulePoll(ms){clearTimeout(pollTimer);pollTimer=setTimeout(pollUI,ms)}
async function pollUI(){try{await refresh()}catch(e){if($('connection'))$('connection').textContent='Нет связи';if(window.onDisconnect)window.onDisconnect()}schedulePoll(busy.size||state?.nodes.some(n=>n.commandPending)?100:500)}
function startUI(){pollUI()}
)JS";
