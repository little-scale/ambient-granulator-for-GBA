(function patcherApplication() {
  "use strict";

  const bank = globalThis.GbaGranulatorBank;
  if (!bank) throw new Error("GBA sample-bank tools did not load.");

  const elements = Object.fromEntries([
    "rom-zone", "rom-input", "rom-label", "audio-zone", "audio-input",
    "audio-label", "export-button", "status", "budget-text", "budget-fill",
    "sample-count", "sample-list", "move-up", "move-down", "remove",
    "play", "stop", "waveform", "name", "gain", "trim-in", "trim-out",
    "source-stat", "duration-stat", "size-stat", "origin-stat",
  ].map(id => [id, document.getElementById(id)]));

  const state = {
    rom: null,
    romName: "",
    location: null,
    samples: [],
    selectedId: null,
    audioContext: null,
    playingSource: null,
    draggingBoundary: null,
    draggedSample: null,
    nextId: 1,
  };

  function selectedSample() {
    return state.samples.find(sample => sample.id === state.selectedId) || null;
  }

  function makeId() {
    if (globalThis.crypto && typeof globalThis.crypto.randomUUID === "function")
      return globalThis.crypto.randomUUID();
    return `sample-${Date.now()}-${state.nextId++}`;
  }

  function setStatus(message, kind) {
    elements.status.textContent = message;
    elements.status.className = `status${kind ? ` ${kind}` : ""}`;
  }

  function outputLength(sample) {
    const inputLength = Math.max(1,
      Math.round(sample.trimEnd) - Math.round(sample.trimStart));
    return Math.max(1, Math.floor(
      (inputLength * bank.TARGET_RATE + sample.sourceRate / 2)
      / sample.sourceRate));
  }

  function renderBudget() {
    const capacity = state.location ? state.location.capacity : 8 * 1024 * 1024;
    const estimated = state.samples.length
      ? bank.estimatedBankBytes(state.samples) : bank.DATA_OFFSET;
    const over = estimated > capacity || state.samples.length > bank.MAX_ENTRIES;
    const usage = Math.min(100, estimated / capacity * 100);
    elements["budget-text"].textContent = `${bank.formatBytes(estimated)} / ${bank.formatBytes(capacity)} · ${state.samples.length}/${bank.MAX_ENTRIES} slots`;
    elements["budget-fill"].style.width = `${usage}%`;
    elements["budget-fill"].className = over ? "over" : "";
    elements["export-button"].disabled = !state.rom || over || !state.samples.length;
  }

  function renderPool() {
    const list = elements["sample-list"];
    list.replaceChildren();
    if (!state.samples.length) {
      const empty = document.createElement("p");
      empty.className = "empty";
      empty.textContent = "No samples in the bank.";
      list.append(empty);
    }
    state.samples.forEach((sample, index) => {
      const row = document.createElement("button");
      row.type = "button";
      row.className = `pool-row${sample.id === state.selectedId ? " selected" : ""}`;
      row.draggable = true;
      row.dataset.id = sample.id;
      row.setAttribute("aria-label", `Select sample ${index + 1}: ${sample.name}`);
      const number = document.createElement("span");
      number.textContent = String(index + 1).padStart(2, "0");
      const name = document.createElement("span");
      name.className = "pool-name";
      name.textContent = sample.name;
      const duration = document.createElement("span");
      duration.textContent = `${((sample.trimEnd - sample.trimStart)
        / sample.sourceRate).toFixed(2)}s`;
      row.append(number, name, duration);
      row.addEventListener("click", () => {
        state.selectedId = sample.id;
        render();
      });
      row.addEventListener("dragstart", event => {
        state.draggedSample = sample.id;
        row.classList.add("dragging");
        if (event.dataTransfer) event.dataTransfer.effectAllowed = "move";
      });
      row.addEventListener("dragend", () => {
        state.draggedSample = null;
        row.classList.remove("dragging");
      });
      row.addEventListener("dragover", event => {
        event.preventDefault();
        if (event.dataTransfer) event.dataTransfer.dropEffect = "move";
      });
      row.addEventListener("drop", event => {
        event.preventDefault();
        if (!state.draggedSample || state.draggedSample === sample.id) return;
        const from = state.samples.findIndex(item => item.id === state.draggedSample);
        const to = state.samples.findIndex(item => item.id === sample.id);
        if (from < 0 || to < 0) return;
        const next = state.samples.slice();
        const [moved] = next.splice(from, 1);
        next.splice(to, 0, moved);
        state.samples = next;
        render();
      });
      list.append(row);
    });
    elements["sample-count"].textContent = String(state.samples.length);
  }

  function drawWaveform() {
    const canvas = elements.waveform;
    const context = canvas.getContext("2d");
    const width = canvas.width;
    const height = canvas.height;
    const sample = selectedSample();
    context.fillStyle = "#000";
    context.fillRect(0, 0, width, height);
    context.strokeStyle = "#202020";
    context.lineWidth = 1;
    for (let x = 0; x <= width; x += width / 8) {
      context.beginPath();
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
    }
    context.strokeStyle = "#555";
    context.beginPath();
    context.moveTo(0, height / 2);
    context.lineTo(width, height / 2);
    context.stroke();
    if (!sample) return;

    const startX = sample.trimStart / sample.data.length * width;
    const endX = sample.trimEnd / sample.data.length * width;
    const prepared = bank.prepareSample(sample).data;
    const framesPerPixel = sample.data.length / width;
    const displayGain = Math.pow(10, sample.gainDb / 20);
    for (let x = 0; x < width; x++) {
      const inside = x >= startX && x <= endX;
      const source = inside ? prepared : sample.data;
      const first = inside
        ? Math.floor((x - startX) / Math.max(1, endX - startX) * source.length)
        : Math.floor(x * framesPerPixel);
      const last = inside
        ? Math.min(source.length, Math.max(first + 1,
          Math.ceil((x + 1 - startX) / Math.max(1, endX - startX)
            * source.length)))
        : Math.min(source.length,
          Math.max(first + 1, Math.ceil((x + 1) * framesPerPixel)));
      let minimum = 1;
      let maximum = -1;
      for (let frame = first; frame < last; frame++) {
        const raw = source[Math.min(frame, source.length - 1)];
        const value = inside ? raw : Math.max(-1, Math.min(1, raw * displayGain));
        minimum = Math.min(minimum, value);
        maximum = Math.max(maximum, value);
      }
      context.strokeStyle = inside ? "#fff" : "#444";
      context.beginPath();
      context.moveTo(x + .5, height / 2 - maximum * height * .43);
      context.lineTo(x + .5, height / 2 - minimum * height * .43);
      context.stroke();
    }

    [[startX, "IN"], [endX, "OUT"]].forEach(([x, label]) => {
      context.strokeStyle = "#fff";
      context.lineWidth = 2;
      context.beginPath();
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
      context.fillStyle = "#fff";
      context.fillRect(label === "IN" ? x : x - 38, 0, 38, 18);
      context.fillStyle = "#000";
      context.font = "12px ui-monospace, monospace";
      context.fillText(label, label === "IN" ? x + 7 : x - 32, 13);
    });
  }

  function renderEditor() {
    const sample = selectedSample();
    const disabled = !sample;
    ["name", "gain", "trim-in", "trim-out", "play"].forEach(id => {
      elements[id].disabled = disabled;
    });
    elements.stop.disabled = !state.playingSource;
    elements.remove.disabled = disabled;
    const index = sample ? state.samples.findIndex(item => item.id === sample.id) : -1;
    elements["move-up"].disabled = index <= 0;
    elements["move-down"].disabled = index < 0 || index >= state.samples.length - 1;

    elements.name.value = sample ? sample.name : "";
    elements.gain.value = sample ? String(sample.gainDb) : "0";
    elements["trim-in"].value = sample
      ? (sample.trimStart / sample.sourceRate * 1000).toFixed(1) : "0";
    elements["trim-out"].value = sample
      ? (sample.trimEnd / sample.sourceRate * 1000).toFixed(1) : "0";
    elements["source-stat"].textContent = sample ? `${sample.sourceRate} HZ` : "—";
    elements["duration-stat"].textContent = sample
      ? `${(outputLength(sample) / bank.TARGET_RATE).toFixed(3)} S` : "—";
    elements["size-stat"].textContent = sample
      ? bank.formatBytes(outputLength(sample) + bank.WAVEFORM_BYTES) : "—";
    elements["origin-stat"].textContent = sample ? sample.origin : "—";
    drawWaveform();
  }

  function render() {
    renderBudget();
    renderPool();
    renderEditor();
  }

  function updateSelected(patch) {
    state.samples = state.samples.map(sample => sample.id === state.selectedId
      ? { ...sample, ...patch } : sample);
    render();
  }

  function stopPreview() {
    if (state.playingSource) {
      try { state.playingSource.stop(); } catch (_) { /* already stopped */ }
    }
    state.playingSource = null;
    elements.stop.disabled = true;
  }

  function audioContext() {
    if (!state.audioContext) {
      const Context = globalThis.AudioContext || globalThis.webkitAudioContext;
      if (!Context) throw new Error("This browser does not provide Web Audio decoding.");
      state.audioContext = new Context();
    }
    return state.audioContext;
  }

  async function loadRom(file) {
    if (!file) return;
    try {
      stopPreview();
      const bytes = new Uint8Array(await file.arrayBuffer());
      const location = bank.locateBank(bytes);
      const samples = bank.decodeBank(bytes, location);
      state.rom = bytes;
      state.romName = file.name;
      state.location = location;
      state.samples = samples;
      state.selectedId = samples[0] ? samples[0].id : null;
      elements["rom-label"].textContent = file.name;
      elements["audio-zone"].classList.remove("disabled");
      elements["audio-input"].disabled = false;
      elements["audio-label"].textContent = "drop multiple files or click";
      setStatus(`Loaded ${file.name}: ${samples.length} sample${samples.length === 1 ? "" : "s"}, ${bank.formatBytes(location.used)} used · ${location.header.title || "GBA ROM"} / ${location.header.gameCode || "----"}.`, "success");
      render();
    } catch (error) {
      setStatus(error instanceof Error ? error.message : String(error), "error");
    }
  }

  async function addAudio(files) {
    const incoming = Array.from(files || []).filter(file =>
      /\.(wav|aif|aiff|mp3|m4a|ogg|flac)$/i.test(file.name)
      || /^audio\//i.test(file.type));
    if (!incoming.length) {
      setStatus("No browser-decodable audio files were selected.", "error");
      return;
    }
    try {
      const context = audioContext();
      const added = [];
      for (const file of incoming) {
        const decoded = await context.decodeAudioData(
          (await file.arrayBuffer()).slice(0));
        const mono = new Float32Array(decoded.length);
        for (let channel = 0; channel < decoded.numberOfChannels; channel++) {
          const source = decoded.getChannelData(channel);
          for (let frame = 0; frame < mono.length; frame++)
            mono[frame] += source[frame] / decoded.numberOfChannels;
        }
        const data = bank.normalizeSamples(mono);
        added.push({
          id: makeId(),
          name: file.name.replace(/\.[^.]+$/, "").slice(0, 31) || "UNTITLED",
          sourceRate: decoded.sampleRate,
          data,
          trimStart: 0,
          trimEnd: data.length,
          gainDb: 0,
          origin: file.name,
        });
      }
      const room = Math.max(0, bank.MAX_ENTRIES - state.samples.length);
      const accepted = added.slice(0, room);
      state.samples = state.samples.concat(accepted);
      if (accepted[0]) state.selectedId = accepted[0].id;
      setStatus(`Added ${accepted.length} sample${accepted.length === 1 ? "" : "s"}. Adjust trim and gain, then export.`, "success");
      render();
    } catch (error) {
      setStatus(`Audio decode failed: ${error instanceof Error ? error.message : String(error)}`, "error");
    }
  }

  function preview() {
    const sample = selectedSample();
    if (!sample) return;
    try {
      stopPreview();
      const data = bank.prepareSample(sample).data;
      const context = audioContext();
      const buffer = context.createBuffer(1, data.length, bank.TARGET_RATE);
      buffer.copyToChannel(data, 0);
      const source = context.createBufferSource();
      source.buffer = buffer;
      source.connect(context.destination);
      source.onended = () => {
        if (state.playingSource === source) {
          state.playingSource = null;
          elements.stop.disabled = true;
        }
      };
      source.start();
      state.playingSource = source;
      elements.stop.disabled = false;
    } catch (error) {
      setStatus(error instanceof Error ? error.message : String(error), "error");
    }
  }

  function moveSelected(direction) {
    const index = state.samples.findIndex(sample => sample.id === state.selectedId);
    const target = index + direction;
    if (index < 0 || target < 0 || target >= state.samples.length) return;
    const next = state.samples.slice();
    [next[index], next[target]] = [next[target], next[index]];
    state.samples = next;
    render();
  }

  function removeSelected() {
    const index = state.samples.findIndex(sample => sample.id === state.selectedId);
    if (index < 0) return;
    stopPreview();
    state.samples.splice(index, 1);
    const next = state.samples[Math.min(index, state.samples.length - 1)];
    state.selectedId = next ? next.id : null;
    render();
  }

  function exportRom() {
    if (!state.rom || !state.location) return;
    try {
      stopPreview();
      const output = bank.patchRom(state.rom, state.location, state.samples);
      const blob = new Blob([output], { type: "application/octet-stream" });
      const link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = state.romName.replace(/\.gba$/i, "") + "-patched.gba";
      link.click();
      setTimeout(() => URL.revokeObjectURL(link.href), 1000);
      setStatus(`Patched and revalidated ${state.samples.length} sample${state.samples.length === 1 ? "" : "s"} in ${link.download}.`, "success");
    } catch (error) {
      setStatus(error instanceof Error ? error.message : String(error), "error");
    }
  }

  function dropFiles(event, handler) {
    event.preventDefault();
    handler(event.dataTransfer.files);
  }

  function armDropZone(zone, callback, enabled) {
    zone.addEventListener("dragover", event => {
      event.preventDefault();
      if (enabled()) zone.classList.add("armed");
    });
    zone.addEventListener("dragleave", () => zone.classList.remove("armed"));
    zone.addEventListener("drop", event => {
      zone.classList.remove("armed");
      if (enabled()) dropFiles(event, callback);
      else event.preventDefault();
    });
  }

  function canvasSampleAt(event) {
    const sample = selectedSample();
    if (!sample) return 0;
    const rect = elements.waveform.getBoundingClientRect();
    const fraction = Math.max(0, Math.min(1,
      (event.clientX - rect.left) / rect.width));
    return Math.round(fraction * sample.data.length);
  }

  function moveTrimBoundary(event) {
    const sample = selectedSample();
    if (!sample || !state.draggingBoundary) return;
    const position = canvasSampleAt(event);
    if (state.draggingBoundary === "start")
      updateSelected({ trimStart: Math.min(position, sample.trimEnd - 1) });
    else
      updateSelected({ trimEnd: Math.max(position, sample.trimStart + 1) });
  }

  elements.waveform.addEventListener("pointerdown", event => {
    const sample = selectedSample();
    if (!sample) return;
    elements.waveform.setPointerCapture(event.pointerId);
    const position = canvasSampleAt(event);
    state.draggingBoundary = Math.abs(position - sample.trimStart)
      <= Math.abs(position - sample.trimEnd) ? "start" : "end";
    moveTrimBoundary(event);
  });
  elements.waveform.addEventListener("pointermove", moveTrimBoundary);
  ["pointerup", "pointercancel"].forEach(type => {
    elements.waveform.addEventListener(type, () => { state.draggingBoundary = null; });
  });

  elements["rom-input"].addEventListener("change", event =>
    loadRom(event.target.files && event.target.files[0]));
  elements["audio-input"].addEventListener("change", event =>
    addAudio(event.target.files));
  elements["export-button"].addEventListener("click", exportRom);
  elements.play.addEventListener("click", preview);
  elements.stop.addEventListener("click", stopPreview);
  elements["move-up"].addEventListener("click", () => moveSelected(-1));
  elements["move-down"].addEventListener("click", () => moveSelected(1));
  elements.remove.addEventListener("click", removeSelected);
  elements.name.addEventListener("input", event =>
    updateSelected({ name: event.target.value.slice(0, 31) }));
  elements.gain.addEventListener("change", event =>
    updateSelected({ gainDb: Math.max(-48, Math.min(18,
      Number(event.target.value) || 0)) }));
  elements["trim-in"].addEventListener("change", event => {
    const sample = selectedSample();
    if (!sample) return;
    const frame = Math.round((Number(event.target.value) || 0)
      * sample.sourceRate / 1000);
    updateSelected({ trimStart: Math.max(0,
      Math.min(sample.trimEnd - 1, frame)) });
  });
  elements["trim-out"].addEventListener("change", event => {
    const sample = selectedSample();
    if (!sample) return;
    const frame = Math.round((Number(event.target.value) || 0)
      * sample.sourceRate / 1000);
    updateSelected({ trimEnd: Math.min(sample.data.length,
      Math.max(sample.trimStart + 1, frame)) });
  });

  armDropZone(elements["rom-zone"], files => loadRom(files[0]), () => true);
  armDropZone(elements["audio-zone"], addAudio, () => Boolean(state.rom));
  render();
})();
