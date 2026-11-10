import fs from 'node:fs';
import assert from 'node:assert/strict';

const glyphs = new Map();
for (const line of fs.readFileSync('Assets/fonts/ui-font.txt', 'utf8').trim().split(/\r?\n/).slice(1)) {
  const [code, advance] = line.trim().split(/\s+/).map(Number);
  glyphs.set(code, advance);
}
const kern = new Map();
for (const line of fs.readFileSync('Assets/fonts/ui-kern.txt', 'utf8').trim().split(/\r?\n/)) {
  const [left, right, amount] = line.trim().split(/\s+/).map(Number);
  kern.set(`${left}:${right}`, amount);
}
for (const file of ['src/ui.cpp', 'src/constructor_ui.cpp', 'src/interface_text.hpp', 'src/log_text.hpp', 'src/extension_installer.cpp']) {
  const source = fs.readFileSync(file, 'utf8');
  for (const match of source.matchAll(/"(?:\\.|[^"\\])*"/g)) {
    for (const character of match[0]) {
      const code = character.codePointAt(0);
      assert(code <= 127 || glyphs.has(code), `${file}: missing glyph ${character} (U+${code.toString(16).toUpperCase()})`);
    }
  }
}
function measure(text, size) {
  let sum = 0, previous = 0;
  for (const character of text) {
    const code = character.codePointAt(0);
    sum += (glyphs.get(code) ?? glyphs.get(63)) + (kern.get(`${previous}:${code}`) ?? 0);
    previous = code;
  }
  return sum * size / 48;
}
function wrappedLineCount(text, size, width) {
  let line = '', count = 1;
  for (const word of text.split(/\s+/)) {
    const next = line ? `${line} ${word}` : word;
    if (line && measure(next, size) > width) { ++count; line = word; }
    else line = next;
    assert(measure(word, size) <= width, `Unbreakable word is too wide: ${word}`);
  }
  return count;
}
const guide = fs.readFileSync('src/extension_installer.cpp', 'utf8');
for (const name of ['titles', 'bodies', 'firefoxTitles', 'firefoxBodies', 'titlesEn', 'bodiesEn', 'firefoxTitlesEn', 'firefoxBodiesEn']) {
  const array = guide.match(new RegExp(`const wchar_t \\*${name}\\[\\] = \\{([\\s\\S]*?)\\};`));
  assert(array, `Missing guide text array: ${name}`);
  const values = [...array[1].matchAll(/L"([^"\\]*)"/g)].map(match => match[1]);
  assert.equal(values.length, 8, `${name}: expected eight guide stages`);
  for (const value of values) {
    if (name.toLowerCase().includes('titles')) assert(measure(value, 21) <= 432, `${name}: title too wide: ${value}`);
    else assert(wrappedLineCount(value, 13, 432) <= 4, `${name}: body exceeds four lines: ${value}`);
  }
}
console.log('PASS bundled font covers interface text and bilingual guide stages fit the panel');
