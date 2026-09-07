import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const source = await readFile(new URL('../client/app.js', import.meta.url), 'utf8');

test('online bid requires selection and explicit confirmation', () => {
  assert.match(source, /if\(onlineCard\)\{online\.selectedCard=Number/);
  assert.doesNotMatch(source, /if\(onlineCard\).*send\('submitBid'/);
  assert.match(source, /action==='online-bid'.*send\('submitBid'/);
  assert.match(source, /data-action="online-bid".*決定 \/ BID!/);
});

test('random back leaves queue and CPU toolbar leaves match', () => {
  assert.match(source, /action==='random-back'.*send\('leaveQueue'/);
  assert.match(source, /button\('BACK','random-back'/);
  assert.match(source, /button\('LEAVE MATCH','leave-local'/);
  assert.match(source, /action==='leave-local'.*game=null;screen='title'/);
});
