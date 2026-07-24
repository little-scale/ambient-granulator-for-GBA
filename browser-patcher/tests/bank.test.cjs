"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const bank = require("../bank.js");

const root = path.resolve(__dirname, "../..");
const romPath = path.join(root, "ambient-granulator-for-gba.gba");

function loadRom() {
  return new Uint8Array(fs.readFileSync(romPath));
}

test("locates and validates the built ROM bank", () => {
  const rom = loadRom();
  const location = bank.locateBank(rom);
  const samples = bank.decodeBank(rom, location);
  assert.equal(location.header.title, "AMBGRANULAR");
  assert.equal(location.header.gameCode, "AGRN");
  assert.equal(location.capacity, 8 * 1024 * 1024);
  assert.equal(samples.length, 10);
  const piano = samples.find((sample) => sample.name === "piano");
  assert.ok(piano);
  assert.equal(piano.data.length, 16 * bank.TARGET_RATE);
});

test("rejects absent, duplicate, corrupt and invalid-header banks", () => {
  const rom = loadRom();
  const location = bank.locateBank(rom);
  const missing = rom.slice();
  missing.fill(0, location.offset, location.offset + bank.BANK_MAGIC.length);
  assert.throws(() => bank.locateBank(missing), /No GBAGRN01/);

  const duplicate = rom.slice();
  duplicate.set(new TextEncoder().encode(bank.BANK_MAGIC), 0x100);
  assert.throws(() => bank.locateBank(duplicate), /more than one/);

  const corrupt = rom.slice();
  const first = location.offset + bank.HEADER_SIZE;
  const pcmOffset = new DataView(corrupt.buffer).getUint32(first + 32, true);
  corrupt[location.offset + pcmOffset] ^= 1;
  assert.throws(() => bank.decodeBank(corrupt, location), /CRC32/);

  const badHeader = rom.slice();
  badHeader[0xbd] ^= 1;
  assert.throws(() => bank.locateBank(badHeader), /header checksum/);
});

test("prepares exactly previewable signed PCM8", () => {
  const sample = {
    name: "prepared",
    sourceRate: 32768,
    data: Float32Array.from([-1, -.5, 0, .5, 1]),
    trimStart: 1,
    trimEnd: 5,
    gainDb: 0,
  };
  const prepared = bank.prepareSample(sample);
  assert.equal(prepared.pcm.length, 2);
  assert.deepEqual(Array.from(prepared.pcm), [192, 64]);
  assert.deepEqual(Array.from(prepared.data), [-.5, .5]);
  assert.deepEqual(Array.from(bank.quantizePcm8(prepared.data).pcm),
    Array.from(prepared.pcm));
});

test("normalizes once on import and preserves subsequent baked gain", () => {
  const normalized = bank.normalizeSamples(Float32Array.from([.25, -.5]));
  const normalizedPeak = Math.max(...Array.from(normalized, Math.abs));
  assert.ok(Math.abs(normalizedPeak - bank.NORMALIZED_PEAK) < 1e-6);

  const reduced = bank.prepareSample({
    name: "reduced after import",
    sourceRate: bank.TARGET_RATE,
    data: normalized,
    trimStart: 0,
    trimEnd: normalized.length,
    gainDb: -6,
  });
  assert.equal(reduced.peak, 62);
  assert.notEqual(reduced.peak, 124);
});

test("patches, reopens and preserves every byte outside the bank", () => {
  const rom = loadRom();
  const location = bank.locateBank(rom);
  const samples = bank.decodeBank(rom, location);
  const extra = {
    id: "synthetic",
    name: "SYNTHETIC / TEST",
    sourceRate: bank.TARGET_RATE,
    data: Float32Array.from({ length: 1024 }, (_, index) =>
      Math.sin(index * Math.PI * 2 / 64) * .5),
    trimStart: 16,
    trimEnd: 1000,
    gainDb: -3,
    origin: "unit test",
  };
  const output = bank.patchRom(rom, location, [extra, ...samples]);
  const reopened = bank.locateBank(output);
  const decoded = bank.decodeBank(output, reopened);
  assert.equal(decoded.length, samples.length + 1);
  assert.equal(decoded[0].name, extra.name);
  assert.deepEqual(output.subarray(0, location.offset),
    rom.subarray(0, location.offset));
  assert.deepEqual(output.subarray(location.offset + location.capacity),
    rom.subarray(location.offset + location.capacity));
  assert.deepEqual(output.subarray(0, 0xc0), rom.subarray(0, 0xc0));
});

test("enforces capacity and generates 240-column summaries", () => {
  const sample = {
    name: "too large",
    sourceRate: bank.TARGET_RATE,
    data: new Float32Array(8192),
    trimStart: 0,
    trimEnd: 8192,
    gainDb: 0,
  };
  assert.throws(() => bank.buildBank([sample], bank.DATA_OFFSET + 512),
    /exceeds ROM capacity/);
  const summary = bank.waveformSummary(Uint8Array.from([0, 127, 128, 255]));
  assert.equal(summary.minimums.length, 240);
  assert.equal(summary.maximums.length, 240);
});
