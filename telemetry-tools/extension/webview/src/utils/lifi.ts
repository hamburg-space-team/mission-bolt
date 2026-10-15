// EXP2 LiFi rounds: joins each EXP2_BER sample with its EXP2_RX chunks (by
// round_seq), rebuilds the sent block from pattern + seed and finds every
// bit error position. Same generator as shared/lifi/lifi_pattern.hpp and
// tools/lifi-errors.py.

export const BLOCK = 64;
export const BITS = BLOCK * 8;
export const BAUD = [9600, 38400, 115200, 460800, 921600]; // ICD-003 rate table
const CHUNK = 40; // PayloadExp2Rx::data
const CAPTURE = 512; // EXP2 sends the first 512 received bytes
const PRBS7 = 0x00;
const PRBS15 = 0x01;

function lfsrBytes(state: number, order: number, n: number): number[] {
  const mask = (1 << order) - 1;
  const out: number[] = [];
  for (let k = 0; k < n; k++) {
    let byte = 0;
    for (let i = 0; i < 8; i++) {
      const bit = ((state >> (order - 1)) ^ (state >> (order - 2))) & 1;
      state = ((state << 1) | bit) & mask;
      byte = (byte << 1) | bit;
    }
    out.push(byte);
  }
  return out;
}

export function expectedBlock(pattern: number, seed: number): number[] | null {
  if (pattern === PRBS15) return lfsrBytes(seed & 0x7fff || 0x7fff, 15, BLOCK);
  if (pattern === PRBS7) return lfsrBytes(0x7f, 7, BLOCK);
  return null;
}

export interface Round {
  seq: number;
  tick: number;
  rateIndex: number;
  threshold: number;
  valid: boolean;
  bitErrors: number;
  rxCount: number;
  offset: number;
  seed: number;
  pattern: number;
  latencyUs: number;
  chunkCount: number;
  chunks: (number[] | undefined)[];
  // filled once every chunk is in
  received?: number[];
  expected?: number[];
  errorBits?: number[]; // block bit positions, 0 = MSB of byte 0
  missingBytes?: number; // block bytes that never arrived
}

export interface Cell {
  rounds: number; // valid rounds
  invalid: number;
  errBits: number;
  bits: number;
}

const toNum = (v: unknown) => Number(v ?? 0);

export class LifiState {
  private pending = new Map<number, Round>();
  latest?: Round; // last round whose bytes are complete (or that has none)
  cells = new Map<string, Cell>(); // `${rateIndex}:${threshold}`
  hist: number[] = new Array(BITS).fill(0);
  histRounds = 0; // valid rounds folded into hist

  reset(): void {
    this.pending.clear();
    this.latest = undefined;
    this.cells.clear();
    this.hist = new Array(BITS).fill(0);
    this.histRounds = 0;
  }

  addBer(s: Record<string, unknown>, tick: number): void {
    const valid = s.measurement_valid === true || s.measurement_valid === 1;
    const rxCount = toNum(s.rx_count);
    const seq = toNum(s.round_seq);
    const r: Round = {
      seq, tick, valid, rxCount,
      rateIndex: toNum(s.rate_index),
      threshold: toNum(s.threshold),
      bitErrors: toNum(s.bit_errors),
      offset: toNum(s.block_offset),
      seed: toNum(s.seed),
      pattern: toNum(s.pattern),
      latencyUs: toNum(s.latency_us),
      // EXP2 sends chunks only for a valid round
      chunkCount: valid ? Math.ceil(Math.min(rxCount, CAPTURE) / CHUNK) : 0,
      chunks: this.pending.get(seq)?.chunks ?? [],
    };
    const key = `${r.rateIndex}:${r.threshold}`;
    const c = this.cells.get(key) ?? { rounds: 0, invalid: 0, errBits: 0, bits: 0 };
    if (valid) {
      c.rounds++;
      c.errBits += r.bitErrors;
      c.bits += toNum(s.bits_sent);
    } else c.invalid++;
    this.cells.set(key, c);
    this.pending.set(seq, r);
    this.tryFinish(r);
    this.prune();
  }

  addRx(s: Record<string, unknown>): void {
    const seq = toNum(s.round_seq);
    const r = this.pending.get(seq) ?? this.placeholder(seq);
    r.chunks[toNum(s.chunk_index)] = (s.data as number[]) ?? [];
    this.pending.set(seq, r);
    this.tryFinish(r);
  }

  private placeholder(seq: number): Round {
    // a chunk ahead of its EXP2_BER: hold it until the BER names the round
    return { seq, tick: 0, rateIndex: 0, threshold: 0, valid: false, bitErrors: 0, rxCount: 0, offset: 0,
      seed: 0, pattern: -1, latencyUs: 0, chunkCount: -1, chunks: [] };
  }

  private tryFinish(r: Round): void {
    if (r.chunkCount < 0) return; // BER not seen yet
    for (let i = 0; i < r.chunkCount; i++) if (!r.chunks[i]) return;
    this.pending.delete(r.seq);
    if (r.valid) this.score(r);
    this.latest = r;
  }

  private score(r: Round): void {
    const exp = expectedBlock(r.pattern, r.seed);
    if (!exp) return;
    const received = r.chunks.slice(0, r.chunkCount).flatMap((c) => c ?? []);
    const bits: number[] = [];
    let missing = 0;
    for (let i = 0; i < BLOCK; i++) {
      const got = received[r.offset + i];
      if (got === undefined) missing++;
      const diff = got === undefined ? 0xff : got ^ exp[i];
      for (let b = 0; b < 8; b++) if (diff & (0x80 >> b)) bits.push(i * 8 + b);
    }
    r.received = received;
    r.expected = exp;
    r.errorBits = bits;
    r.missingBytes = missing;
    for (const p of bits) this.hist[p]++;
    this.histRounds++;
  }

  private prune(): void {
    // a lost chunk must not pin a round forever
    while (this.pending.size > 32) this.pending.delete(this.pending.keys().next().value as number);
  }
}
