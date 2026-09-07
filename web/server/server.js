import { createServer } from 'node:http';
import { randomBytes, randomInt } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { fileURLToPath } from 'node:url';
import { WebSocketServer, WebSocket } from 'ws';
import { Match, validateName } from './game.js';
import { Analytics } from './analytics.js';
import { RateLimiter, allowedWebSocketOrigin, clientAddress, setSecurityHeaders } from './security.js';

const root = fileURLToPath(new URL('../client/', import.meta.url));
const port = Number(process.env.PORT || 8080);
const sessions = new Map();
const queue = [];
const rooms = new Map();
const matches = new Map();
const production = process.env.NODE_ENV === 'production';
const analyticsRate = new RateLimiter({ limit: 30, windowMs: 60_000 });
const connectionRate = new RateLimiter({ limit: 20, windowMs: 60_000 });
const analytics = new Analytics({
  file: join(process.env.RENDER_DISK_PATH || join(process.cwd(), 'data'), 'analytics.json'),
  webhookUrl: process.env.DISCORD_WEBHOOK_URL || '',
});
const contentTypes = { '.html':'text/html; charset=utf-8', '.js':'text/javascript; charset=utf-8', '.css':'text/css; charset=utf-8', '.wasm':'application/wasm', '.mp3':'audio/mpeg' };

function send(ws, type, payload = {}) {
  if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ type, ...payload }));
}
function queueStatus() {
  for (const s of sessions.values()) send(s.ws, 'queueStatus', { waiting: queue.length });
}
function removeFromQueue(id) {
  const index = queue.indexOf(id);
  if (index >= 0) queue.splice(index, 1);
  queueStatus();
}
function roomCode() {
  let code;
  do code = String(randomInt(100000, 1000000)); while (rooms.has(code));
  return code;
}
function startMatch(ids) {
  if (new Set(ids).size !== 2) return;
  const players = ids.map(id => sessions.get(id));
  if (players.some(p => !p)) return;
  const id = randomBytes(12).toString('hex');
  const match = new Match(id, players.map(p => ({ id: p.id, name: p.name })));
  matches.set(id, match);
  analytics.record('onlineMatchesStarted');
  players.forEach((p, i) => {
    p.matchId = id;
    p.playerNumber = i + 1;
    send(p.ws, 'matchFound', { state: match.publicState(i + 1) });
  });
}
function peerFor(session) {
  const match = matches.get(session.matchId);
  if (!match) return null;
  const peer = match.players.find(p => p.id !== session.id);
  return peer ? sessions.get(peer.id) : null;
}

const server = createServer(async (req, res) => {
  setSecurityHeaders(req, res);
  try {
    const url = new URL(req.url, 'http://local');
    if (url.pathname === '/api/analytics' && req.method === 'POST') {
      if (production && !allowedWebSocketOrigin(req.headers.origin, req.headers.host, true)) { res.writeHead(403); return res.end(); }
      if (!analyticsRate.allow(clientAddress(req))) { res.writeHead(429, { 'retry-after': '60' }); return res.end(); }
      let body = '';
      for await (const chunk of req) { body += chunk; if (body.length > 4096) throw new Error('request too large'); }
      const event = JSON.parse(body || '{}').event;
      if (!analytics.record(event)) { res.writeHead(400, { 'content-type': 'application/json' }); return res.end(JSON.stringify({ ok: false })); }
      res.writeHead(204, { 'cache-control': 'no-store' }); return res.end();
    }
    if (!['GET', 'HEAD'].includes(req.method)) { res.writeHead(405, { allow: 'GET, HEAD' }); return res.end(); }
    const relative = url.pathname === '/' ? 'index.html' : decodeURIComponent(url.pathname.slice(1));
    const path = normalize(join(root, relative));
    if (!path.startsWith(root)) throw new Error('invalid path');
    const data = await readFile(path);
    res.writeHead(200, { 'content-type': contentTypes[extname(path)] || 'application/octet-stream', 'cache-control':'no-store' });
    res.end(req.method === 'HEAD' ? undefined : data);
  } catch {
    res.writeHead(404); res.end('Not found');
  }
});
server.headersTimeout = 10_000;
server.requestTimeout = 10_000;
server.keepAliveTimeout = 5_000;

const wss = new WebSocketServer({
  server, path: '/ws', maxPayload: 16 * 1024, perMessageDeflate: false,
  verifyClient(info, done) {
    if (sessions.size >= 500) return done(false, 503, 'Server Full');
    const address = clientAddress(info.req);
    if (!connectionRate.allow(address)) return done(false, 429, 'Too Many Requests');
    if (!allowedWebSocketOrigin(info.origin, info.req.headers.host, production)) return done(false, 403, 'Forbidden');
    done(true);
  },
});
wss.on('connection', ws => {
  const id = randomBytes(16).toString('hex');
  const messageRate = new RateLimiter({ limit: 60, windowMs: 10_000 });
  const roomJoinRate = new RateLimiter({ limit: 10, windowMs: 60_000 });
  const session = { id, name: `Guest${randomInt(1000, 10000)}`, ws, matchId: null, playerNumber: 0, roomCode: null };
  sessions.set(id, session);
  send(ws, 'connected', { sessionId: id, waiting: queue.length });

  ws.on('message', raw => {
    if (!messageRate.allow(id)) { send(ws, 'error', { message: '操作が多すぎます。しばらく待ってください。' }); return ws.close(1008, 'Rate limit'); }
    let msg;
    try { msg = JSON.parse(String(raw)); } catch { return send(ws, 'error', { message: '無効な操作です。' }); }
    try {
      if (msg.type === 'setPlayerName') {
        const name = validateName(msg.name);
        if (!name) throw new Error('プレイヤー名が無効です。');
        session.name = name;
        send(ws, 'nameAccepted', { name });
      } else if (msg.type === 'getQueueStatus') {
        send(ws, 'queueStatus', { waiting: queue.length });
      } else if (msg.type === 'joinQueue') {
        if (session.matchId || queue.includes(id)) return;
        queue.push(id); queueStatus();
        while (queue.length >= 2) startMatch([queue.shift(), queue.shift()]);
        queueStatus();
      } else if (msg.type === 'leaveQueue') {
        removeFromQueue(id);
      } else if (msg.type === 'createRoom') {
        if (session.matchId) throw new Error('すでに対戦中です。');
        if (session.roomCode) rooms.delete(session.roomCode);
        const code = roomCode();
        rooms.set(code, { hostId: id, closed: false });
        session.roomCode = code;
        send(ws, 'roomCreated', { code, host: session.name });
      } else if (msg.type === 'joinRoom') {
        if (!roomJoinRate.allow(id)) throw new Error('ルーム参加の試行回数が多すぎます。しばらく待ってください。');
        if (session.matchId) throw new Error('すでに対戦中です。');
        if (!/^\d{6}$/.test(String(msg.code || ''))) throw new Error('ルームコードが無効です。');
        const room = rooms.get(String(msg.code));
        if (!room) throw new Error('ルームが見つかりません。');
        if (room.hostId === id) throw new Error('自分で作成したルームには参加できません。');
        if (room.closed) throw new Error('このルームは終了しています。');
        if (room.guestId) throw new Error('このルームは満員です。');
        room.guestId = id; room.closed = true;
        startMatch([room.hostId, id]);
        rooms.delete(String(msg.code));
      } else if (msg.type === 'cancelRoom') {
        if (session.roomCode) rooms.delete(session.roomCode);
        session.roomCode = null;
      } else if (msg.type === 'submitBid') {
        const match = matches.get(msg.matchId);
        if (!match || match.id !== session.matchId) throw new Error('対戦情報が無効です。');
        const result = match.submit(session.playerNumber, msg.card);
        send(ws, 'bidAccepted');
        if (result) {
          for (let p = 1; p <= 2; p++) {
            const target = sessions.get(match.players[p - 1].id);
            send(target.ws, 'roundResult', { result, state: match.publicState(p) });
            if (match.finished) send(target.ws, 'gameResult', { result: match.finalResult(), state: match.publicState(p) });
          }
          if (match.finished) {
            analytics.record('onlineMatchesCompleted');
            for (const player of match.players) { const finished = sessions.get(player.id); if (finished) finished.matchId = null; }
            matches.delete(match.id);
          }
        }
      } else if (msg.type === 'leaveMatch') {
        const peer = peerFor(session);
        if (peer) send(peer.ws, 'opponentDisconnected');
        matches.delete(session.matchId); session.matchId = null;
      }
    } catch (error) { send(ws, 'error', { message: error.message }); }
  });

  ws.on('close', () => {
    removeFromQueue(id);
    if (session.roomCode) rooms.delete(session.roomCode);
    const peer = peerFor(session);
    if (peer) { send(peer.ws, 'opponentDisconnected'); analytics.record('disconnects'); }
    if (session.matchId) matches.delete(session.matchId);
    messageRate.remove(id);
    sessions.delete(id);
  });
});

await analytics.init();
server.listen(port, () => console.log(`BILL BILL: http://localhost:${port} (Discord reports: ${process.env.DISCORD_WEBHOOK_URL ? 'enabled' : 'disabled'})`));

