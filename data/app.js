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
  if (cb) cb.addEventListener('change', () => { updateTrigBodyVisibility(n); enforceM6(n); });
  const tap = document.getElementById('t' + n + '-metertap');
  if (tap) tap.addEventListener('change', () => onTrigMeterTapChange(n));
  const num = document.getElementById('t' + n + '-chnum');
  if (num) num.addEventListener('change', () => onTrigChNumChange(n));
  updateTrigBodyVisibility(n);
}

// ── Channel type → update max channel number + disable Meter for DCA ─────────
const CH_MAX = { 0:32, 1:16, 2:6, 3:8, 4:8, 5:8, 6:0, 7:0 };

// Meter tap (meterSignalType) labels. 0=pre, 1=gate-GR, 2=comp-GR, 3=post.
const METER_TAPS = {
  0: 'Pre-Fader',
  1: 'Gate Gain-Reduction',
  2: 'Comp Gain-Reduction',
  3: 'Post-Fader',
};

// Which meter taps actually return data per channel type (X32 metering reality,
// confirmed live). Pre/Comp/Gate are multi-channel from the full banks. Post (and
// Main/Mono Pre) come from the single-channel /meters/6 strip — only ONE channel
// can use it at a time (see enforceM6 + METERING.md). Gate-GR exists for inputs
// only; Main and Mono have a Post bulk meter (their Pre uses /meters/6). DCA: none.
//   tap values: 0=pre, 1=gate, 2=comp, 3=post
const METER_TAPS_BY_CHTYPE = {
  0: [0, 3, 2, 1],   // CH_INPUT : pre, post, comp, gate
  1: [0, 3, 2],      // CH_BUS   : pre, post, comp
  2: [0, 3, 2],      // CH_MATRIX
  3: [],             // CH_DCA   : none
  4: [0, 3],         // CH_AUXIN : pre, post
  5: [0, 3],         // CH_FXRTN : pre, post
  6: [3, 0, 2],      // CH_MAIN  : post, pre, comp
  7: [3, 0, 2],      // CH_MONO  : post, pre, comp
};

// Rebuild the meter-tap <select> so it only offers taps that work for the
// current channel type. Keeps the current selection if still valid.
function rebuildMeterTapOptions(n) {
  const chType = parseInt(document.getElementById('t' + n + '-chtype').value) || 0;
  const sel = document.getElementById('t' + n + '-metertap');
  if (!sel) return;
  const allowed = METER_TAPS_BY_CHTYPE[chType] || [];
  const prev = parseInt(sel.value);
  sel.innerHTML = '';
  for (const v of allowed) {
    const opt = document.createElement('option');
    opt.value = v;
    opt.textContent = METER_TAPS[v];
    sel.appendChild(opt);
  }
  if (allowed.includes(prev))      sel.value = prev;
  else if (allowed.length)         sel.value = allowed[0];
}

// ── /meters/6 single-channel guard ────────────────────────────────────────────
// Post-fader (and Main pre) read from /meters/6, which has ONE console-wide
// channel selection. Only one channel can use it; we prevent the user from
// configuring a second /meters/6 trigger on a different channel.
const CH_TYPE_NAME = { 0:'Input', 1:'Bus', 2:'Matrix', 3:'DCA',
                       4:'AuxIn', 5:'FxRtn', 6:'Main', 7:'Mono' };

// Does (channel type, tap) resolve to the /meters/6 strip? Mirrors meterRoute().
function meterUsesM6(chType, tap) {
  if (chType === 3) return false;                  // DCA: no meter at all
  if (chType === 6 || chType === 7) return tap === 0;  // Main/Mono: Pre uses /m6 (Post is bulk)
  return tap === 3;                                 // all others: Post uses /meters/6
}

// /meters/6 channel_id for a channel type + 1-based number. Mirrors meterChannelId().
function m6ChannelOf(chType, chNum) {
  const i = (chNum > 0 ? chNum - 1 : 0);
  switch (chType) {
    case 0: return i;        // Input  0-31
    case 4: return 32 + i;   // AuxIn  32-39
    case 5: return 40 + i;   // FxRtn  40-47
    case 1: return 48 + i;   // Bus    48-63
    case 2: return 64 + i;   // Matrix 64-69
    case 6: return 70;       // Main
    case 7: return 71;       // Mono
    default: return -1;
  }
}

// /meters/6 channel claimed by trigger n right now, or null if it doesn't use it.
function triggerM6Channel(n) {
  const en = document.getElementById('t' + n + '-enabled');
  if (!en || !en.checked) return null;
  if ((parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0) !== 1) return null;
  const ct  = parseInt(document.getElementById('t' + n + '-chtype').value) || 0;
  const tap = parseInt(document.getElementById('t' + n + '-metertap').value);
  if (Number.isNaN(tap) || !meterUsesM6(ct, tap)) return null;
  const cn  = parseInt(document.getElementById('t' + n + '-chnum').value) || 1;
  return m6ChannelOf(ct, cn);
}

function triggerChanLabel(n) {
  const ct = parseInt(document.getElementById('t' + n + '-chtype').value) || 0;
  const cn = parseInt(document.getElementById('t' + n + '-chnum').value) || 1;
  return CH_TYPE_NAME[ct] + ((ct === 6 || ct === 7) ? '' : ' ' + cn);
}

// Returns the index of an enabled meter trigger (other than excludeN) that already
// holds a /meters/6 channel different from `ch`, or -1 if none.
function otherM6ChannelHolder(excludeN, ch) {
  for (let m = 0; m < NUM_TRIGGERS; m++) {
    if (m === excludeN) continue;
    const oc = triggerM6Channel(m);
    if (oc !== null && oc !== ch) return m;
  }
  return -1;
}

let suppressM6Guard = false;   // true while applyConfig() is populating the form

// If trigger n now uses /meters/6 on a channel that clashes with another trigger,
// switch n's tap to the first non-/meters/6 tap. Returns true if it intervened.
// `explicit` = the user directly chose the meter tap → show the explaining popup.
// Otherwise (a channel-type/number change that incidentally lands on /meters/6)
// the tap is switched silently, with no popup.
function enforceM6(n, explicit = false) {
  if (suppressM6Guard) return false;
  const myCh = triggerM6Channel(n);
  if (myCh === null) return false;
  const holder = otherM6ChannelHolder(n, myCh);
  if (holder < 0) return false;

  const ct  = parseInt(document.getElementById('t' + n + '-chtype').value) || 0;
  const sel = document.getElementById('t' + n + '-metertap');
  const wantedTap = parseInt(sel.value);
  const safe = (METER_TAPS_BY_CHTYPE[ct] || []).find(tap => !meterUsesM6(ct, tap));
  if (safe !== undefined && sel) sel.value = safe;

  if (explicit) {
    showInfoModal('Only one Post-Fader channel possible',
      `This device can meter only <b>one</b> channel's Post-Fader (channel strip) ` +
      `at a time (due to a limitation) - it is read from <code>/meters/6</code>, which has a single ` +
      `console-wide selection.<br><br>` +
      `<b>Trigger ${holder + 1}</b> (${triggerChanLabel(holder)}) already uses it, so ` +
      `<b>Trigger ${n + 1}</b> (${triggerChanLabel(n)}) cannot use ` +
      `<b>${METER_TAPS[wantedTap] || 'that tap'}</b> on a different channel.<br><br>` +
      `Trigger ${n + 1} was set to <b>${safe !== undefined ? METER_TAPS[safe] : '—'}</b>. ` +
      `To meter this channel post-fader, free up Trigger ${holder + 1} first, or put ` +
      `both triggers on the <i>same</i> channel. Pre / Comp / Gate work for many ` +
      `channels at once.`);
  }
  return true;
}

// Validate the whole trigger set before saving. Returns true if OK (no clash).
function validateM6BeforeSave() {
  const chans = new Set();
  let firstN = -1;
  for (let n = 0; n < NUM_TRIGGERS; n++) {
    const ch = triggerM6Channel(n);
    if (ch === null) continue;
    if (chans.size && !chans.has(ch)) {
      showInfoModal('Cannot save — two Post-Fader channels',
        `Only <b>one</b> channel can use Post-Fader / <code>/meters/6</code> at a time. ` +
        `<b>Trigger ${firstN + 1}</b> and <b>Trigger ${n + 1}</b> are configured for ` +
        `different channels. Change one to Pre / Comp / Gate, or use the same channel, ` +
        `then save again.`);
      return false;
    }
    if (!chans.size) firstN = n;
    chans.add(ch);
  }
  return true;
}

// ── Simple modal popup (themed, self-contained) ───────────────────────────────
function showInfoModal(title, bodyHtml) {
  let ov = document.getElementById('info-modal');
  if (!ov) {
    ov = document.createElement('div');
    ov.id = 'info-modal';
    ov.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,.65);' +
      'display:none;align-items:center;justify-content:center;z-index:1000;';
    ov.innerHTML =
      '<div style="background:var(--bg2);color:var(--text);border:1px solid var(--border);' +
      'border-radius:var(--radius);max-width:520px;margin:1rem;padding:1.25rem 1.5rem;' +
      'box-shadow:0 10px 40px rgba(0,0,0,.6);">' +
      '<h3 id="info-modal-title" style="margin:0 0 .75rem;color:var(--warn);"></h3>' +
      '<div id="info-modal-body" style="font-size:.92rem;line-height:1.5;"></div>' +
      '<div style="text-align:right;margin-top:1.25rem;">' +
      '<button type="button" class="btn-primary" id="info-modal-ok">Got it</button></div></div>';
    document.body.appendChild(ov);
    const close = () => { ov.style.display = 'none'; };
    ov.addEventListener('click', e => { if (e.target === ov) close(); });
    document.getElementById('info-modal-ok').addEventListener('click', close);
  }
  document.getElementById('info-modal-title').textContent = title;
  document.getElementById('info-modal-body').innerHTML = bodyHtml;
  ov.style.display = 'flex';
}

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

  rebuildMeterTapOptions(n);   // offer only taps that work for this channel type
  onTrigSigSrcChange(n);
}

// Values now arrive from the firmware already in dB (fader + meter); mute is 0/1.
function formatSignalValue(value, signalSource) {
  const val = parseFloat(value) || 0;
  if (signalSource === 2) return val >= 0.5 ? 'Active' : 'Muted';
  return val.toFixed(1) + ' dB';
}

// Map a value (dB, or 0/1 for mute) to a 0..1 bar/graph fraction. GR taps
// (gate/comp, 0..~40 dB) fill upward; levels use a -60..0 dB window.
function levelFraction(n, value) {
  const v = parseFloat(value) || 0;
  const sig = triggerSignalSources[n] ?? 0;
  if (sig === 2) return v >= 0.5 ? 1 : 0;                 // mute
  const tapEl = document.getElementById('t' + n + '-metertap');
  const tap = tapEl ? parseInt(tapEl.value) : -1;
  if (sig === 1 && (tap === 1 || tap === 2))              // gate/comp gain reduction
    return Math.max(0, Math.min(1, v / 40));
  return Math.max(0, Math.min(1, (v + 60) / 60));         // level / fader: -60..0 dB
}



// Threshold is now a plain dB value (slider and number both hold dB).
function getThresholdValue(n) {
  const num = document.getElementById('t' + n + '-thresh-n');
  const v = num ? parseFloat(num.value) : NaN;
  return Number.isFinite(v) ? v : 0;
}

// dB range offered for the threshold slider/number, depending on signal + tap.
function thresholdRange(n) {
  const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  if (sigSrc === 1) {   // meter
    const tap = parseInt(document.getElementById('t' + n + '-metertap').value);
    if (tap === 1 || tap === 2)
      return { lo: 0, hi: 40, label: 'Threshold — gain reduction (dB)' };  // GR
    return { lo: -60, hi: 0, label: 'Threshold (dB)' };                    // level curve
  }
  return { lo: -90, hi: 10, label: 'Threshold (dB)' };                     // fader
}

function updateThresholdDisplay(n) {
  const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  const range  = document.getElementById('t' + n + '-thresh');
  const num    = document.getElementById('t' + n + '-thresh-n');
  if (!range || !num) return;

  const label = range.closest('label');
  const labelText = label ? label.querySelector('.threshold-label-text') : null;

  // Mute has no threshold/hysteresis/smoothing — hide those rows.
  const thresholdLabel  = label;
  const hysteresisLabel = document.getElementById('t' + n + '-hyst')?.closest('label');
  const smoothingLabel  = document.getElementById('t' + n + '-smooth')?.closest('label');
  const showThreshold = sigSrc !== 2;
  [thresholdLabel, hysteresisLabel, smoothingLabel].forEach(el => {
    if (el) el.style.display = showThreshold ? '' : 'none';
  });
  if (sigSrc === 2) {
    if (labelText) labelText.textContent = 'Threshold (disabled for Mute)';
    return;
  }

  const r = thresholdRange(n);
  range.min = r.lo; range.max = r.hi; range.step = 0.5;
  num.min   = r.lo; num.max   = r.hi; num.step   = 0.1;

  // Keep the current dB value, clamped into the new range.
  let v = parseFloat(num.value);
  if (!Number.isFinite(v)) v = r.lo;
  v = Math.max(r.lo, Math.min(r.hi, v));
  range.value = v;
  num.value   = v;
  if (labelText) labelText.textContent = r.label;
}
function onTrigSigSrcChange(n) {
  triggerSignalSources[n] = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  updateThresholdDisplay(n);
  updateMeterTapVisibility(n);
  enforceM6(n);   // block a second /meters/6 channel
}

// Called when the meter tap or channel number changes (wired up below).
function onTrigMeterTapChange(n) {
  updateMeterTapVisibility(n);
  updateThresholdDisplay(n);   // level vs gain-reduction → different dB range
  enforceM6(n, true);   // explicit tap choice → explain via popup if it clashes
}
function onTrigChNumChange(n) {
  enforceM6(n);         // incidental clash → switch tap silently
}

// Show the Pre/Post-Fader/... meter tap selector only for Meter signal source.
// DCA channels cannot use meters (the Meter option is disabled for them), so the
// selector stays hidden in that case as well.
function updateMeterTapVisibility(n) {
  const sigSrc = parseInt(document.getElementById('t' + n + '-sigsrc').value) || 0;
  const chType = parseInt(document.getElementById('t' + n + '-chtype').value) || 0;
  const lbl = document.getElementById('t' + n + '-metertap-lbl');
  const hasTaps = (METER_TAPS_BY_CHTYPE[chType] || []).length > 0;
  if (lbl) lbl.style.display = (sigSrc === 1 && hasTaps) ? '' : 'none';
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
  suppressM6Guard = true;   // don't fire the guard while populating fields
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
    setVal('t' + n + '-metertap', t.meterSignalType);
    setVal('t' + n + '-oscpath', t.customOSCPath  ?? '');
    // Threshold + hysteresis are dB now; slider and number both hold the dB value.
    // updateThresholdDisplay() (via onTrigSigSrcChange below) then sets the range.
    const thrDb = Number.isFinite(t.threshold) ? t.threshold : -20;
    setVal('t' + n + '-thresh',   roundInput(thrDb) ?? -20);
    setVal('t' + n + '-thresh-n', roundInput(thrDb) ?? -20);
    setVal('t' + n + '-hyst',    roundInput(t.hysteresis)     ?? 3);
    setVal('t' + n + '-hyst-n',  roundInput(t.hysteresis)     ?? 3);
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
  suppressM6Guard = false;
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
      meterSignalType: getInt('t' + n + '-metertap'),
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
  if (!validateM6BeforeSave()) return;   // refuse impossible /meters/6 combos
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
// Range/number sync by element ID (trigger sliders). All sliders (threshold in
// dB, hysteresis in dB, smoothing) now hold the same value in both controls, so
// this is a plain copy in either direction.
function syncNumById(rangeEl, numId) {
  const numEl = document.getElementById(numId);
  if (numEl) numEl.value = rangeEl.value;
}

function syncRangeById(numEl, rangeId) {
  const r = document.getElementById(rangeId);
  if (r) r.value = numEl.value;
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

// ── Config import / export ────────────────────────────────────────────────────
function exportConfig() {
  // Content-Disposition on the endpoint makes the browser download the file.
  window.location.href = '/api/config/export';
}

async function importConfig(input) {
  const file = input.files && input.files[0];
  input.value = '';                       // reset so the same file can be re-picked
  if (!file) return;

  let cfg;
  try {
    cfg = JSON.parse(await file.text());
  } catch (e) {
    showToast('Not a valid JSON file', 'error');
    return;
  }
  if (cfg._type !== 'talkback-calllight-config' &&
      !confirm('This file is not a recognised TalkBack config. Import anyway?')) return;
  if (!confirm('Import this configuration? All settings except Wi-Fi will be overwritten.')) return;

  await apiPost('/api/config', cfg);      // WiFi keys are absent → left untouched
  setTimeout(loadConfig, 800);            // repopulate the form from the applied config
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
      if (bar) bar.style.width = (levelFraction(n, lvRaw) * 100) + '%';
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
    if (bar) bar.style.width = (levelFraction(n, val) * 100) + '%';

    if (st.min === null || val < st.min) st.min = val;
    if (st.max === null || val > st.max) st.max = val;
    st.sum += val; st.count++;
    setText('mon-min-' + n, formatSignalValue(st.min, src));
    setText('mon-max-' + n, formatSignalValue(st.max, src));
    setText('mon-avg-' + n, formatSignalValue(st.sum / st.count, src));

    // History ring-buffer for graph view (store the 0..1 plot fraction)
    MON_HIST[n].push(levelFraction(n, val));
    if (MON_HIST[n].length > MON_HIST_SIZE) MON_HIST[n].shift();
    if (monViewMode[n] === 'graph') drawMonitorGraph(n);

    const log = document.getElementById('mon-log-' + n);
    if (!log || !log.classList.contains('active')) return;
    const ts  = new Date().toLocaleTimeString('de-DE', { hour12:false, hour:'2-digit', minute:'2-digit', second:'2-digit' });
    const pct = Math.round(levelFraction(n, val) * 100);
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
