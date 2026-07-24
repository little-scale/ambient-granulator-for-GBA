(function attachBankTools(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  root.GbaGranulatorBank = api;
})(typeof globalThis === "object" ? globalThis : this, function bankFactory() {
  "use strict";

  const BANK_MAGIC = "GBAGRN01";
  const BANK_VERSION = 1;
  const TARGET_RATE = 16384;
  const HEADER_SIZE = 64;
  const ENTRY_SIZE = 64;
  const MAX_ENTRIES = 64;
  const DATA_OFFSET = HEADER_SIZE + ENTRY_SIZE * MAX_ENTRIES;
  const SAMPLE_FORMAT_PCM8_MONO = 1;
  const WAVEFORM_COLUMNS = 240;
  const WAVEFORM_BYTES = WAVEFORM_COLUMNS * 2;
  const NORMALIZED_PEAK = Math.pow(10, -0.3 / 20);
  const encoder = new TextEncoder();
  const decoder = new TextDecoder("utf-8");

  function readU32(bytes, offset) {
    return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
      .getUint32(offset, true);
  }

  function writeU32(view, offset, value) {
    view.setUint32(offset, value >>> 0, true);
  }

  function align32(value) {
    return (value + 31) & ~31;
  }

  function rangeValid(offset, length, limit) {
    return offset >= 0 && length >= 0 && offset <= limit && length <= limit - offset;
  }

  function findAll(bytes, needle) {
    const matches = [];
    outer: for (let index = 0; index <= bytes.length - needle.length; index++) {
      for (let part = 0; part < needle.length; part++) {
        if (bytes[index + part] !== needle[part]) continue outer;
      }
      matches.push(index);
    }
    return matches;
  }

  function validateGbaHeader(rom) {
    if (!(rom instanceof Uint8Array) || rom.length < 0xc0)
      throw new Error("This file is too small to be a GBA ROM.");
    if (rom.length > 32 * 1024 * 1024)
      throw new Error("The ROM exceeds the GBA 32 MiB address space.");
    if (rom[0xb2] !== 0x96)
      throw new Error("The GBA fixed header byte is invalid.");
    let sum = 0;
    for (let index = 0xa0; index < 0xbd; index++) sum += rom[index];
    const expected = (-sum - 0x19) & 0xff;
    if (rom[0xbd] !== expected)
      throw new Error("The GBA header checksum is invalid.");
    const readText = (start, end) => decoder.decode(rom.subarray(start, end))
      .replace(/[\0 ]+$/g, "");
    return {
      title: readText(0xa0, 0xac),
      gameCode: readText(0xac, 0xb0),
      makerCode: readText(0xb0, 0xb2),
    };
  }

  function locateBank(rom) {
    const header = validateGbaHeader(rom);
    const matches = findAll(rom, encoder.encode(BANK_MAGIC));
    if (matches.length !== 1) {
      throw new Error(matches.length
        ? "ROM contains more than one GBAGRN01 bank signature."
        : "No GBAGRN01 sample bank found. Load a compatible Ambient Granulator GBA ROM.");
    }
    const offset = matches[0];
    const bank = rom.subarray(offset);
    const version = readU32(bank, 8);
    const capacity = readU32(bank, 12);
    const count = readU32(bank, 16);
    const used = readU32(bank, 20);
    const sampleRate = readU32(bank, 24);
    if (version !== BANK_VERSION)
      throw new Error(`Unsupported bank version ${version}.`);
    if (capacity < DATA_OFFSET || !rangeValid(offset, capacity, rom.length))
      throw new Error("The ROM sample-bank capacity is invalid.");
    if (count < 1 || count > MAX_ENTRIES)
      throw new Error("The ROM sample count is invalid.");
    if (used < DATA_OFFSET || used > capacity)
      throw new Error("The ROM sample-bank used-byte count is invalid.");
    if (sampleRate !== TARGET_RATE
        || readU32(bank, 28) !== ENTRY_SIZE
        || readU32(bank, 32) !== MAX_ENTRIES
        || readU32(bank, 36) !== DATA_OFFSET
        || readU32(bank, 40) !== SAMPLE_FORMAT_PCM8_MONO
        || readU32(bank, 44) !== WAVEFORM_COLUMNS)
      throw new Error("The ROM uses an incompatible GBA sample-bank layout.");
    return { offset, capacity, count, used, sampleRate, header };
  }

  const crcTable = new Uint32Array(256);
  for (let index = 0; index < 256; index++) {
    let value = index;
    for (let bit = 0; bit < 8; bit++)
      value = (value >>> 1) ^ ((value & 1) ? 0xedb88320 : 0);
    crcTable[index] = value >>> 0;
  }

  function crc32(bytes) {
    let crc = 0xffffffff;
    for (const byte of bytes)
      crc = (crc >>> 8) ^ crcTable[(crc ^ byte) & 0xff];
    return (crc ^ 0xffffffff) >>> 0;
  }

  function readName(bank, offset) {
    let end = offset;
    while (end < offset + 32 && bank[end] !== 0) end++;
    return decoder.decode(bank.subarray(offset, end));
  }

  function decodeBank(rom, location) {
    const bank = rom.subarray(location.offset, location.offset + location.capacity);
    const samples = [];
    for (let index = 0; index < location.count; index++) {
      const entry = HEADER_SIZE + index * ENTRY_SIZE;
      const pcmOffset = readU32(bank, entry + 32);
      const pcmLength = readU32(bank, entry + 36);
      const expectedCrc = readU32(bank, entry + 40);
      const waveformOffset = readU32(bank, entry + 44);
      const waveformLength = readU32(bank, entry + 48);
      if (pcmLength < 1
          || (pcmOffset & 31) !== 0
          || (waveformOffset & 31) !== 0
          || !rangeValid(pcmOffset, pcmLength, location.used)
          || waveformLength !== WAVEFORM_BYTES
          || !rangeValid(waveformOffset, waveformLength, location.used))
        throw new Error(`Sample entry ${index + 1} has invalid offsets or lengths.`);
      const pcm = bank.subarray(pcmOffset, pcmOffset + pcmLength);
      if (crc32(pcm) !== expectedCrc)
        throw new Error(`Sample entry ${index + 1} failed its CRC32 check.`);
      const data = new Float32Array(pcmLength);
      for (let frame = 0; frame < pcmLength; frame++) {
        const signed = pcm[frame] < 128 ? pcm[frame] : pcm[frame] - 256;
        data[frame] = signed / 128;
      }
      samples.push({
        id: `rom-${index}-${expectedCrc.toString(16)}`,
        name: readName(bank, entry) || `Sample ${index + 1}`,
        sourceRate: TARGET_RATE,
        data,
        trimStart: 0,
        trimEnd: data.length,
        gainDb: 0,
        origin: "ROM bank",
      });
    }
    return samples;
  }

  function normalizeSamples(data) {
    let peak = 0;
    for (const value of data) peak = Math.max(peak, Math.abs(value));
    const output = new Float32Array(data.length);
    if (peak === 0) return output;
    const gain = NORMALIZED_PEAK / peak;
    for (let index = 0; index < data.length; index++)
      output[index] = Math.max(-1, Math.min(1, data[index] * gain));
    return output;
  }

  function resampleSample(sample) {
    const start = Math.max(0, Math.min(sample.data.length - 1,
      Math.round(sample.trimStart)));
    const end = Math.max(start + 1, Math.min(sample.data.length,
      Math.round(sample.trimEnd)));
    const inputLength = end - start;
    const outputLength = Math.max(1,
      Math.floor((inputLength * TARGET_RATE + sample.sourceRate / 2)
        / sample.sourceRate));
    const output = new Float32Array(outputLength);
    const gain = Math.pow(10, sample.gainDb / 20);
    for (let index = 0; index < outputLength; index++) {
      const numerator = index * sample.sourceRate;
      const left = Math.floor(numerator / TARGET_RATE);
      const fraction = numerator % TARGET_RATE;
      let value;
      if (left >= inputLength - 1) {
        value = sample.data[end - 1];
      } else {
        const first = sample.data[start + left];
        const second = sample.data[start + left + 1];
        value = first + (second - first) * fraction / TARGET_RATE;
      }
      output[index] = Math.max(-1, Math.min(1, value * gain));
    }
    return output;
  }

  function quantizePcm8(data) {
    const pcm = new Uint8Array(data.length);
    let peak = 0;
    for (let index = 0; index < data.length; index++) {
      let value = Math.floor(data[index] * 128 + 0.5);
      value = Math.max(-127, Math.min(127, value));
      peak = Math.max(peak, Math.abs(value));
      pcm[index] = value & 0xff;
    }
    return { pcm, peak };
  }

  function prepareSample(sample) {
    const quantized = quantizePcm8(resampleSample(sample));
    const data = new Float32Array(quantized.pcm.length);
    for (let index = 0; index < quantized.pcm.length; index++) {
      const value = quantized.pcm[index];
      data[index] = (value < 128 ? value : value - 256) / 128;
    }
    return { ...quantized, data };
  }

  function waveformSummary(pcm) {
    const minimums = new Uint8Array(WAVEFORM_COLUMNS);
    const maximums = new Uint8Array(WAVEFORM_COLUMNS);
    for (let column = 0; column < WAVEFORM_COLUMNS; column++) {
      const start = Math.floor(column * pcm.length / WAVEFORM_COLUMNS);
      let end = Math.floor((column + 1) * pcm.length / WAVEFORM_COLUMNS);
      if (end <= start) end = Math.min(pcm.length, start + 1);
      let minimum = 127;
      let maximum = -127;
      const fallback = pcm.length ? pcm[pcm.length - 1] : 0;
      if (start >= pcm.length) {
        minimum = fallback < 128 ? fallback : fallback - 256;
        maximum = minimum;
      } else {
        for (let index = start; index < end; index++) {
          const value = pcm[index] < 128 ? pcm[index] : pcm[index] - 256;
          minimum = Math.min(minimum, value);
          maximum = Math.max(maximum, value);
        }
      }
      minimums[column] = minimum & 0xff;
      maximums[column] = maximum & 0xff;
    }
    return { minimums, maximums };
  }

  function safeName(name) {
    return String(name).replace(/[^\x20-\x7e]/g, " ").trim().slice(0, 31)
      || "UNTITLED";
  }

  function estimatedBankBytes(samples) {
    let cursor = DATA_OFFSET;
    for (const sample of samples) {
      const inputLength = Math.max(1,
        Math.round(sample.trimEnd) - Math.round(sample.trimStart));
      const outputLength = Math.max(1,
        Math.floor((inputLength * TARGET_RATE + sample.sourceRate / 2)
          / sample.sourceRate));
      const waveformOffset = align32(cursor);
      const pcmOffset = align32(waveformOffset + WAVEFORM_BYTES);
      cursor = align32(pcmOffset + outputLength);
    }
    return cursor;
  }

  function buildBank(samples, capacity) {
    if (!samples.length) throw new Error("Add at least one sample.");
    if (samples.length > MAX_ENTRIES)
      throw new Error(`A bank can contain at most ${MAX_ENTRIES} samples.`);
    if (capacity < DATA_OFFSET)
      throw new Error("The ROM bank capacity is smaller than its entry table.");

    const prepared = samples.map(sample => {
      const quantized = prepareSample(sample);
      return { ...quantized, waveform: waveformSummary(quantized.pcm) };
    });
    const locations = [];
    let cursor = DATA_OFFSET;
    for (const item of prepared) {
      const waveformOffset = align32(cursor);
      const pcmOffset = align32(waveformOffset + WAVEFORM_BYTES);
      const end = pcmOffset + item.pcm.length;
      if (end > capacity)
        throw new Error(`Bank exceeds ROM capacity by ${end - capacity} bytes.`);
      locations.push({ waveformOffset, pcmOffset });
      cursor = align32(end);
    }

    const output = new Uint8Array(capacity);
    const view = new DataView(output.buffer);
    output.set(encoder.encode(BANK_MAGIC), 0);
    writeU32(view, 8, BANK_VERSION);
    writeU32(view, 12, capacity);
    writeU32(view, 16, samples.length);
    writeU32(view, 20, cursor);
    writeU32(view, 24, TARGET_RATE);
    writeU32(view, 28, ENTRY_SIZE);
    writeU32(view, 32, MAX_ENTRIES);
    writeU32(view, 36, DATA_OFFSET);
    writeU32(view, 40, SAMPLE_FORMAT_PCM8_MONO);
    writeU32(view, 44, WAVEFORM_COLUMNS);

    prepared.forEach((item, index) => {
      const entry = HEADER_SIZE + index * ENTRY_SIZE;
      const name = encoder.encode(safeName(samples[index].name));
      const { waveformOffset, pcmOffset } = locations[index];
      output.set(name, entry);
      writeU32(view, entry + 32, pcmOffset);
      writeU32(view, entry + 36, item.pcm.length);
      writeU32(view, entry + 40, crc32(item.pcm));
      writeU32(view, entry + 44, waveformOffset);
      writeU32(view, entry + 48, WAVEFORM_BYTES);
      writeU32(view, entry + 52, 0);
      output.set(item.waveform.minimums, waveformOffset);
      output.set(item.waveform.maximums,
        waveformOffset + WAVEFORM_COLUMNS);
      output.set(item.pcm, pcmOffset);
    });
    return output;
  }

  function patchRom(rom, location, samples) {
    const originalHeader = rom.slice(0, 0xc0);
    const output = rom.slice();
    output.set(buildBank(samples, location.capacity), location.offset);
    for (let index = 0; index < originalHeader.length; index++) {
      if (output[index] !== originalHeader[index])
        throw new Error("Patching unexpectedly changed the GBA header.");
    }
    const reopened = locateBank(output);
    decodeBank(output, reopened);
    return output;
  }

  function formatBytes(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  }

  return {
    BANK_MAGIC,
    BANK_VERSION,
    TARGET_RATE,
    HEADER_SIZE,
    ENTRY_SIZE,
    MAX_ENTRIES,
    DATA_OFFSET,
    SAMPLE_FORMAT_PCM8_MONO,
    WAVEFORM_COLUMNS,
    WAVEFORM_BYTES,
    NORMALIZED_PEAK,
    validateGbaHeader,
    locateBank,
    decodeBank,
    normalizeSamples,
    resampleSample,
    quantizePcm8,
    prepareSample,
    waveformSummary,
    estimatedBankBytes,
    buildBank,
    patchRom,
    crc32,
    formatBytes,
  };
});
