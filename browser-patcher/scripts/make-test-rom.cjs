"use strict";

const fs = require("node:fs");
const path = require("node:path");
const bank = require("../bank.js");

const root = path.resolve(__dirname, "../..");
const sourcePath = path.join(root, "ambient-granulator-for-gba.gba");
const destination = path.join(root, "build", "browser-patcher-test.gba");
const rom = new Uint8Array(fs.readFileSync(sourcePath));
const location = bank.locateBank(rom);
const samples = bank.decodeBank(rom, location);
const synthetic = {
  id: "browser-patcher-test",
  name: "PATCHER TEST TONE",
  sourceRate: bank.TARGET_RATE,
  data: Float32Array.from({ length: bank.TARGET_RATE * 2 }, (_, index) =>
    Math.sin(index * Math.PI * 2 * 220 / bank.TARGET_RATE) * .45),
  trimStart: 0,
  trimEnd: bank.TARGET_RATE * 2,
  gainDb: -3,
  origin: "automated patcher test",
};
const output = bank.patchRom(rom, location, [...samples, synthetic]);
fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.writeFileSync(destination, output);
console.log(`Patched and revalidated ${destination} with ${samples.length + 1} samples`);
