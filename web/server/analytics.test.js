import test from 'node:test';
import assert from 'node:assert/strict';
import { Analytics, japanDate, nextReportDelay } from './analytics.js';

test('Japan date and next 21:00 report are calculated in JST', () => {
  const now = new Date('2026-09-07T11:00:00Z');
  assert.equal(japanDate(now), '2026-09-07');
  assert.equal(nextReportDelay(now), 60 * 60 * 1000);
});

test('daily report contains anonymous aggregate counts', async () => {
  let payload;
  const analytics = new Analytics({ file: 'unused.json', webhookUrl: 'https://discord.invalid', now: () => new Date('2026-09-08T12:00:00Z'), fetchImpl: async (_url, options) => { payload = JSON.parse(options.body); return { ok: true }; } });
  analytics.data = { date: '2026-09-07', counters: { pageViews: 3, cpuMatchesStarted: 2, cpuMatchesCompleted: 1, onlineMatchesStarted: 0, onlineMatchesCompleted: 0, disconnects: 0 } };
  analytics.queueSave = () => {};
  await analytics.flushOverdue();
  assert.match(payload.content, /アクセス: 3/);
  assert.match(payload.content, /CPU戦: 開始 2 \/ 完走 1/);
  assert.doesNotMatch(payload.content, /IP|名前|name/i);
});
