import { CONFIG } from './config.js';
import { BrowserSettingsStore, validateName } from './settings.js';

const MONEY = [1,3,5,7,9,11,13,15,17,19,21,23];
const BUILDINGS = [2,4,6,8,10,12,14,16,18,20];
const app = document.querySelector('#app');
const store = new BrowserSettingsStore();
const settings = store.load();
const audio = Object.fromEntries(['kettei','billGet','opencard'].map(name =>
  [name, new Audio(`assets/sounds/${name}.mp3`)]));
let screen = 'title';
const screenHistory = [];
function openScreen(next) { if (screen !== next) screenHistory.push(screen); screen = next; }
function goBack() { screen = screenHistory.pop() || 'title'; }
let game = null;
let difficulty = 'NORMAL';
const difficultyLabel = value => ({EASY:'かんたん',NORMAL:'ふつう',HARD:'むずかしい'}[value] || value);
let network = null;
let wasmCore = null;
let online = { waiting: '--', status: '', roomCode: '', error: '', match: null, result: null, selectedCard: null };

function track(event) {
  fetch('/api/analytics', { method: 'POST', headers: {'content-type':'application/json'}, body: JSON.stringify({event}), keepalive: true }).catch(() => {});
}
track('pageViews');

function esc(value) {
  return String(value).replace(/[&<>'"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;',"'":'&#39;','"':'&quot;'}[c]));
}
function sound(name) {
  const item = audio[name];
  item.volume = settings.muted ? 0 : settings.masterVolume / 100;
  item.currentTime = 0;
  item.play().catch(() => {});
}
function shuffle(values) {
  const result = [...values];
  for (let i=result.length-1;i>0;i--) { const j=Math.floor(Math.random()*(i+1)); [result[i],result[j]]=[result[j],result[i]]; }
  return result;
}
function winner(building,a,b) {
  if (a===b) return 0;
  if (a>building && a>b) return 1;
  if (b>building && b>a) return 2;
  return 0;
}
function cpuCard(g) {
  if (difficulty==='EASY') return g.p2.hand[Math.floor(Math.random()*g.p2.hand.length)];
  const legal = g.p2.hand.filter(v => v>g.building).sort((a,b)=>a-b);
  if (difficulty==='NORMAL') {
    if (!legal.length || (g.building<8 && Math.random()<.35)) return g.p2.hand[0];
    return legal[Math.min(legal.length-1, Math.random()<.25 ? 1 : 0)];
  }
  const opponent = g.p1.hand;
  return g.p2.hand.map(card => {
    const wins = opponent.filter(v => card>g.building && card>v).length / opponent.length;
    const value = wins*g.building*2 - Math.max(0,card-g.building)*.65 + (23-card)*.22 + Math.random()*1.5;
    return {card,value};
  }).sort((a,b)=>b.value-a.value)[0].card;
}
function newLocalGame() {
  if (wasmCore) {
    wasmCore.start(['EASY','NORMAL','HARD'].indexOf(difficulty));
    game=wasmCore.state();
    sound('opencard');
    return;
  }
  game = { round:1, deck:shuffle(BUILDINGS), building:0, p1:{hand:[...MONEY],buildings:[],used:[]}, p2:{hand:[...MONEY],buildings:[],used:[]}, selected:null, result:null, phase:'bid', discarded:[] };
  game.building=game.deck[0]; sound('opencard');
}
function fromWasmState(s) {
  return {
    round:s.round,building:s.building,
    p1:{hand:s.p1Money,buildings:s.p1Buildings,used:[]},
    p2:{hand:s.p2Money,buildings:s.p2Buildings,used:[]},
    discarded:s.discarded,selected:null,result:s.result||null,
    phase:s.state===4?'result':s.state===5?'final':'bid',wasm:true,
  };
}
function summary(g,p) {
  return { count:g[p].buildings.length, value:g[p].buildings.reduce((a,b)=>a+b,0), cash:g[p].hand.reduce((a,b)=>a+b,0) };
}
function finishLocalRound() {
  const a=game.selected, b=cpuCard(game), win=winner(game.building,a,b);
  game.p1.hand=game.p1.hand.filter(v=>v!==a); game.p2.hand=game.p2.hand.filter(v=>v!==b);
  game.p1.used.push(a); game.p2.used.push(b);
  if (win) game[`p${win}`].buildings.push(game.building); else game.discarded.push(game.building);
  game.result={building:game.building,p1Bid:a,p2Bid:b,winner:win}; game.phase='result'; game.selected=null;
  if (win) sound('billGet');
}
function button(label, action, cls='') { return `<button class="btn ${cls}" data-action="${action}">${label}</button>`; }
function shell(content, title='') { return `<section class="game-shell"><header><div class="mini-logo">ビルビル <small>BILL BILL</small></div><span>${title}</span></header>${content}</section>`; }
function moneyRack(player, { selectable=false, onlineCards=false, disabled=false, selectedCard=null }={}) {
  return `<div class="money-rack" aria-label="残りお金カード">${MONEY.map(value => {
    const available=player.hand.includes(value);
    const selected=selectable&&(onlineCards?selectedCard===value:game?.selected===value);
    const attr=available&&selectable&&!disabled ? (onlineCards?`data-online-card="${value}"`:`data-card="${value}"`) : 'disabled';
    return `<button class="money-slot ${available?'available':'used'} ${selected?'selected':''}" ${attr} aria-label="${value}${available?'':' 使用済み'}"><b>${value}</b>${available?'':'<span>×</span>'}</button>`;
  }).join('')}</div>`;
}
function city(player, name, active=false, options={}) {
  const buildings=player.buildings.map((v,i)=>{ const tier=v<=6?'shop':v<=12?'midrise':v<=16?'highrise':'tower'; const colors=['#ff786d','#49aee8','#f2b84b','#7bc67b','#a88be0']; return `<span class="building ${tier} ${options.highlightBuilding===v?'new-building':''}" style="--h:${38+v*3.7}px;--c:${colors[(v/2-1)%colors.length]}" title="建物 ${v}"><i aria-hidden="true"></i><b>${v}</b></span>`; }).join('');
  return `<article class="city ${active?'active':''} ${options.justAcquired?'just-acquired':''}"><h3>${esc(name)}</h3><div class="skyline">${buildings || '<span class="empty-city">まだ建物がありません</span>'}</div><p>建物 ${player.buildings.length}棟 ・ 合計 ${player.buildings.reduce((a,b)=>a+b,0)}</p><h4>お金カード</h4>${moneyRack(player,options)}</article>`;
}
function resultPanel(result, player1Name, player2Name) {
  const winnerName=result.winner===1?player1Name:result.winner===2?player2Name:'';
  return `<div class="round-result"><div class="result-building"><small>建物</small><b>${result.building}</b></div><div class="bid-showdown"><div class="bid-player ${result.winner===1?'round-winner':''}"><strong>${esc(player1Name)}</strong><span class="revealed-money">${result.p1Bid}</span></div><div class="versus">対</div><div class="bid-player ${result.winner===2?'round-winner':''}"><strong>${esc(player2Name)}</strong><span class="revealed-money">${result.p2Bid}</span></div></div><h2 class="round-outcome">${winnerName?`${esc(winnerName)}が建物を獲得！`:'獲得者なし'}</h2></div>`;
}
function renderTitle() {
  app.innerHTML=`<section class="title-screen"><div class="cloud c1"></div><div class="cloud c2"></div><div class="brand"><h1>ビルビル</h1><p>BILL BILL</p></div><nav class="title-menu">${button('VS CPU','cpu','coral')}${button('VS PLAYER','online','blue')}${button('HOW TO PLAY','how','light')}${button('SETTINGS','settings','yellow')}${button('CREDITS','credits','light')}${button('FEEDBACK ↗','feedback','light')}</nav><p class="tagline">カードを選んで、あなただけの街をつくろう。</p><div class="ground"></div></section>`;
}
function renderDifficulty() {
  app.innerHTML=shell(`<div class="panel centered"><h1>CPU難易度</h1><p>対戦する強さを選んでください</p><div class="stack">${button('かんたん','diff:EASY','blue')}${button('ふつう','diff:NORMAL','yellow')}${button('むずかしい','diff:HARD','coral')}${button('戻る','title','light')}</div></div>`,'CPU対戦');
}
function renderLocal() {
  const p1name=settings.playerName;
  if(game.phase==='final') {
    if (!game.completionTracked) { game.completionTracked=true; track('cpuMatchesCompleted'); }
    const a=summary(game,'p1'),b=summary(game,'p2'); let ap=0,bp=0;
    for(const k of ['count','value','cash']) { if(a[k]>b[k])ap++; else if(b[k]>a[k])bp++; }
    app.innerHTML=shell(`<div class="panel centered"><h1>${ap===bp?'引き分け':ap>bp?'あなたの勝ち！':'CPUの勝ち'}</h1><div class="score-grid"><div><h2>${esc(p1name)}</h2><p>建物枚数 ${a.count}</p><p>建物合計 ${a.value}</p><p>残金 ${a.cash}</p><strong>${ap} ポイント</strong></div><div><h2>CPU</h2><p>建物枚数 ${b.count}</p><p>建物合計 ${b.value}</p><p>残金 ${b.cash}</p><strong>${bp} ポイント</strong></div></div>${button('もう一度遊ぶ','restart','coral')}${button('タイトルへ','title','light')}</div>`,'最終結果'); return;
  }
  const result=game.result, bidding=!result;
  const p1Options={selectable:bidding,justAcquired:result?.winner===1,highlightBuilding:result?.winner===1?result.building:null};
  const p2Options={justAcquired:result?.winner===2,highlightBuilding:result?.winner===2?result.building:null};
  app.innerHTML=shell(`<div class="toolbar">ラウンド ${Math.min(game.round,10)} / 10 ${button('対戦を終了','leave-local','tiny')}</div><div class="cities">${city(game.p1,p1name,true,p1Options)}${city(game.p2,`CPU・${difficultyLabel(difficulty)}`,false,p2Options)}</div><div class="auction">${result?`${resultPanel(result,p1name,'CPU')}${button(game.round===10?'最終結果':'次へ','next','coral')}`:`<div class="property"><small>今回の建物</small><b>${game.building}</b></div><h2>お金カードを1枚選択</h2>${button('決定','bid','coral')}`}</div>`,'CPU対戦');
}
function renderSettings() {
  app.innerHTML=shell(`<div class="panel settings"><h1>設定</h1><label>プレイヤー名<input id="player-name" type="text" maxlength="12" value="${esc(settings.playerName)}"><small id="name-error">入力後にエンターキー、または枠外をクリックして保存</small></label><label>マスター音量 <output>${settings.masterVolume}%</output><input id="volume" type="range" min="0" max="100" value="${settings.masterVolume}"></label><div class="setting-row"><span>ミュート <small>現在: ${settings.muted?'オン':'オフ'}</small></span><button class="btn ${settings.muted?'blue':'coral'}" data-action="mute" aria-pressed="${settings.muted}">${settings.muted?'オフ':'オン'}</button></div><div class="menu-grid">${button('戻る','back','light')}</div></div>`,'設定');
}
function renderHow() {
  app.innerHTML=shell(`<div class="panel rules"><h1>遊び方</h1><ol><li>毎ラウンド、建物カードが1枚公開されます。</li><li>両プレイヤーがお金カードを1枚選びます。</li><li>建物価格より高く、相手より高いカードを出した側が獲得します。</li><li>使用したカードは勝敗に関係なく消費されます。</li><li>同額、または両者とも建物価格以下なら誰も獲得しません。</li><li>全10ラウンド後、建物枚数・価値合計・残金の3項目で勝敗を決めます。</li></ol><div class="example"><span class="money static">15</span><b>＞</b><span class="property small"><b>12</b></span><b>＆</b><span class="money static red">13</span><p>15は建物12より高く、相手の13より高いので建物を獲得！</p></div>${button('戻る','back','blue')}</div>`,'遊び方');
}
function renderCredits() { app.innerHTML=shell(`<div class="panel centered credits"><h1>クレジット</h1><p>制作者</p><h2>${esc(CONFIG.creator)}</h2><a class="social-link" href="${esc(CONFIG.creatorSocialUrl)}" target="_blank" rel="noopener noreferrer">X（旧ツイッター）@MihaLseisaku ↗</a>${button('戻る','back','blue')}</div>`,'クレジット'); }
function renderOnlineMenu() { app.innerHTML=shell(`<div class="panel centered"><h1>オンライン対戦</h1><p>あなたの名前<br><strong>${esc(settings.playerName)}</strong></p><div class="stack">${button('ランダムマッチ','random','coral')}${button('ルームマッチ','room','blue')}${button('戻る','title','light')}</div></div>`,'対人戦'); }
function renderRandom() { app.innerHTML=shell(`<div class="panel centered"><h1>ランダムマッチ</h1><p>あなたの名前<br><strong>${esc(settings.playerName)}</strong></p><div class="waiting">待機中：${esc(online.waiting)}人</div><p>${esc(online.status)}</p>${online.status?button('キャンセル','cancel-search','light'):button('対戦相手を探す','find','coral')}${button('戻る','random-back','light')}<p class="error">${esc(online.error)}</p></div>`,'オンライン対戦'); }
function renderRoom() { app.innerHTML=shell(`<div class="panel centered"><h1>ルームマッチ</h1><p>あなたの名前<br><strong>${esc(settings.playerName)}</strong></p>${button('ルームを作る','create-room','coral')}${button('ルームに参加','join-room','blue')}${button('戻る','online','light')}</div>`,'オンライン対戦'); }
function renderJoin() { app.innerHTML=shell(`<div class="panel centered"><h1>ルームコードを入力</h1><input id="room-code" class="room-code" inputmode="numeric" maxlength="6" placeholder="______"><p>あなたの名前<br><strong>${esc(settings.playerName)}</strong></p>${button('参加','do-join','coral')}${button('戻る','room','light')}<p class="error">${esc(online.error)}</p></div>`,'ルームマッチ'); }
function renderWaitingRoom() { app.innerHTML=shell(`<div class="panel centered"><h1>ルームコード</h1><div class="code">${esc(online.roomCode)}</div><p>ホスト<br><strong>${esc(settings.playerName)}</strong></p><p>参加者を待っています…</p>${button('キャンセル','cancel-room','light')}</div>`,'ルームマッチ'); }
function renderDisconnected() { app.innerHTML=shell(`<div class="panel centered"><h1>対戦相手との接続が切れました</h1>${button('オンラインメニューへ戻る','online','coral')}</div>`,'接続'); }
function onlinePlayer(n) { const s=online.match; return { hand:s.hands[n], buildings:s.acquired[n], used:s.used[n] }; }
function renderOnlineGame() {
  const s=online.match, you=s.you, other=you===1?2:1;
  if (!s) return;
  const result=online.result, displayRound=result?.round??Math.min(s.round,10);
  const p1=onlinePlayer(you),p2=onlinePlayer(other);
  const myOptions={selectable:!result,onlineCards:true,disabled:s.submitted,selectedCard:online.selectedCard,justAcquired:result?.winner===you,highlightBuilding:result?.winner===you?result.building:null};
  const otherOptions={justAcquired:result?.winner===other,highlightBuilding:result?.winner===other?result.building:null};
  const firstName=s.players[you-1].name,secondName=s.players[other-1].name;
  const orderedResult=result&&you===2?{...result,p1Bid:result.p2Bid,p2Bid:result.p1Bid,winner:result.winner===1?2:result.winner===2?1:0}:result;
  app.innerHTML=shell(`<div class="toolbar">ラウンド ${displayRound} / 10 ${button('対戦を終了','leave-match','tiny')}</div><div class="cities">${city(p1,firstName,true,myOptions)}${city(p2,secondName,false,otherOptions)}</div><div class="auction">${result?`${resultPanel(orderedResult,firstName,secondName)}${button('次へ','online-next','coral')}`:`<div class="property"><small>今回の建物</small><b>${s.currentBuilding ?? '-'}</b></div><h2>${s.submitted?'相手の入札を待っています…':'お金カードを1枚選択'}</h2>${s.submitted?'':`<button class="btn coral" data-action="online-bid" ${online.selectedCard==null?'disabled':''}>決定</button>`}`}</div>`,'オンライン対戦');
}
function renderOnlineFinal() {
  const final=online.final, s=online.match, you=s.you, other=you===1?2:1;
  const mine=final.summaries[you-1], theirs=final.summaries[other-1];
  app.innerHTML=shell(`<div class="panel centered"><h1>${final.winner===0?'引き分け':final.winner===you?'あなたの勝ち！':'あなたの負け'}</h1><div class="score-grid"><div><h2>${esc(s.players[you-1].name)}</h2><p>建物枚数 ${mine.buildingCount}</p><p>建物合計 ${mine.buildingTotal}</p><p>残金 ${mine.remainingMoney}</p><strong>${mine.points} ポイント</strong></div><div><h2>${esc(s.players[other-1].name)}</h2><p>建物枚数 ${theirs.buildingCount}</p><p>建物合計 ${theirs.buildingTotal}</p><p>残金 ${theirs.remainingMoney}</p><strong>${theirs.points} ポイント</strong></div></div>${button('オンラインメニューへ戻る','online','coral')}</div>`,'最終結果');
}
function render() {
  ({title:renderTitle,difficulty:renderDifficulty,local:renderLocal,settings:renderSettings,how:renderHow,credits:renderCredits,online:renderOnlineMenu,random:renderRandom,room:renderRoom,join:renderJoin,waitingRoom:renderWaitingRoom,onlineGame:renderOnlineGame,onlineFinal:renderOnlineFinal,disconnected:renderDisconnected}[screen]||renderTitle)();
}

class NetworkClient {
  constructor() {
    this.ws=new WebSocket(CONFIG.websocketUrl);
    this.ws.addEventListener('open',()=>this.send('setPlayerName',{name:settings.playerName}));
    this.ws.addEventListener('message',event=>this.onMessage(JSON.parse(event.data)));
    this.ws.addEventListener('close',()=>{ if(screen==='onlineGame'){screen='disconnected';render();} });
  }
  send(type,payload={}) { if(this.ws.readyState===1)this.ws.send(JSON.stringify({type,...payload})); else this.ws.addEventListener('open',()=>this.send(type,payload),{once:true}); }
  onMessage(msg) {
    if(msg.type==='connected'||msg.type==='queueStatus') online.waiting=msg.waiting;
    if(msg.type==='error'){online.error=msg.message;online.status='';}
    if(msg.type==='roomCreated'){online.roomCode=msg.code;screen='waitingRoom';}
    if(msg.type==='matchFound'){online.match=msg.state;online.result=null;online.selectedCard=null;screen='onlineGame';sound('opencard');}
    if(msg.type==='bidAccepted'&&online.match) online.match.submitted=true;
    if(msg.type==='roundResult'){online.match=msg.state;online.result=msg.result;online.selectedCard=null;if(msg.result.winner)sound('billGet');}
    if(msg.type==='gameResult'){online.final=msg.result;}
    if(msg.type==='opponentDisconnected') screen='disconnected';
    render();
  }
}
function net(){ return network ||= new NetworkClient(); }

app.addEventListener('click', e => {
  const card=e.target.closest('[data-card]');
  if(card){
    const value=Number(card.dataset.card);
    if(!wasmCore||!game.wasm||wasmCore.select(value)){game.selected=value;sound('kettei');render();}
    return;
  }
  const onlineCard=e.target.closest('[data-online-card]');
  if(onlineCard){online.selectedCard=Number(onlineCard.dataset.onlineCard);sound('kettei');render();return;}
  const el=e.target.closest('[data-action]'); if(!el)return;
  const action=el.dataset.action; sound('kettei'); online.error='';
  if(action==='cpu')screen='difficulty';
  else if(action.startsWith('diff:')){difficulty=action.split(':')[1];newLocalGame();track('cpuMatchesStarted');screen='local';}
  else if(action==='bid'&&game?.selected!=null){if(wasmCore&&game.wasm){wasmCore.confirm();game=wasmCore.state();if(game.result?.winner)sound('billGet');}else finishLocalRound();}
  else if(action==='next'){if(wasmCore&&game.wasm){wasmCore.advance();game=wasmCore.state();if(game.phase==='bid')sound('opencard');}else if(game.round===10)game.phase='final';else{game.round++;game.building=game.deck[game.round-1];game.result=null;game.phase='bid';sound('opencard');}}
  else if(action==='leave-local'){if(confirm('CPU対戦を終了してタイトルへ戻りますか？')){game=null;screen='title';screenHistory.length=0;}}
  else if(action==='restart'){if(confirm('対戦を最初からやり直しますか？')){if(wasmCore&&game?.wasm){wasmCore.restart();game=wasmCore.state();sound('opencard');}else newLocalGame();}}
  else if(action==='settings')openScreen('settings');
  else if(action==='how')openScreen('how');
  else if(action==='credits')openScreen('credits');
  else if(action==='back')goBack();
  else if(action==='feedback')window.open(CONFIG.feedbackUrl,'_blank','noopener,noreferrer');
  else if(action==='online'){net().send('getQueueStatus');screen='online';}
  else if(action==='random'){net().send('getQueueStatus');screen='random';}
  else if(action==='find'){online.status='対戦相手を探しています…';net().send('joinQueue');}
  else if(action==='cancel-search'){net().send('leaveQueue');online.status='';}
  else if(action==='random-back'){net().send('leaveQueue');online.status='';online.error='';screen='online';}
  else if(action==='room')screen='room';
  else if(action==='create-room')net().send('createRoom');
  else if(action==='join-room')screen='join';
  else if(action==='do-join')net().send('joinRoom',{code:document.querySelector('#room-code').value.trim()});
  else if(action==='cancel-room'){net().send('cancelRoom');screen='room';}
  else if(action==='online-bid'&&online.selectedCard!=null){const card=online.selectedCard;online.selectedCard=null;net().send('submitBid',{matchId:online.match.matchId,card});}
  else if(action==='online-next'){online.result=null;if(online.final){screen='onlineFinal';}else{online.match.submitted=false;sound('opencard');}}
  else if(action==='leave-match'){if(confirm('オンライン対戦から退出しますか？')){net().send('leaveMatch');screen='online';}}
  else if(action==='mute'){settings.muted=!settings.muted;store.save(settings);}
  else if(action==='title'){screen='title';screenHistory.length=0;}
  render();
});
function savePlayerName(input) {
  const checked=validateName(input.value),error=document.querySelector('#name-error');
  if (!checked.ok) { error.textContent=checked.error; return false; }
  settings.playerName=checked.value; input.value=checked.value; store.save(settings); error.textContent='保存しました';
  if(network)network.send('setPlayerName',{name:settings.playerName});
  return true;
}
app.addEventListener('change',e=>{if(e.target.id==='player-name')savePlayerName(e.target);});
app.addEventListener('keydown',e=>{if(e.target.id==='player-name'&&e.key==='Enter'){e.preventDefault();savePlayerName(e.target);e.target.blur();}});
app.addEventListener('input', e => {
  if(e.target.id==='volume'){settings.masterVolume=Number(e.target.value);store.save(settings);e.target.previousElementSibling.textContent=`${settings.masterVolume}%`;}

});

render();

import('./wasm/billbill.js').then(({default:createModule})=>createModule()).then(module=>{
  wasmCore={
    start(level){module.ccall('bb_start',null,['number','number'],[level,Date.now()>>>0]);},
    select(card){return Boolean(module.ccall('bb_select_card','number',['number'],[card]));},
    confirm(){return Boolean(module.ccall('bb_confirm','number',[],[]));},
    advance(){return Boolean(module.ccall('bb_advance','number',[],[]));},
    restart(){module.ccall('bb_restart',null,[],[]);},
    state(){const ptr=module.ccall('bb_state_json','number',[],[]);try{return fromWasmState(JSON.parse(module.UTF8ToString(ptr)));}finally{module.ccall('bb_free',null,['number'],[ptr]);}},
  };
}).catch(()=>{ /* JavaScript core remains available during development. */ });

const modelContext = document.modelContext;
if (modelContext?.registerTool) {
  const toolsLifecycle = new AbortController();
  Promise.resolve(modelContext.registerTool({
    name:'start_cpu_match',
    title:'Start CPU Match',
    description:'Start a new BILL BILL match against the selected CPU difficulty.',
    inputSchema:{type:'object',properties:{difficulty:{type:'string',enum:['EASY','NORMAL','HARD']}},required:['difficulty'],additionalProperties:false},
    annotations:{readOnlyHint:false,untrustedContentHint:false},
    execute(input){
      if(!['EASY','NORMAL','HARD'].includes(input?.difficulty)) throw new Error('Invalid difficulty');
      difficulty=input.difficulty;newLocalGame();screen='local';render();
      return {started:true,difficulty,round:game.round};
    },
  },{signal:toolsLifecycle.signal})).catch(()=>{});
  Promise.resolve(modelContext.registerTool({
    name:'get_game_status',
    title:'Get Game Status',
    description:'Read the current BILL BILL screen and local match round.',
    inputSchema:{type:'object',properties:{},additionalProperties:false},
    annotations:{readOnlyHint:true,untrustedContentHint:false},
    execute(){return {screen,round:game?.round??null,difficulty:game?difficulty:null};},
  },{signal:toolsLifecycle.signal})).catch(()=>{});
}




