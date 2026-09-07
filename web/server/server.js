import { createServer } from 'node:http';
import { randomBytes, randomInt } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { fileURLToPath } from 'node:url';
import { WebSocketServer, WebSocket } from 'ws';
import { Match, validateName } from './game.js';
import { Analytics } from './analytics.js';

const root = fileURLToPath(new URL('../client/', import.meta.url));
const port = Number(process.env.PORT || 8080);
const sessions = new Map();
const queue = [];
const rooms = new Map();
const matches = new Map();
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
  try {
    const url = new URL(req.url, 'http://local');
    if (url.pathname === '/api/analytics' && req.method === 'POST') {
      let body = '';
      for await (const chunk of req) { body += chunk; if (body.length > 4096) throw new Error('request too large'); }
      const event = JSON.parse(body || '{}').event;
      if (!analytics.record(event)) { res.writeHead(400, { 'content-type': 'application/json' }); return res.end(JSON.stringify({ ok: false })); }
      res.writeHead(204, { 'cache-control': 'no-store' }); return res.end();
    }
    const relative = url.pathname === '/' ? 'index.html' : decodeURIComponent(url.pathname.slice(1));
    const path = normalize(join(root, relative));
    if (!path.startsWith(root)) throw new Error('invalid path');
    const data = await readFile(path);
    res.writeHead(200, { 'content-type': contentTypes[extname(path)] || 'application/octet-stream', 'cache-control':'no-store' });
    res.end(data);
  } catch {
    res.writeHead(404); res.end('Not found');
  }
});

const wss = new WebSocketServer({ server, path: '/ws' });
wss.on('connection', ws => {
  const id = randomBytes(16).toString('hex');
  const session = { id, name: `Guest${randomInt(1000, 10000)}`, ws, matchId: null, playerNumber: 0, roomCode: null };
  sessions.set(id, session);
  send(ws, 'connected', { sessionId: id, waiting: queue.length });

  ws.on('message', raw => {
    let msg;
    try { msg = JSON.parse(String(raw)); } catch { return send(ws, 'error', { message: 'Invalid message.' }); }
    try {
      if (msg.type === 'setPlayerName') {
        const name = validateName(msg.name);
        if (!name) throw new Error('Invalid player name.');
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
        const code = roomCode();
        rooms.set(code, { hostId: id, closed: false });
        session.roomCode = code;
        send(ws, 'roomCreated', { code, host: session.name });
      } else if (msg.type === 'joinRoom') {
        if (!/^\d{6}$/.test(String(msg.code || ''))) throw new Error('Invalid room code.');
        const room = rooms.get(String(msg.code));
        if (!room) throw new Error('Room does not exist.');
        if (room.closed) throw new Error('Room has ended.');
        if (room.guestId) throw new Error('Room is full.');
        room.guestId = id; room.closed = true;
        startMatch([room.hostId, id]);
      } else if (msg.type === 'cancelRoom') {
        if (session.roomCode) rooms.delete(session.roomCode);
        session.roomCode = null;
      } else if (msg.type === 'submitBid') {
        const match = matches.get(msg.matchId);
        if (!match || match.id !== session.matchId) throw new Error('Match is invalid.');
        const result = match.submit(session.playerNumber, msg.card);
        send(ws, 'bidAccepted');
        if (result) {
          for (let p = 1; p <= 2; p++) {
            const target = sessions.get(match.players[p - 1].id);
            send(target.ws, 'roundResult', { result, state: match.publicState(p) });
            if (match.finished) send(target.ws, 'gameResult', { result: match.finalResult(), state: match.publicState(p) });
          }
          if (match.finished) analytics.record('onlineMatchesCompleted');
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
    sessions.delete(id);
  });
});

await analytics.init();
server.listen(port, () => console.log(`BILL BILL: http://localhost:${port} (Discord reports: ${process.env.DISCORD_WEBHOOK_URL ? 'enabled' : 'disabled'})`));



