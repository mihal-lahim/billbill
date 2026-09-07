import { mkdir, readFile, rename, writeFile } from 'node:fs/promises';
import { dirname } from 'node:path';

const EMPTY = Object.freeze({ pageViews: 0, cpuMatchesStarted: 0, cpuMatchesCompleted: 0, onlineMatchesStarted: 0, onlineMatchesCompleted: 0, disconnects: 0 });
const ALLOWED_EVENTS = new Set(Object.keys(EMPTY));
const japanDate = (now = new Date()) => new Intl.DateTimeFormat('en-CA', { timeZone: 'Asia/Tokyo', year: 'numeric', month: '2-digit', day: '2-digit' }).format(now);

function nextReportDelay(now = new Date()) {
  const parts = Object.fromEntries(new Intl.DateTimeFormat('en-US', { timeZone: 'Asia/Tokyo', hour12: false, year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' }).formatToParts(now).filter(p => p.type !== 'literal').map(p => [p.type, Number(p.value)]));
  const current = Date.UTC(parts.year, parts.month - 1, parts.day, parts.hour % 24, parts.minute, parts.second);
  let target = Date.UTC(parts.year, parts.month - 1, parts.day, 21, 0, 0);
  if (target <= current) target += 86400000;
  return target - current;
}

export class Analytics {
  constructor({ file, webhookUrl, fetchImpl = fetch, now = () => new Date() }) {
    this.file = file; this.webhookUrl = webhookUrl; this.fetchImpl = fetchImpl; this.now = now;
    this.data = { date: japanDate(now()), counters: { ...EMPTY } }; this.saveQueue = Promise.resolve(); this.timer = null;
  }
  async init() {
    try { const saved = JSON.parse(await readFile(this.file, 'utf8')); if (saved?.date && saved?.counters) this.data = saved; }
    catch (error) { if (error.code !== 'ENOENT') console.warn('Analytics load failed:', error.message); }
    await this.flushOverdue(); this.schedule();
  }
  record(event) {
    if (!ALLOWED_EVENTS.has(event)) return false;
    this.data.counters[event] = (this.data.counters[event] || 0) + 1; this.queueSave(); return true;
  }
  queueSave() {
    this.saveQueue = this.saveQueue.then(async () => { await mkdir(dirname(this.file), { recursive: true }); const temp = `${this.file}.tmp`; await writeFile(temp, JSON.stringify(this.data), 'utf8'); await rename(temp, this.file); }).catch(error => console.warn('Analytics save failed:', error.message));
  }
  async flushOverdue(force = false) {
    const today = japanDate(this.now()); if (!force && this.data.date === today) return false;
    const c = this.data.counters; const total = Object.values(c).reduce((sum, value) => sum + Number(value || 0), 0);
    if (this.webhookUrl && total > 0) {
      const response = await this.fetchImpl(this.webhookUrl, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ username: 'ビルビル 日次レポート', allowed_mentions: { parse: [] }, content: [`📊 **ビルビル 日次レポート (${this.data.date})**`, `アクセス: ${c.pageViews || 0}`, `CPU戦: 開始 ${c.cpuMatchesStarted || 0} / 完走 ${c.cpuMatchesCompleted || 0}`, `オンライン戦: 開始 ${c.onlineMatchesStarted || 0} / 完走 ${c.onlineMatchesCompleted || 0}`, `切断: ${c.disconnects || 0}`].join('\n') }) });
      if (!response.ok) throw new Error(`Discord webhook returned ${response.status}`);
    }
    this.data = { date: today, counters: { ...EMPTY } }; this.queueSave(); return total > 0;
  }
  schedule() {
    clearTimeout(this.timer);
    this.timer = setTimeout(async () => { try { await this.flushOverdue(true); } catch (error) { console.warn('Discord daily report failed:', error.message); } this.schedule(); }, nextReportDelay(this.now()));
    this.timer.unref?.();
  }
}
export { japanDate, nextReportDelay };

