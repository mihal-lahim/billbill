import test from 'node:test';
import assert from 'node:assert/strict';
import WebSocket from 'ws';

const URL = 'ws://localhost:8080/ws';

function client(name) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(URL);
    const inbox = [];
    const waiters = [];
    ws.on('message', data => {
      const message = JSON.parse(String(data));
      const index = waiters.findIndex(w => w.type === message.type);
      if (index >= 0) waiters.splice(index, 1)[0].resolve(message);
      else inbox.push(message);
    });
    ws.on('error', reject);
    ws.on('open', () => {
      ws.send(JSON.stringify({ type:'setPlayerName', name }));
      resolve({
        ws,
        send: (type, payload={}) => ws.send(JSON.stringify({ type, ...payload })),
        next(type) {
          const found = inbox.findIndex(m => m.type === type);
          if (found >= 0) return Promise.resolve(inbox.splice(found, 1)[0]);
          return new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error(`Timeout: ${type}`)), 3000);
            waiters.push({ type, resolve:value => { clearTimeout(timer); resolve(value); } });
          });
        },
      });
    });
  });
}

test('room match completes ten authoritative rounds', async () => {
  const host = await client('ホスト');
  const guest = await client('Guest1842');
  host.send('createRoom');
  const room = await host.next('roomCreated');
  assert.match(room.code, /^\d{6}$/);
  guest.send('joinRoom', { code:room.code });
  const [hostFound, guestFound] = await Promise.all([host.next('matchFound'), guest.next('matchFound')]);
  assert.equal(hostFound.state.players[1].name, 'Guest1842');
  const cards = [1,3,5,7,9,11,13,15,17,19];
  for (const card of cards) {
    host.send('submitBid', { matchId:hostFound.state.matchId, card });
    guest.send('submitBid', { matchId:guestFound.state.matchId, card });
    const [a,b] = await Promise.all([host.next('roundResult'),guest.next('roundResult')]);
    assert.equal(a.result.winner,0);
    assert.equal(b.result.p1Bid,card);
  }
  const [aFinal,bFinal] = await Promise.all([host.next('gameResult'),guest.next('gameResult')]);
  assert.equal(aFinal.result.winner,0);
  assert.equal(bFinal.result.summaries[0].remainingMoney,44);
  host.ws.close(); guest.ws.close();
});

test('random queue matches two unique sessions', async () => {
  const a = await client('SameName');
  const b = await client('SameName');
  a.send('joinQueue');
  a.send('joinQueue');
  b.send('joinQueue');
  const [af,bf] = await Promise.all([a.next('matchFound'),b.next('matchFound')]);
  assert.equal(af.state.matchId,bf.state.matchId);
  assert.notEqual(af.state.you,bf.state.you);
  a.send('leaveMatch');
  await b.next('opponentDisconnected');
  a.ws.close(); b.ws.close();
});
