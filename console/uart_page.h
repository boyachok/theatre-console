#pragma once
const char UART_PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><title>Theatre · UART YX6300</title><link rel="stylesheet" href="/ui.css">
<style>.stats{display:grid;grid-template-columns:1fr 1fr;gap:7px}.stat{background:var(--card);padding:10px;border-radius:10px}.stat b{display:block;font-size:20px;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}.stat small{font-size:12px}pre{white-space:pre-wrap}</style>
<header><div class="inner"><div class="row"><a class="btn" href="/">← Консоль</a><strong class="grow" id="family">UART</strong><span id="connection">—</span></div><select id="target" onchange="targetChanged()" aria-label="Выбранная плата"></select></div></header>
<main><p id="hint">—</p><div class="stats"><div class="stat"><small>TX кадров / байт</small><b id="tx">—</b></div><div class="stat"><small>RX байт / ответов</small><b id="rx">—</b></div><div class="stat"><small>Корректных кадров</small><b id="frames">—</b></div><div class="stat"><small>Checksum / длина / таймаут</small><b id="errors">—</b></div></div>
<section class="panel"><div id="pins">ESP TX GPIO2 → RX плеера · ESP RX GPIO1 ← TX плеера · 9600 8N1</div><div id="age">—</div><div>Последний TX</div><pre id="lastTx">—</pre><div>Последний RX (хвост потока)</div><pre id="lastRx">—</pre><small id="extra">—</small><p id="playerReplies">—</p><p id="playback">—</p></section>
<section class="panel"><strong>Стендовый тест YX6300 — включает настоящий звук</strong><p>Сначала остановите сцены и отключите опасные нагрузки. Поставьте небольшую громкость, затем запустите трек. Номер трека — индекс файла на карте, порядок имён не гарантирует порядок FAT.</p>
<div class="grid cols2"><button onclick="stopAll()">Остановить ВСЕ сцены/выходы</button><button onclick="send(10)">STOP плеера</button>
<button data-audio-test onclick="send(13,10)">Громкость 10 из 30</button><button data-audio-test onclick="send(7,1)">Играть трек 1</button>
<button data-audio-test onclick="send(9)">Пауза</button><button data-audio-test onclick="send(8)">Продолжить</button></div>
<p>Принято ESP ≠ ACK плеера ≠ слышимый звук. При подключении только ESP TX управление возможно, но UART-ответов/подтверждения воспроизведения не будет.</p><div id="command">—</div></section>
<div class="grid cols2"><button onclick="send(17)">Запрос статуса</button><button onclick="send(19)">Очистить счётчики</button><button id="poll" onclick="togglePoll()">Пауза UART-опроса</button><button onclick="toggleMeasurements()" id="measure">Стоп пинг-теста</button></div></main><div id="notice" role="status"></div><script src="/ui.js"></script><script>
let requestAt=0;window.onTarget=()=>{requestAt=0};
window.onState=s=>{
 const n=selected();$('measure').textContent=s.measuring?'Стоп пинг-теста':'Начать пинг-тест';
 const online=n&&n.reportAgeMs!==null&&n.reportAgeMs<3000,u=n?.uart,age=n?.uartReportAgeMs;
 $('connection').textContent=online?'онлайн':'offline';
 if(online&&Date.now()-requestAt>5000){requestAt=Date.now();post('/uart/request',{id:n.id,mac:n.mac}).catch(e=>toast(e.message))}
 const valid=online&&u&&age!==null&&age<4000;
 for(const b of document.querySelectorAll('[data-audio-test]'))b.disabled=!valid||u.player!=='YX6300'||s.console?.running||n?.commandPending||busy.has(n?.mac);
 $('command').textContent=n?commandLabel(n):'Выберите плату';
 if(!valid){$('hint').textContent=!online?'Нет связи с платой':'Нет UART-телеметрии';for(const id of ['tx','rx','frames','errors','lastTx','lastRx','playerReplies','playback'])$(id).textContent='—';return}
 $('family').textContent='UART · '+(u.player||'XY-V17B (legacy)');$('tx').textContent=u.txFrames+' / '+u.txBytes;$('rx').textContent=u.rxBytes+' / '+u.replies;$('frames').textContent=u.validFrames;
 $('errors').textContent=u.checksumErrors+' / '+u.lengthErrors+' / '+u.frameTimeouts;
 $('pins').textContent=`ESP TX GPIO${u.txPin} → RX плеера · ESP RX GPIO${u.rxPin} ← TX плеера · 9600 8N1`;
 $('age').textContent=`Ответ: ${u.replyAgeMs===null?'—':u.replyAgeMs+' мс'} · RX: ${u.rxLevel} · отчёт: ${age} мс`;
 $('lastTx').textContent=u.lastTx||'—';$('lastRx').textContent=u.lastRx||'—';$('extra').textContent=`Эхо: ${u.loopbacks} · Ошибки TX: ${u.txFailures}`;
 $('playerReplies').textContent=u.player==='YX6300'?`ACK плеера: ${u.playerAcks} · NACK: ${u.playerErrors} · Последняя ошибка: ${u.lastPlayerError} · EOF: ${u.playerEnds}`:'Старая прошивка/разводка: тестовые кнопки YX6300 отключены';
 const c=n.control,fresh=c&&c.audioAgeMs!==null&&c.audioAgeMs<3000;
 $('playback').textContent=fresh?`Плеер сообщает: ${['STOP','PLAY','PAUSE'][c.audioState]||'состояние неизвестно'} · трек ${c.audioTrack||'—'} · громкость ${c.audioVolume<=30?c.audioVolume:'—'}`:'Нет свежего состояния плеера';
 $('poll').textContent=u.polling?'Пауза UART-опроса':'Включить UART-опрос';
 $('hint').textContent=s.console?.running?'Идёт сцена: стендовые PLAY-кнопки заблокированы':u.replyAgeMs!==null&&u.replyAgeMs<3000?'UART отвечает (не датчик звука)':u.loopbacks>0?'Эхо TX → RX':u.rxBytes===0?u.txFrames===0?'Нет передачи':'Нет входящих байтов':u.validFrames===0?'Неверные UART-кадры':'Нет свежего ответа';
};
function togglePoll(){const n=selected();if(n?.uart)send(20,n.uart.polling?0:1)}startUI();
</script></html>)HTML";
