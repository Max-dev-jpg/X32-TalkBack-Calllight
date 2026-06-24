// =============================================================================
// TalkBack CallLight – Frontend Application
// =============================================================================

'use strict';

const NUM_TRIGGERS = 4;
let triggerSignalSources = Array(NUM_TRIGGERS).fill(0);

// ── Tab switching ─────────────────────────────────────────────────────────────
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
  });
});

// ── Trigger sub-tab switching ─────────────────────────────────────────────────
document.querySelectorAll('.trig-subtab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.trig-subtab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.trig-sub-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('trig-sub-' + btn.dataset.trig).classList.add('active');
  });
});

// ── Trigger enable toggles ────────────────────────────────────────────────────
function updateTrigBodyVisibility(n) {
  const en   = document.getElementById('t' + n + '-enabled');
  const body = document.getElementById('t' + n + '-body');
  if (en && body) body.classList.toggle('hidden', !en.checked);
  // Update sub-tab dot
  const tab = document.querySelector('.trig-subtab[data-trig="' + n + '"]');
  if (tab) tab.classList.toggle('enabled-dot', en && en.checked);
}
for (let n = 0; n < NUM_TRIGGERS; n++) {
  const cb = document.getElementById('t' + n + '-enabled');
  if (cb) cb.addEventListener('change', () => updateTrigBodyVisibility(n));
  updateTrigBodyVisibility(n);
}

// ── Channel type → update max channel number + disable Meter for DCA ─────────
const CH_MAX = { 0:32, 1:16, 2:6, 3:8, 4:8, 5:8, 6:0, 7:0 };
function onTrigChTypeChange(n) {
  const chType  = parseInt(document.getElementById('t' + n + '-chtype').value);
  const numLbl  = document.getElementById('t' + n + '-chnum-lbl');
  const numInp  = document.getElementById('t' + n + '-chnum');
  const sigSrc  = document.getElementById('t' + n + '-sigsrc');

  const noNum = (chType === 6 || chType === 7); // Main L/R, Main Mono
  if (numLbl) numLbl.style.display = noNum ? 'none' : '';
  if (numInp) {
    numInp.max = CH_MAX[chType] || 32;
    if (!noNum && parseInt(numInp.value) > (CH_MAX[chType] || 32))
      numInp.value = 1;
  }

  // DCA doesn't support meters — hide Meter option
  if (sigSrc) {
    const meterOpt = sigSrc.querySelector('option[value="1"]');
    if (meterOpt) {
      meterOpt.disabled = (chType === 3); // CH_DCA
      if (chType === 3 && parseInt(sigSrc.value) === 1) sigSrc.value = '0';
    }
  }
  onTrigSigSrcChange(n);
}

// Matching curve for Meter dB value<>float conversion
const METER_POINTS = [
  { db: -60, f: 0.0010 }, { db: -55, f: 0.0018 }, { db: -50, f: 0.0032 },
  { db: -45, f: 0.0057 }, { db: -40, f: 0.0101 }, { db: -35, f: 0.0180 },
  { db: -30, f: 0.0320 }, { db: -28, f: 0.0402 }, { db: -26, f: 0.0507 },
  { db: -24, f: 0.0638 }, { db: -22, f: 0.0803 }, { db: -20, f: 0.1010 },
  { db: -18, f: 0.1273 }, { db: -16, f: 0.1602 }, { db: -14, f: 0.2016 },
  { db: -13, f: 0.2263 }, { db: -12, f: 0.2539 }, { db: -11, f: 0.2849 },
  { db: -10, f: 0.3197 }, { db: -9,  f: 0.3585 }, { db: -8,  f: 0.4022 },
  { db: -7,  f: 0.4515 }, { db: -6,  f: 0.5066 }, { db: -5,  f: 0.5684 },
  { db: -4,  f: 0.6378 }, { db: -3,  f: 0.7156 }, { db: -2,  f: 0.8029 },
  { db: -1,  f: 0.9009 }, { db: 0,   f: 1.0000 }
];

function normalizedToDb(value, isMeter = false) {
  const norm = parseFloat(value) || 0;
  
  if (isMeter) {
    if (norm <= 0.0) return -60.0;
    if (norm >= 1.0) return 0.0;
  
    // find correct segment
    for (let i = 0; i < METER_POINTS.length - 1; i++) {
      const p1 = METER_POINTS[i];
      const p2 = METER_POINTS[i+1];
      
      if (norm >= p1.f && norm <= p2.f) {
        // linear interpolation between p1 and p2
        const ratio = (norm - p1.f) / (p2.f - p1.f);
        return p1.db + ratio * (p2.db - p1.db);
      }
    }
    return 0;
  }

  // fader logic
  if (norm <= 0.0) return -90.0;
  if (norm >= 1.0) return 10.0;

  let db;
  if (norm < 0.0625) {
      db = (norm / 0.0625) * 30 - 90;
  } else if (norm < 0.25) {
      db = ((norm - 0.0625) / 0.1875) * 30 - 60;
  } else if (norm < 0.5) {
      db = ((norm - 0.25) / 0.25) * 20 - 30;
  } else {
      db = ((norm - 0.5) / 0.5) * 20 - 10;
  }
  return Math.round(db * 10) / 10;
}

function formatSignalValue(value, signalSource) {
  const val = parseFloat(value) || 0;
  if (signalSource === 0) {
    return normalizedToDb(val, false).toFixed(1) + ' dB';
  }
  if (signalSource === 1) {
    return normalizedToDb(val, true).toFixed(1) + ' dB';
  }
  return val.toFixed(0);
}

function dbToNormalized(value, isMeter = false) {
  const db = parseFloat(value);
  
  if (isMeter) {
    if (!Number.isFinite(db) || db <= -60) return 0.0;
    if (db >= 0) return 1.0;
    for (let i = 0; i < METER_POINTS.length - 1; i++) {
      const p1 = METER_POINTS[i];
      const p2 = METER_POINTS[i+1];
      
      if (db >= p1.db && db <= p2.db) {
        const ratio = (db - p1.db) / (p2.db - p1.db);
        return p1.f + ratio * (p2.f - p1.f);
      }
    }
    return 1.0;
  }

  // fader logic
  if (!Number.isFinite(db) || db <= -90) return 0.0;
  if (db >= 10) return 1.0;

  let norm;
  if (db < -60) {
      norm = ((db + 90) / 30) * 0.0625;
  } else if (db < -30) {
      norm = 0.0625 + ((db + 60) / 30) * 0.1875;
  } else if (db < -10) {
      norm = 0.25 + ((db + 30) / 20) * 0.25;
  } else {
      norm = 0.5 + ((db + 10) / 20) * 0.5;
  }
  return Math.max(0, Math.min(1, norm));
}


function getThresholdValue(n) {
  const range = document.getElementById('t' + n + '-thresh');
  if (!range) return 0;
  const norm = parseFloat(range.dataset.norm);
  return Number.isFinite(norm) ? norm : 0;
}
function updateThresholdDisplay(n) {
  const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  const range  = document.getElementById('t' + n + '-thresh');
  const num    = document.getElementById('t' + n + '-thresh-n');
  if (!range || !num) return;

  const label = range.closest('label');
  const labelText = label ? label.querySelector('.threshold-label-text') : null;
  const valueNorm = getThresholdValue(n);

  const thresholdLabel = document.getElementById('t' + n + '-thresh')?.closest('label');
  const hysteresisLabel = document.getElementById('t' + n + '-hyst')?.closest('label');
  const smoothingLabel = document.getElementById('t' + n + '-smooth')?.closest('label');
  const showThreshold = sigSrc !== 2;

  [thresholdLabel, hysteresisLabel, smoothingLabel].forEach(el => {
    if (el) el.style.display = showThreshold ? '' : 'none';
  });

  if (sigSrc === 2) {
    if (labelText) labelText.textContent = 'Threshold (disabled for Mute)';
    return;
  }

  if (label) label.style.display = '';
  if (sigSrc === 0) {
    const db = normalizedToDb(valueNorm, false);
    range.min = 0;
    range.max = 1;
    range.step = 0.01;
    num.min = -90;
    num.max = 10;
    num.step = 0.1;
    range.value = valueNorm.toFixed(2);
    num.value = db.toFixed(1);
    if (labelText) labelText.textContent = 'Threshold (-90.0 – +10.0 dB)';
  }
  else if (sigSrc === 1) {
    let db = normalizedToDb(valueNorm, true);
    range.min = 0;
    range.max = 1;
    range.step = 0.01;
    num.min = -60; // Startet jetzt bei -60
    num.max = 0;
    num.step = 0.1;
    range.value = valueNorm.toFixed(2);
    num.value = db.toFixed(1);
    if (labelText) labelText.textContent = 'Threshold (-60.0 – 0.0 dB)';
  }
  else {
    range.min = 0;
    range.max = 1;
    range.step = 0.01;
    num.min = 0;
    num.max = 1;
    num.step = 0.01;
    const formatted = valueNorm.toFixed(2);
    range.value = formatted;
    num.value = formatted;
    if (labelText) labelText.textContent = 'Threshold (0.0 – 1.0)';
  }
}
function updateThresholdNorm(n, rawValue, fromRange = false) {
  const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  const norm = fromRange
    ? (parseFloat(rawValue) || 0)
    : ((sigSrc === 0 || sigSrc === 1)
       ? dbToNormalized(rawValue, sigSrc === 1)
       : (parseFloat(rawValue) || 0));
  const range = document.getElementById('t' + n + '-thresh');
  const num   = document.getElementById('t' + n + '-thresh-n');
  if (range) range.dataset.norm = norm;
  if (num)   num.dataset.norm = norm;
}
function onTrigSigSrcChange(n) {
  triggerSignalSources[n] = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  updateThresholdDisplay(n);
}

// ── Per-trigger LED color helpers ─────────────────────────────────────────────
function updateTrigLedColorVisibility(n) {
  const cb  = document.getElementById('t' + n + '-lcc');
  const row = document.getElementById('t' + n + '-color-row');
  if (row) row.classList.toggle('off', !(cb && cb.checked));
}
function applyTrigColorPicker(n, picker) {
  const hex = picker.value;
  const r = parseInt(hex.slice(1,3), 16);
  const g = parseInt(hex.slice(3,5), 16);
  const b = parseInt(hex.slice(5,7), 16);
  const rEl = document.getElementById('t' + n + '-lr');
  const gEl = document.getElementById('t' + n + '-lg');
  const bEl = document.getElementById('t' + n + '-lb');
  if (rEl) rEl.value = r;
  if (gEl) gEl.value = g;
  if (bEl) bEl.value = b;
}
function syncTrigColorPicker(n) {
  const r = parseInt(document.getElementById('t' + n + '-lr').value) || 0;
  const g = parseInt(document.getElementById('t' + n + '-lg').value) || 0;
  const b = parseInt(document.getElementById('t' + n + '-lb').value) || 0;
  const cp = document.getElementById('t' + n + '-lcp');
  if (cp) cp.value = rgbToHex(r, g, b);
}

// ── Talkback section visibility ───────────────────────────────────────────────
function updateTBVisibility() {
  const on = document.getElementById('tb-enabled').checked;
  document.getElementById('tb-engine-fields')  .classList.toggle('hidden', !on);
  document.getElementById('tb-advanced-section').classList.toggle('hidden', !on);
}
document.getElementById('tb-enabled').addEventListener('change', updateTBVisibility);
updateTBVisibility();

// ── Talkback sub-tab switching ────────────────────────────────────────────────
document.querySelectorAll('.tb-subtab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tb-subtab')     .forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tb-sub-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tb-sub-' + btn.dataset.tbsub).classList.add('active');
  });
});

// ── Action-builder state ──────────────────────────────────────────────────────
// Talkback A/B
const tbState = { aOn: [], aOff: [], bOn: [], bOff: [] };
// Per-trigger action lists: trigState[n] = { on: [], off: [] }
const trigState = Array.from({ length: NUM_TRIGGERS }, () => ({ on: [], off: [] }));

const CH_DEFS = [
  { v:0, n:'Input CH',   max:32 },
  { v:4, n:'Aux In',     max:8  },
  { v:5, n:'FX Return',  max:8  },
  { v:1, n:'Mix Bus',    max:16 },
  { v:2, n:'Matrix',     max:6  },
  { v:3, n:'DCA',        max:8  },
  { v:6, n:'Main L/R',   max:0, noNum:true },
  { v:7, n:'Main M/C',   max:0, noNum:true },
];

const ACT_DEFS = [
  { t:'clearSolo', label:'Clear All Solos'                         },
  { t:'solo',      label:'Solo Channel',              hasCh:true   },
  { t:'unsolo',    label:'Unsolo Channel',            hasCh:true   },
  { t:'mute',      label:'Mute Channel',              hasCh:true   },
  { t:'unmute',    label:'Unmute Channel',            hasCh:true   },
  { t:'osc',       label:'Custom OSC Command',        hasOsc:true  },
  { t:'out',       label:'Call Light (this source)',  hasOut:true  },
  { t:'forceout',  label:'Force ALL Outputs OFF (overrides triggers)' },
];

function safeParseJSON(s) {
  try { return JSON.parse(s || '[]') || []; } catch(e) { return []; }
}

// ── Add-action row init ───────────────────────────────────────────────────────
function initAddRow(lk, stateArr) {
  const row = document.getElementById('add-row-' + lk);
  if (!row) return;
  const opts = ACT_DEFS.map(d => `<option value="${d.t}">${d.label}</option>`).join('');
  row.innerHTML =
    `<select id="add-sel-${lk}">${opts}</select>` +
    `<button type="button" class="btn-secondary"` +
    ` onclick="addAction('${lk}', document.getElementById('add-sel-${lk}').value)">+ Add</button>`;
}

// TB action keys
['aOn','aOff','bOn','bOff'].forEach(lk => initAddRow(lk, tbState));
// Trigger action keys
for (let n = 0; n < NUM_TRIGGERS; n++) {
  initAddRow('t' + n + 'On',  trigState[n]);
  initAddRow('t' + n + 'Off', trigState[n]);
}

// ── Unified action state accessor ─────────────────────────────────────────────
function getActionList(lk) {
  if (lk in tbState) return tbState[lk];
  const m = lk.match(/^t(\d+)(On|Off)$/);
  if (m) return trigState[parseInt(m[1])][m[2].toLowerCase()];
  return [];
}
function setActionList(lk, arr) {
  if (lk in tbState) { tbState[lk] = arr; return; }
  const m = lk.match(/^t(\d+)(On|Off)$/);
  if (m) trigState[parseInt(m[1])][m[2].toLowerCase()] = arr;
}

// ── Action CRUD ───────────────────────────────────────────────────────────────
function addAction(lk, type) {
  const def = ACT_DEFS.find(d => d.t === type) || {};
  const a = { t: type };
  if (def.hasCh)  { a.ct = 0; a.cn = 1; }
  if (def.hasOsc) { a.p = ''; a.v = 1;  }
  if (def.hasOut) { a.s = true; }
  getActionList(lk).push(a);
  renderActionList(lk);
}

function removeAction(lk, idx) {
  getActionList(lk).splice(idx, 1);
  renderActionList(lk);
}

function moveAction(lk, idx, dir) {
  const list   = getActionList(lk);
  const newIdx = idx + dir;
  if (newIdx < 0 || newIdx >= list.length) return;
  [list[idx], list[newIdx]] = [list[newIdx], list[idx]];
  renderActionList(lk);
}

function setActParam(lk, idx, key, val) {
  const list = getActionList(lk);
  if (list[idx]) list[idx][key] = val;
}

function onChTypeChange(lk, idx, selEl) {
  const ct = CH_DEFS.find(d => d.v === parseInt(selEl.value)) || CH_DEFS[0];
  setActParam(lk, idx, 'ct', parseInt(selEl.value));
  const card  = selEl.closest('.action-card');
  const numIn = card && card.querySelector('.chnum-inp');
  if (numIn) {
    numIn.max  = ct.max || 32;
    numIn.style.display = ct.noNum ? 'none' : '';
    if (!ct.noNum && parseInt(numIn.value) > (ct.max || 32)) {
      numIn.value = ct.max || 1;
      setActParam(lk, idx, 'cn', ct.max || 1);
    }
  }
}

function renderChOpts(selected) {
  return CH_DEFS.map(d =>
    `<option value="${d.v}"${d.v===selected?' selected':''}>${d.n}</option>`
  ).join('');
}

function renderActionCard(a, lk, i) {
  const def = ACT_DEFS.find(d => d.t === a.t) || { label: a.t };
  let params = '';
  if (def.hasCh) {
    const ct = CH_DEFS.find(d => d.v === (a.ct || 0)) || CH_DEFS[0];
    params = `<div class="action-params">
      <select onchange="onChTypeChange('${lk}',${i},this)">${renderChOpts(a.ct||0)}</select>
      <input type="number" class="chnum-inp" min="1" max="${ct.max||32}"
             value="${a.cn||1}" ${ct.noNum?'style="display:none"':''}
             onchange="setActParam('${lk}',${i},'cn',parseInt(this.value))">
    </div>`;
  } else if (def.hasOsc) {
    const safeP = (a.p || '').replace(/"/g,'&quot;');
    params = `<div class="action-params">
      <input type="text" placeholder="/osc/path" value="${safeP}"
             onchange="setActParam('${lk}',${i},'p',this.value)">
      <input type="number" value="${a.v !== undefined ? a.v : 1}" style="width:70px"
             onchange="setActParam('${lk}',${i},'v',parseInt(this.value))">
    </div>`;
  } else if (def.hasOut) {
    params = `<div class="action-params">
      <select onchange="setActParam('${lk}',${i},'s',this.value==='1')">
        <option value="1"${a.s?' selected':''}>Call Light ON</option>
        <option value="0"${!a.s?' selected':''}>Call Light OFF</option>
      </select>
    </div>`;
  }
  const listLen = getActionList(lk).length;
  return `<div class="action-card">
    <div class="action-card-hdr">
      <span class="act-label">${def.label}</span>
      <div class="act-hdr-btns">
        <button type="button" class="act-mv btn-xs" onclick="moveAction('${lk}',${i},-1)"
          title="Move up"   ${i === 0          ? 'disabled' : ''}>↑</button>
        <button type="button" class="act-mv btn-xs" onclick="moveAction('${lk}',${i},+1)"
          title="Move down" ${i === listLen-1  ? 'disabled' : ''}>↓</button>
        <button type="button" class="act-del" onclick="removeAction('${lk}',${i})">&#xd7;</button>
      </div>
    </div>
    ${params}
  </div>`;
}

function renderActionList(lk) {
  const el = document.getElementById('actions-' + lk);
  if (!el) return;
  const list = getActionList(lk);
  el.innerHTML = list.length
    ? list.map((a,i) => renderActionCard(a, lk, i)).join('')
    : '<div class="act-empty">No actions — nothing will be sent.</div>';
}

// ── Priority state ────────────────────────────────────────────────────────────
let trigPriorityOrder = [0, 1, 2, 3];

function onPrioModeChange() {
  const mode = parseInt(document.getElementById('trig-prio-mode').value || '0');
  const wrap = document.getElementById('trig-prio-order-wrap');
  if (wrap) wrap.style.display = (mode === 1) ? '' : 'none';
}

function renderPrioList() {
  const el = document.getElementById('trig-prio-list');
  if (!el) return;
  el.innerHTML = trigPriorityOrder.map((n, i) =>
    `<div class="prio-item">
      <span class="prio-label">Trigger ${n + 1}</span>
      <div class="prio-btns">
        <button type="button" class="btn-secondary btn-xs"
          onclick="movePrioItem(${i},-1)" ${i === 0 ? 'disabled' : ''}>↑</button>
        <button type="button" class="btn-secondary btn-xs"
          onclick="movePrioItem(${i},1)"  ${i === trigPriorityOrder.length - 1 ? 'disabled' : ''}>↓</button>
      </div>
    </div>`
  ).join('');
}

function movePrioItem(idx, dir) {
  const newIdx = idx + dir;
  if (newIdx < 0 || newIdx >= trigPriorityOrder.length) return;
  [trigPriorityOrder[idx], trigPriorityOrder[newIdx]] =
    [trigPriorityOrder[newIdx], trigPriorityOrder[idx]];
  renderPrioList();
}

// ── OSC Monitor graph ─────────────────────────────────────────────────────────
const MON_HIST_SIZE = 120;
const MON_HIST = Array.from({ length: NUM_TRIGGERS }, () => []);
const monViewMode = Array(NUM_TRIGGERS).fill('log');

function toggleMonitorGraph(n) {
  monViewMode[n] = (monViewMode[n] === 'log') ? 'graph' : 'log';
  const log    = document.getElementById('mon-log-' + n);
  const canvas = document.getElementById('mon-canvas-' + n);
  const btn    = document.getElementById('mon-view-btn-' + n);
  const inGraph = (monViewMode[n] === 'graph');
  if (log)    log.style.display    = inGraph ? 'none' : '';
  if (canvas) canvas.style.display = inGraph ? 'block' : 'none';
  if (btn)    btn.textContent      = inGraph ? '📋 Log' : '📊 Graph';
  if (inGraph && canvas) {
    drawMonitorGraph(n);
  }
}

function drawMonitorGraph(n) {
  const canvas = document.getElementById('mon-canvas-' + n);
  if (!canvas || canvas.style.display === 'none') return;
  const hist = MON_HIST[n];
  const ctx  = canvas.getContext('2d');
  // Match rendered size for crisp pixels
  canvas.width  = canvas.clientWidth  || 200;
  canvas.height = canvas.clientHeight || 80;
  const W = canvas.width, H = canvas.height;

  ctx.fillStyle = '#0d0d0d';
  ctx.fillRect(0, 0, W, H);

  // Horizontal grid lines at 0.25 / 0.5 / 0.75
  ctx.strokeStyle = '#252525'; ctx.lineWidth = 1;
  [0.25, 0.5, 0.75, 1.0].forEach(v => {
    const y = Math.round(H - v * H) + 0.5;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  });

  if (hist.length < 2) return;

  ctx.strokeStyle = '#00e5a0'; ctx.lineWidth = 1.5; ctx.beginPath();
  hist.forEach((v, i) => {
    const x = (i / (hist.length - 1)) * W;
    const y = H - v * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.stroke();
}

// ── Load config → populate forms ──────────────────────────────────────────────
function loadTBConfig(c) {
  const tbEn  = document.getElementById('tb-enabled');
  const tbMon = document.getElementById('tb-monitor');
  const tbBfA = document.getElementById('tb-b-follows-a');
  if (tbEn)  tbEn.checked  = !!c.tbEnabled;
  if (tbMon) tbMon.value   = c.tbMonitor ?? 0;
  if (tbBfA) tbBfA.checked = !!c.tbBFollowsA;

  tbState.aOn  = safeParseJSON(c.tbAOnJson);
  tbState.aOff = safeParseJSON(c.tbAOffJson);
  tbState.bOn  = safeParseJSON(c.tbBOnJson);
  tbState.bOff = safeParseJSON(c.tbBOffJson);
  ['aOn','aOff','bOn','bOff'].forEach(renderActionList);
  updateTBVisibility();
}

// Function for Rounding Floats (Because JSON (De)serialization can lead to very long floats)
function roundInput(v, digits = 2) {
  return Number(parseFloat(v || 0).toFixed(digits));
}

// Channel-type max map for updating input.max
const CHMAX = [32, 16, 6, 8, 8, 8, 0, 0];

function loadTriggerConfig(triggers) {
  if (!Array.isArray(triggers)) return;
  triggers.forEach((t, n) => {
    if (n >= NUM_TRIGGERS) return;
    const setVal = (id, v) => { const el = document.getElementById(id); if (el) el.value = v; };
    const setChk = (id, v) => { const el = document.getElementById(id); if (el) el.checked = !!v; };

    setChk('t' + n + '-enabled', t.enabled);
    setVal('t' + n + '-chtype',  t.channelType   ?? 3);
    setVal('t' + n + '-chnum',   t.channelNumber  ?? 1);
    const sigSrc = t.signalSource ?? 0;
    triggerSignalSources[n] = sigSrc;
    setVal('t' + n + '-sigsrc',  sigSrc);
    setVal('t' + n + '-oscpath', t.customOSCPath  ?? '');
    const thresholdNorm = Number.isFinite(t.threshold) ? t.threshold : 0.5;
    const threshEl = document.getElementById('t' + n + '-thresh');
    const threshNumEl = document.getElementById('t' + n + '-thresh-n');
    if (threshEl) threshEl.dataset.norm = thresholdNorm;
    if (threshNumEl) threshNumEl.dataset.norm = thresholdNorm;
    setVal('t' + n + '-thresh',  roundInput(thresholdNorm)      ?? 0.5);
    setVal('t' + n + '-thresh-n',roundInput(thresholdNorm)      ?? 0.5);
    setVal('t' + n + '-hyst',    roundInput(t.hysteresis)     ?? 0.05);
    setVal('t' + n + '-hyst-n',  roundInput(t.hysteresis)     ?? 0.05);
    setVal('t' + n + '-smooth',  roundInput(t.smoothing)      ?? 0.15);
    setVal('t' + n + '-smooth-n',roundInput(t.smoothing)      ?? 0.15);
    setVal('t' + n + '-hold',    t.holdTimeMs     ?? 500);
    setVal('t' + n + '-rel',     t.releaseDelayMs ?? 1000);
    setVal('t' + n + '-dbnc',    t.debounceMs     ?? 50);
    setChk('t' + n + '-invert',  t.invert);

    // LED color
    setChk('t' + n + '-lcc', t.useTriggerColor);
    setVal('t' + n + '-lr',  t.trigLedR ?? 255);
    setVal('t' + n + '-lg',  t.trigLedG ?? 120);
    setVal('t' + n + '-lb',  t.trigLedB ?? 0);
    const lcp = document.getElementById('t' + n + '-lcp');
    if (lcp) lcp.value = rgbToHex(t.trigLedR ?? 255, t.trigLedG ?? 120, t.trigLedB ?? 0);
    updateTrigLedColorVisibility(n);

    // Resolved path preview
    const prev = document.getElementById('t' + n + '-pathpreview');
    if (prev) prev.textContent = t.resolvedPath || '—';

    // Apply channel-type constraints
    onTrigChTypeChange(n);
    onTrigSigSrcChange(n);
    updateTrigBodyVisibility(n);

    // Action lists
    trigState[n].on  = safeParseJSON(t.onJson);
    trigState[n].off = safeParseJSON(t.offJson);
    renderActionList('t' + n + 'On');
    renderActionList('t' + n + 'Off');
  });
}

// ── Save: collect all trigger configs ────────────────────────────────────────
function collectTriggers() {
  return trigState.map((ts, n) => {
    const getVal = id => { const el = document.getElementById(id); return el ? el.value : ''; };
    const getNum = id => { const el = document.getElementById(id); return el ? parseFloat(el.value) : 0; };
    const getInt = id => { const el = document.getElementById(id); return el ? parseInt(el.value)   : 0; };
    const getChk = id => { const el = document.getElementById(id); return el ? el.checked : false;       };
    return {
      enabled:        getChk('t' + n + '-enabled'),
      channelType:    getInt('t' + n + '-chtype'),
      channelNumber:  getInt('t' + n + '-chnum'),
      signalSource:   getInt('t' + n + '-sigsrc'),
      customOSCPath:  getVal('t' + n + '-oscpath'),
      threshold:      getThresholdValue(n),
      hysteresis:     getNum('t' + n + '-hyst'),
      smoothing:      getNum('t' + n + '-smooth'),
      holdTimeMs:     getInt('t' + n + '-hold'),
      releaseDelayMs: getInt('t' + n + '-rel'),
      debounceMs:     getInt('t' + n + '-dbnc'),
      invert:           getChk('t' + n + '-invert'),
      useTriggerColor:  getChk('t' + n + '-lcc'),
      trigLedR:         getInt('t' + n + '-lr'),
      trigLedG:         getInt('t' + n + '-lg'),
      trigLedB:         getInt('t' + n + '-lb'),
      onJson:           JSON.stringify(ts.on),
      offJson:          JSON.stringify(ts.off),
    };
  });
}

// ── Form submit handlers ──────────────────────────────────────────────────────

// Trigger form — all 4 triggers + priority settings in one POST
document.getElementById('form-trigger').addEventListener('submit', async e => {
  e.preventDefault();
  const prioMode = parseInt(document.getElementById('trig-prio-mode').value || '0');
  const data = {
    triggers:           collectTriggers(),
    trigPriorityMode:   prioMode,
    trigPriorityOrder:  [...trigPriorityOrder],
  };
  await apiPost('/api/config', data);
  setTimeout(loadConfig, 800);
});

// Talkback form
document.getElementById('form-talkback').addEventListener('submit', async e => {
  e.preventDefault();
  const bfA = document.getElementById('tb-b-follows-a');
  const data = {
    tbEnabled:   document.getElementById('tb-enabled').checked,
    tbMonitor:   parseInt(document.getElementById('tb-monitor').value || '0'),
    tbBFollowsA: bfA ? bfA.checked : false,
    tbAOnJson:   JSON.stringify(tbState.aOn),
    tbAOffJson:  JSON.stringify(tbState.aOff),
    tbBOnJson:   JSON.stringify(tbState.bOn),
    tbBOffJson:  JSON.stringify(tbState.bOff),
  };
  await apiPost('/api/config', data);
  setTimeout(loadConfig, 800);
});

// Standard form bindings (no custom logic needed)
function bindForm(formId) {
  document.getElementById(formId).addEventListener('submit', async e => {
    e.preventDefault();
    await apiPost('/api/config', collectForm(formId));
    setTimeout(loadConfig, 800);
  });
}
['form-network', 'form-mixer', 'form-osc'].forEach(bindForm);

// Output form — saves output settings AND per-trigger colors in one request
document.getElementById('form-output').addEventListener('submit', async e => {
  e.preventDefault();
  const data = Object.assign(collectForm('form-output'), { triggers: collectTriggers() });
  await apiPost('/api/config', data);
  setTimeout(loadConfig, 800);
});

// ── OSC receiver visibility ───────────────────────────────────────────────────
function updateOscVisibility() {
  const on = document.getElementById('ext-osc-enabled').checked;
  document.getElementById('ext-osc-fields').classList.toggle('hidden', !on);
}
document.getElementById('ext-osc-enabled').addEventListener('change', updateOscVisibility);
updateOscVisibility();

// ── DHCP toggle ──────────────────────────────────────────────────────────────
const useDhcpCb    = document.getElementById('use-dhcp');
const staticFields = document.getElementById('static-ip-fields');
function updateDhcpVisibility() {
  staticFields.classList.toggle('hidden', useDhcpCb.checked);
}
useDhcpCb.addEventListener('change', updateDhcpVisibility);
updateDhcpVisibility();

// ── Range/number sync (named inputs — output form) ────────────────────────────
function syncNum(rangeEl, numId) {
  document.getElementById(numId).value = rangeEl.value;
}
function syncRange(numEl, rangeName) {
  const r = document.querySelector(`[name="${rangeName}"]`);
  if (r) r.value = numEl.value;
}
// Range/number sync by element ID (trigger sliders)
function syncNumById(rangeEl, numId) {
  const numEl = document.getElementById(numId);
  const triggerId = rangeEl.id.match(/^t(\d+)-thresh$/);
  if (triggerId) {
    const n = triggerId[1];
    const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
    updateThresholdNorm(n, rangeEl.value, true);

    if (sigSrc === 0) {
      // Fader: logarithmic
      const db = normalizedToDb(rangeEl.value, false);
      if (numEl) numEl.value = db.toFixed(1);
    } else {
      // Meter: linear slider
      const db = (parseFloat(rangeEl.value) * 60) - 60;
      if (numEl) numEl.value = db.toFixed(1);
    }
    return;
  }
  if (numEl) numEl.value = rangeEl.value;
}

function syncRangeById(numId, rangeId) { // Hinweis: Habe hier Parameter für bessere Logik getauscht
  const r = document.getElementById(rangeId);
  const numEl = document.getElementById(numId);
  if (!r || !numEl) return;
  
  const triggerId = rangeId.match(/^t(\d+)-thresh$/);
  if (triggerId) {
    const n = triggerId[1];
    const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;

    if (sigSrc === 0) {
      // Fader: logarithmic
      const norm = dbToNormalized(numEl.value, false);
      r.value = norm.toFixed(2);
      updateThresholdNorm(n, numEl.value, false);
    } else {
      // Meter: linear slider
      const norm = (parseFloat(numEl.value) + 60) / 60;
      r.value = norm.toFixed(2);
      updateThresholdNorm(n, numEl.value, false);
    }
    return;
  }
  r.value = numEl.value;
}

// ── Color picker ──────────────────────────────────────────────────────────────
function applyColorPicker(picker) {
  const hex = picker.value;
  const r = parseInt(hex.slice(1,3), 16);
  const g = parseInt(hex.slice(3,5), 16);
  const b = parseInt(hex.slice(5,7), 16);
  document.querySelector('[name="ledR"]').value = r;
  document.querySelector('[name="ledG"]').value = g;
  document.querySelector('[name="ledB"]').value = b;
}
function rgbToHex(r,g,b) {
  return '#' + [r,g,b].map(v => v.toString(16).padStart(2,'0')).join('');
}

// ── Toast ─────────────────────────────────────────────────────────────────────
let toastTimer = null;
function showToast(msg, type = '') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className   = 'toast show ' + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.className = 'toast'; }, 3000);
}

// ── API helpers ───────────────────────────────────────────────────────────────
async function apiPost(url, body = null) {
  try {
    const opts = { method: 'POST' };
    if (body) {
      opts.headers = { 'Content-Type': 'application/json' };
      opts.body    = JSON.stringify(body);
    }
    const res  = await fetch(url, opts);
    const json = await res.json().catch(() => ({}));
    if (res.ok) { showToast(json.message || 'Done.', 'success'); }
    else        { showToast('Error: ' + (json.message || res.status), 'error'); }
    return json;
  } catch(e) {
    showToast('Request failed: ' + e.message, 'error');
  }
}

function confirmAction(url, question) {
  if (confirm(question)) apiPost(url);
}

// ── Collect named form fields → object ────────────────────────────────────────
function collectForm(formId) {
  const form = document.getElementById(formId);
  const data = {};
  form.querySelectorAll('[name]').forEach(el => {
    const key = el.name;
    if (el.type === 'checkbox')                          data[key] = el.checked;
    else if (el.type === 'number' || el.tagName === 'SELECT') data[key] = el.type === 'number' ? parseFloat(el.value) : parseInt(el.value, 10);
    else                                                 data[key] = el.value;
  });
  return data;
}

// ── Load full config from device ──────────────────────────────────────────────
async function loadConfig() {
  try {
    const res = await fetch('/api/config');
    if (!res.ok) return;
    const c = await res.json();

    // Standard named inputs (network, mixer, output, osc)
    document.querySelectorAll('[name]').forEach(el => {
      const key = el.name;
      if (!(key in c)) return;
      if      (el.type === 'checkbox') el.checked = !!c[key];
      else if (el.type === 'range' || el.type === 'number' || el.tagName === 'SELECT') el.value = c[key];
      else el.value = c[key] ?? '';
    });

    // Output form slider sync
    const lbr = document.querySelector('[name="ledBrightness"]');
    if (lbr) document.getElementById('led-bri-num').value = lbr.value;

    // Color picker
    const r = c.ledR ?? 255, g = c.ledG ?? 120, b = c.ledB ?? 0;
    const cp = document.getElementById('color-picker');
    if (cp) cp.value = rgbToHex(r, g, b);

    updateDhcpVisibility();
    updateOscVisibility();
    loadTBConfig(c);
    loadTriggerConfig(c.triggers);

    // Multi-trigger priority
    const prioModeEl = document.getElementById('trig-prio-mode');
    if (prioModeEl) prioModeEl.value = c.trigPriorityMode ?? 0;
    if (Array.isArray(c.trigPriorityOrder) && c.trigPriorityOrder.length === NUM_TRIGGERS) {
      trigPriorityOrder = [...c.trigPriorityOrder];
    }
    renderPrioList();
    onPrioModeChange();
  } catch(e) {
    console.warn('Config load failed:', e);
  }
}

// ── WebSocket ─────────────────────────────────────────────────────────────────
let ws = null;
let wsRetryTimer = null;
let wsRetryDelay = 1000;

function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(proto + '//' + location.host + '/ws');
  ws.onopen  = () => { console.log('[WS] Connected'); wsRetryDelay = 1000; clearTimeout(wsRetryTimer); };
  ws.onmessage = e => {
    try {
      const d = JSON.parse(e.data);
      if (d.type === 'osc') updateOSCMonitor(d);
      else                  updateStatus(d);
    } catch(_) {}
  };
  ws.onclose = ws.onerror = () => {
    wsRetryTimer = setTimeout(() => { wsRetryDelay = Math.min(wsRetryDelay * 1.5, 15000); connectWS(); }, wsRetryDelay);
  };
}

// ── Status update ─────────────────────────────────────────────────────────────
function updateStatus(d) {
  const setText = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };

  // Mixer
  const mixOk = !!d.mixerConnected;
  setText('st-mixer-con', mixOk ? 'Connected' : 'Disconnected');
  const mixEl = document.getElementById('st-mixer-con');
  if (mixEl) mixEl.style.color = mixOk ? 'var(--success)' : 'var(--danger)';

  // Per-trigger status cards (Trigger 1–4)
  if (Array.isArray(d.triggers)) {
    d.triggers.forEach((t, n) => {
      const lvRaw = parseFloat(t.level    ?? 0);
      const svRaw = parseFloat(t.smoothed ?? 0);
      const src = triggerSignalSources[n] ?? 0;
      const lv = formatSignalValue(lvRaw, src);
      const sv = formatSignalValue(svRaw, src);
      setText('st-trig-' + n + '-level',  lv);
      setText('st-trig-' + n + '-smooth', sv);
      const bar = document.getElementById('st-trig-' + n + '-bar');
      if (bar) bar.style.width = (Math.max(0, Math.min(1, lvRaw)) * 100) + '%';
      const statusEl = document.getElementById('st-trig-' + n + '-status');
      if (statusEl) {
        if (!t.enabled) {
          statusEl.textContent = 'Disabled';
          statusEl.className   = 'card-value trig-status disabled';
        } else if (t.triggered) {
          statusEl.textContent = 'ACTIVE';
          statusEl.className   = 'card-value trig-status active';
        } else {
          statusEl.textContent = 'Idle';
          statusEl.className   = 'card-value trig-status';
        }
      }
    });
  }

  // Per-trigger status chips
  const chipRow = document.getElementById('trig-status-row');
  if (chipRow && Array.isArray(d.triggers)) {
    // Build or refresh chips
    d.triggers.forEach((t, n) => {
      let chip = document.getElementById('trig-chip-' + n);
      if (!chip) {
        chip = document.createElement('div');
        chip.id = 'trig-chip-' + n;
        chip.className = 'trig-status-chip';
        chipRow.appendChild(chip);
      }
      chip.textContent = 'T' + (n + 1) + ': ' + (t.triggered ? 'ACTIVE' : 'Idle');
      chip.classList.toggle('active',   t.enabled && t.triggered);
      chip.classList.toggle('disabled', !t.enabled);
    });
  }

  // Header badge — triggered if any trigger is active
  const anyTriggered = !!d.triggered;
  setBadge('hdr-mixer',   mixOk,       'ok',     'Mixer');
  setBadge('hdr-trigger', anyTriggered,'active',  anyTriggered ? 'TRIGGERED' : 'Idle');
  setBadge('hdr-wifi',    !!d.staConnected, 'ok', d.staConnected ? (d.staIP || 'WiFi') : 'WiFi');

  // WiFi
  const wifiOk = !!d.staConnected;
  setText('st-wifi', wifiOk ? (d.staIP || 'Connected') : 'Not connected');
  setText('st-rssi',       (d.rssi ?? '—') + ' dBm');
  setText('st-ap-clients', d.apClients ?? '—');

  // Uptime
  const up = parseInt(d.uptime ?? 0);
  setText('st-uptime', `${Math.floor(up/3600)}h ${Math.floor((up%3600)/60)}m ${up%60}s`);

  // Misc
  setText('st-heap',   d.freeHeap ? (Math.round(d.freeHeap/1024) + ' KB') : '—');
  setText('st-fw',     d.firmware  ?? '—');
  setText('st-ap-ip',  d.apIP      ?? '—');
  setText('st-sta-ip', wifiOk ? (d.staIP ?? '—') : '—');

  // STA IP link in status tab
  const stStaLink = document.getElementById('st-sta-link');
  if (stStaLink) {
    if (wifiOk && d.staIP) { stStaLink.href = 'http://' + d.staIP; stStaLink.style.display = ''; }
    else                    stStaLink.style.display = 'none';
  }

  // Tools tab
  const toolApIp = document.getElementById('tool-ap-ip');
  if (toolApIp) toolApIp.textContent = d.apIP ?? '—';
  const toolStaIp = document.getElementById('tool-sta-ip');
  if (toolStaIp) toolStaIp.textContent = wifiOk ? (d.staIP ?? '—') : '—';
  const toolApLink = document.getElementById('tool-ap-link');
  if (toolApLink && d.apIP) { toolApLink.href = 'http://' + d.apIP; toolApLink.textContent = 'http://' + d.apIP; }
  const toolStaLink  = document.getElementById('tool-sta-link');
  const toolStaNoLnk = document.getElementById('tool-sta-nolink');
  if (toolStaLink && toolStaNoLnk) {
    if (wifiOk && d.staIP) {
      toolStaLink.href = 'http://' + d.staIP; toolStaLink.textContent = 'http://' + d.staIP;
      toolStaLink.style.display = ''; toolStaNoLnk.style.display = 'none';
    } else {
      toolStaLink.style.display = 'none'; toolStaNoLnk.style.display = '';
    }
  }

  // OSC tab info
  const oscStaIp = document.getElementById('osc-sta-ip');
  if (oscStaIp) oscStaIp.textContent = wifiOk ? (d.staIP ?? '—') : 'Not connected';
  const oscPort = document.getElementById('osc-listen-port');
  if (oscPort) oscPort.textContent = d.extOscEnabled ? (d.extOscPort ?? '—') : 'Disabled';

  // Talkback indicators
  const tbEnabled = !!d.tbEnabled;
  const tbA = !!d.tbA, tbB = !!d.tbB;
  setText('st-tb-a', tbEnabled ? (tbA ? 'ACTIVE' : 'Idle') : 'Disabled');
  setText('st-tb-b', tbEnabled ? (tbB ? 'ACTIVE' : 'Idle') : 'Disabled');
  const stTbAEl = document.getElementById('st-tb-a');
  const stTbBEl = document.getElementById('st-tb-b');
  if (stTbAEl) stTbAEl.style.color = (tbEnabled && tbA) ? 'var(--warn)' : 'var(--text-dim)';
  if (stTbBEl) stTbBEl.style.color = (tbEnabled && tbB) ? 'var(--warn)' : 'var(--text-dim)';
  const tbIndA = document.getElementById('tb-ind-a');
  const tbIndB = document.getElementById('tb-ind-b');
  if (tbIndA) tbIndA.classList.toggle('active', tbEnabled && tbA);
  if (tbIndB) tbIndB.classList.toggle('active', tbEnabled && tbB);
}

function setBadge(id, condition, okClass, label) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = label;
  el.className   = 'badge ' + (condition ? okClass : '');
}

// ── HTTP fallback ─────────────────────────────────────────────────────────────
async function pollStatus() {
  if (ws && ws.readyState === WebSocket.OPEN) return;
  try { const r = await fetch('/api/status'); if (r.ok) updateStatus(await r.json()); } catch(_) {}
}

// ── OSC Monitor ───────────────────────────────────────────────────────────────
let monitorRunning = false;
const MON_LOG_MAX = 40;

// Per-trigger stats state
const MON_STATE = Array.from({ length: NUM_TRIGGERS }, () =>
  ({ min: null, max: null, sum: 0, count: 0 })
);

async function toggleMonitor() {
  const res  = await fetch('/api/monitor', { method: 'POST' });
  if (!res.ok) return;
  const json = await res.json();
  monitorRunning = json.active;
  const btn = document.getElementById('mon-toggle');
  if (monitorRunning) {
    btn.textContent = '⏹ Stop Monitor'; btn.className = 'btn-warning';
    for (let n = 0; n < NUM_TRIGGERS; n++) {
      const log    = document.getElementById('mon-log-' + n);
      const canvas = document.getElementById('mon-canvas-' + n);
      if (log)    log.classList.add('active');
      if (canvas) canvas.classList.add('active');
    }
  } else {
    btn.textContent = '▶ Start Monitor'; btn.className = 'btn-primary';
  }
}

function clearMonitor(n) {
  const log = document.getElementById('mon-log-' + n);
  if (log) log.innerHTML = '';
  const st = MON_STATE[n];
  st.min = null; st.max = null; st.sum = 0; st.count = 0;
  const setText = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };
  setText('mon-min-' + n, '—'); setText('mon-max-' + n, '—'); setText('mon-avg-' + n, '—');
}
function clearAllMonitors() {
  for (let n = 0; n < NUM_TRIGGERS; n++) clearMonitor(n);
}

function updateOSCMonitor(d) {
  // Support both new {monitors:[...]} format and legacy single-trigger format
  const monitors = Array.isArray(d.monitors)
    ? d.monitors
    : [{ n: 0, address: d.address, value: d.value, smoothed: d.smoothed, triggered: d.triggered }];

  monitors.forEach(m => {
    const n = m.n;
    if (n >= NUM_TRIGGERS) return;
    const val = parseFloat(m.value ?? 0);
    const st  = MON_STATE[n];

    const setText = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };
    const src = triggerSignalSources[n] ?? 0;
    setText('mon-value-' + n, formatSignalValue(val, src));
    setText('mon-path-'  + n, m.address || '—');
    const bar = document.getElementById('mon-bar-' + n);
    if (bar) bar.style.width = (Math.max(0, Math.min(1, val)) * 100) + '%';

    if (st.min === null || val < st.min) st.min = val;
    if (st.max === null || val > st.max) st.max = val;
    st.sum += val; st.count++;
    setText('mon-min-' + n, st.min.toFixed(4));
    setText('mon-max-' + n, st.max.toFixed(4));
    setText('mon-avg-' + n, (st.sum / st.count).toFixed(4));

    // History ring-buffer for graph view
    MON_HIST[n].push(val);
    if (MON_HIST[n].length > MON_HIST_SIZE) MON_HIST[n].shift();
    if (monViewMode[n] === 'graph') drawMonitorGraph(n);

    const log = document.getElementById('mon-log-' + n);
    if (!log || !log.classList.contains('active')) return;
    const ts  = new Date().toLocaleTimeString('de-DE', { hour12:false, hour:'2-digit', minute:'2-digit', second:'2-digit' });
    const pct = Math.round(val * 100);
    const displayValue = formatSignalValue(val, src);
    const row = document.createElement('div');
    row.className = 'log-row';
    row.innerHTML = `<span class="log-ts">${ts}</span><span class="log-val">${displayValue}</span>` +
      `<span class="log-bar"><span class="log-fill" style="width:${pct}%"></span></span>` +
      (m.triggered ? '<span class="log-trig">▲</span>' : '');
    log.insertBefore(row, log.firstChild);
    while (log.children.length > MON_LOG_MAX) log.removeChild(log.lastChild);
  });
}

// ── Init ──────────────────────────────────────────────────────────────────────
loadConfig();
connectWS();
setInterval(pollStatus, 3000);
