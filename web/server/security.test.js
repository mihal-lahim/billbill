import test from 'node:test';
import assert from 'node:assert/strict';
import { RateLimiter, allowedWebSocketOrigin } from './security.js';

test('rate limiter rejects excess requests and resets after its window', () => {
  let time = 1000; const limiter = new RateLimiter({ limit: 2, windowMs: 100, now: () => time });
  assert.equal(limiter.allow('a'), true); assert.equal(limiter.allow('a'), true); assert.equal(limiter.allow('a'), false);
  time += 101; assert.equal(limiter.allow('a'), true);
});

test('production websocket accepts only same-host HTTPS origins', () => {
  assert.equal(allowedWebSocketOrigin('https://billbill.onrender.com', 'billbill.onrender.com', true), true);
  assert.equal(allowedWebSocketOrigin('https://evil.example', 'billbill.onrender.com', true), false);
  assert.equal(allowedWebSocketOrigin('', 'billbill.onrender.com', true), false);
  assert.equal(allowedWebSocketOrigin('', 'localhost:8080', false), true);
});
