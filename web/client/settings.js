const KEY = 'billbill.settings.v1';

export function guestName() {
  return `Guest${Math.floor(1000 + Math.random() * 9000)}`;
}

export function validateName(raw) {
  const name = String(raw ?? '').trim();
  if (!name) return { ok: false, error: 'Please enter a name.' };
  if ([...name].length > 12) return { ok: false, error: 'Name is too long.' };
  if (/\p{Cc}|[\r\n]/u.test(name)) return { ok: false, error: 'Name contains unsupported characters.' };
  return { ok: true, value: name };
}

export class BrowserSettingsStore {
  load() {
    try {
      const parsed = JSON.parse(localStorage.getItem(KEY) || '{}');
      const checked = validateName(parsed.playerName);
      return {
        playerName: checked.ok ? checked.value : guestName(),
        masterVolume: Math.max(0, Math.min(100, Number(parsed.masterVolume) || 100)),
        muted: Boolean(parsed.muted),
      };
    } catch {
      return { playerName: guestName(), masterVolume: 100, muted: false };
    }
  }

  save(settings) {
    localStorage.setItem(KEY, JSON.stringify({
      playerName: settings.playerName,
      masterVolume: settings.masterVolume,
      muted: settings.muted,
    }));
  }
}
