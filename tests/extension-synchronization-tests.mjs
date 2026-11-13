import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';

function check(dir, relative = '') {
  for (const entry of fs.readdirSync(dir, {withFileTypes:true})) {
    const name = path.join(relative, entry.name);
    if (entry.isDirectory()) check(path.join(dir, entry.name), name);
    else for (const browser of ['chromium-development', 'firefox-development'])
      assert.deepEqual(fs.readFileSync(path.join('extension',browser,name)),fs.readFileSync(path.join(dir,entry.name)),`${browser}/${name} differs from shared source`);
  }
}
check('extension/shared');
console.log('Both browser packages match the shared extension source');
