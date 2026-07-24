"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

test("standalone build is one offline HTML file", () => {
  const file = path.resolve(__dirname,
    "../dist/ambient-granulator-gba-patcher.html");
  const html = fs.readFileSync(file, "utf8");
  assert.match(html, /GbaGranulatorBank/);
  assert.match(html, /patcherApplication/);
  assert.match(html, /<style>/);
  assert.doesNotMatch(html, /https?:\/\//);
  assert.doesNotMatch(html, /href="style\.css"|src="(?:bank|app)\.js"/);
});
