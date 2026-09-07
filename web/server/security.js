export class RateLimiter {
  constructor({ limit, windowMs, now = Date.now }) { this.limit = limit; this.windowMs = windowMs; this.now = now; this.entries = new Map(); }
  allow(key) {
    const time = this.now(); const current = this.entries.get(key);
    if (!current || time - current.start >= this.windowMs) { this.entries.set(key, { start: time, count: 1 }); return true; }
    if (current.count >= this.limit) return false;
    current.count++; return true;
  }
  remove(key) { this.entries.delete(key); }
}

export function clientAddress(req) {
  const forwarded = String(req.headers['x-forwarded-for'] || '').split(',')[0].trim();
  return forwarded || req.socket.remoteAddress || 'unknown';
}

export function allowedWebSocketOrigin(origin, host, production) {
  if (!production) return true;
  if (!origin || !host) return false;
  try { const value = new URL(origin); return value.protocol === 'https:' && value.host === host; }
  catch { return false; }
}

export function setSecurityHeaders(req, res) {
  res.setHeader('Content-Security-Policy', "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; img-src 'self' data:; media-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'");
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('X-Frame-Options', 'DENY');
  res.setHeader('Referrer-Policy', 'no-referrer');
  res.setHeader('Permissions-Policy', 'camera=(), microphone=(), geolocation=(), payment=()');
  res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
  res.setHeader('Cross-Origin-Resource-Policy', 'same-origin');
  if (req.headers['x-forwarded-proto'] === 'https') res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
}
