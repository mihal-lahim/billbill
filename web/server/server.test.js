import test from 'node:test';
import assert from 'node:assert/strict';
import { Match, resolveRound, validateName } from './game.js';

test('rules and name validation', () => {
  assert.equal(resolveRound(12, 15, 13), 1);
  assert.equal(resolveRound(12, 13, 15), 2);
  assert.equal(resolveRound(12, 11, 9), 0);
  assert.equal(resolveRound(12, 13, 13), 0);
  assert.equal(validateName(' ミハル '), 'ミハル');
  assert.equal(validateName('   '), null);
  assert.equal(validateName('1234567890123'), null);
});

test('server match consumes one card and hides pending bid', () => {
  const match = new Match('m1', [{id:'a',name:'A'},{id:'b',name:'B'}]);
  assert.equal(match.submit(1, 23), null);
  assert.equal('bids' in match.publicState(2), false);
  const result = match.submit(2, 21);
  assert.equal(result.p1Bid, 23);
  assert.equal(match.hands[1].includes(23), false);
  assert.equal(match.hands[2].includes(21), false);
  assert.throws(() => match.submit(1, 23));
});
