import { randomInt } from 'node:crypto';

export const MONEY = [1,3,5,7,9,11,13,15,17,19,21,23];
export const BUILDINGS = [2,4,6,8,10,12,14,16,18,20];

export function validateName(raw) {
  if (typeof raw !== 'string') return null;
  const value = raw.trim();
  if (!value || [...value].length > 12 || /\p{Cc}|[\r\n]/u.test(value)) return null;
  return value;
}

export function shuffle(values) {
  const result = [...values];
  for (let i = result.length - 1; i > 0; i--) {
    const j = randomInt(i + 1);
    [result[i], result[j]] = [result[j], result[i]];
  }
  return result;
}

export function resolveRound(building, p1, p2) {
  if (p1 === p2) return 0;
  if (p1 > building && p1 > p2) return 1;
  if (p2 > building && p2 > p1) return 2;
  return 0;
}

export class Match {
  constructor(id, players) {
    this.id = id;
    this.players = players;
    this.deck = shuffle(BUILDINGS);
    this.round = 1;
    this.hands = [[], [...MONEY], [...MONEY]];
    this.used = [[], [], []];
    this.acquired = [[], [], []];
    this.discarded = [];
    this.bids = new Map();
    this.finished = false;
  }

  publicState(forPlayer) {
    return {
      matchId: this.id,
      round: this.round,
      currentBuilding: this.deck[this.round - 1],
      you: forPlayer,
      players: this.players.map(p => ({ name: p.name })),
      hands: this.hands,
      used: this.used,
      acquired: this.acquired,
      discarded: this.discarded,
      submitted: this.bids.has(forPlayer),
    };
  }

  submit(player, card) {
    if (this.finished || ![1, 2].includes(player)) throw new Error('Bidding is closed.');
    if (this.bids.has(player)) throw new Error('You already submitted this round.');
    if (!Number.isInteger(card) || !this.hands[player].includes(card)) throw new Error('That card is not available.');
    this.bids.set(player, card);
    if (this.bids.size < 2) return null;

    const building = this.deck[this.round - 1];
    const p1Bid = this.bids.get(1);
    const p2Bid = this.bids.get(2);
    const winner = resolveRound(building, p1Bid, p2Bid);
    for (const p of [1, 2]) {
      const bid = this.bids.get(p);
      this.hands[p] = this.hands[p].filter(v => v !== bid);
      this.used[p].push(bid);
    }
    if (winner) this.acquired[winner].push(building);
    else this.discarded.push(building);
    this.bids.clear();
    const result = { building, p1Bid, p2Bid, winner, round: this.round };
    this.round++;
    if (this.round > 10) this.finished = true;
    return result;
  }

  finalResult() {
    const summaries = [1, 2].map(p => ({
      buildingCount: this.acquired[p].length,
      buildingTotal: this.acquired[p].reduce((a, b) => a + b, 0),
      remainingMoney: this.hands[p].reduce((a, b) => a + b, 0),
      points: 0,
    }));
    for (const key of ['buildingCount', 'buildingTotal', 'remainingMoney']) {
      if (summaries[0][key] > summaries[1][key]) summaries[0].points++;
      if (summaries[1][key] > summaries[0][key]) summaries[1].points++;
    }
    return { summaries, winner: summaries[0].points === summaries[1].points ? 0 : summaries[0].points > summaries[1].points ? 1 : 2 };
  }
}
