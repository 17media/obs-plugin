const fs = require('fs');
const path = require('path');

function parseCsv(text) {
  const rows = [];
  let row = [];
  let field = '';
  let inQuotes = false;

  for (let i = 0; i < text.length; i++) {
    const ch = text[i];

    if (inQuotes) {
      if (ch === '"') {
        const next = text[i + 1];
        if (next === '"') {
          field += '"';
          i++;
        } else {
          inQuotes = false;
        }
      } else {
        field += ch;
      }
      continue;
    }

    if (ch === '"') {
      inQuotes = true;
      continue;
    }

    if (ch === ',') {
      row.push(field);
      field = '';
      continue;
    }

    if (ch === '\n') {
      row.push(field);
      field = '';
      const isEmptyRow = row.length === 1 && row[0] === '';
      if (!isEmptyRow) rows.push(row);
      row = [];
      continue;
    }

    if (ch === '\r') continue;

    field += ch;
  }

  if (field.length > 0 || row.length > 0) {
    row.push(field);
    const isEmptyRow = row.length === 1 && row[0] === '';
    if (!isEmptyRow) rows.push(row);
  }

  return rows;
}

function main() {
  const repoRoot = path.resolve(__dirname, '..', '..', '..');
  const samplesPath = path.join(repoRoot, 'temp', 'docs', 'p3', 'enter_animation', 'samples.csv');
  const outDir = path.join(repoRoot, 'web', 'ably_chat', 'public', 'mock');

  const csv = fs.readFileSync(samplesPath, 'utf8');
  const rows = parseCsv(csv);

  const header = (rows[0] || []).map((h) =>
    String(h || '')
      .replace(/^\uFEFF/, '')
      .trim()
  );
  const idxType = header.findIndex((h) => h.toLowerCase() === 'type');
  const idxAnimation = header.findIndex((h) => h.toLowerCase() === 'animation');
  const idxPayload = header.findIndex((h) => h.toLowerCase().includes('payload'));

  if (idxType < 0 || idxAnimation < 0 || idxPayload < 0) {
    throw new Error(`Unexpected CSV header: ${header.join(',')}`);
  }

  const items = [];
  for (let i = 1; i < rows.length; i++) {
    const r = rows[i];
    if (!r || r.length <= idxPayload) continue;
    const t = String(r[idxType] || '').trim();
    if (t !== '27') continue;
    const animation = String(r[idxAnimation] || '').trim();
    const payloadStr = String(r[idxPayload] || '').trim();
    if (!payloadStr) continue;

    let obj;
    try {
      obj = JSON.parse(payloadStr);
    } catch (e) {
      throw new Error(`Failed to parse payload JSON at row ${i + 1}: ${e.message}`);
    }

    items.push({ animation: Number(animation) || animation, obj });
  }

  items.sort((a, b) => {
    const ax = typeof a.animation === 'number' ? a.animation : Number.MAX_SAFE_INTEGER;
    const bx = typeof b.animation === 'number' ? b.animation : Number.MAX_SAFE_INTEGER;
    if (ax !== bx) return ax - bx;
    return String(a.animation).localeCompare(String(b.animation));
  });

  if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });

  const list = [];
  for (const it of items) {
    const name = String(it.animation).padStart(2, '0');
    const filename = `chat_enter_animation_${name}.json`;
    const filePath = path.join(outDir, filename);
    fs.writeFileSync(filePath, JSON.stringify(it.obj, null, 2) + '\n', 'utf8');
    list.push(it.obj);
  }

  const indexPath = path.join(outDir, 'chat_enter_animation_samples.json');
  fs.writeFileSync(indexPath, JSON.stringify(list, null, 2) + '\n', 'utf8');

  process.stdout.write(
    `Generated ${items.length} enter animation mocks + index: ${path.relative(repoRoot, indexPath)}\n`
  );
}

main();
