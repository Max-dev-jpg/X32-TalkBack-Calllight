// =============================================================================
// TalkBack CallLight – Frontend Application
// =============================================================================

'use strict';

// ── Tab switching ─────────────────────────────────────────────────────────────
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
  });
});

// ── Talkback section visibility ───────────────────────────────────────────────
const tbEnabledCb     = document.getElementById('tb-enabled');
const tbEngineFields  = document.getElementById('tb-engine-fields');
const tbAdvSection    = document.getElementById('tb-advanced-section');
const tbSoloEnabledCb = document.getElementById('tb-solo-enabled');
const tbSoloSection   = document.getElementById('tb-solo-section');

function updateTBVisibility() {
  const on = tbEnabledCb.checked;
  tbEngineFields.classList.toggle('hidden', !on);
  tbAdvSection.classList.toggle('hidden', !on);
}
function updateTBSoloVisibility() {
  tbSoloSection.classList.toggle('hidden', !tbSoloEnabledCb.checked);
}
tbEnabledCb.addEventListener('change', updateTBVisibility);
tbSoloEnabledCb.addEventListener('change', updateTBSoloVisibility);
updateTBVisibility();
updateTBSoloVisibility();

// ── DHCP toggle ──────────────────────────────────────────────────────────────
const useDhcpCb = document.getElementById('use-dhcp');
const staticFields = document.getElementById('static-ip-fields');
function updateDhcpVisibility() {
  staticFields.classList.toggle('hidden', useDhcpCb.checked);
}
useDhcpCb.addEventListener('change', updateDhcpVisibility);
updateDhcpVisibility();

// ── Range ↔ number sync ───────────────────────────────────────────────────────
function syncNum(rangeEl, numId) {
  document.getElementById(numId).value = rangeEl.value;
}
function syncRange(numEl, rangeName) {
  const r = document.querySelector(`[name="${rangeName}"]`);
  if (r) r.value = numEl.value;
}

// ── Color picker → R/G/B fields ───────────────────────────────────────────────
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

// ── Toast notification ────────────────────────────────────────────────────────
let toastTimer = null;
function showToast(msg, type = '') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'toast show ' + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.className = 'toast'; }, 3000);
}

// ── Generic API POST ──────────────────────────────────────────────────────────
async function apiPost(url, body = null) {
  try {
    const opts = { method: 'POST' };
    if (body) {
      opts.headers = { 'Content-Type': 'application/json' };
      opts.body = JSON.stringify(body);
    }
    const res = await fetch(url, opts);
    const json = await res.json().catch(() => ({}));
    if (res.ok) {
      showToast(json.message || 'Done.', 'success');
    } else {
      showToast('Error: ' + (json.message || res.status), 'error');
    }
    return json;
  } catch(e) {
    showToast('Request failed: ' + e.message, 'error');
  }
}

function confirmAction(url, question) {
  if (confirm(question)) apiPost(url);
}

// ── Load config from device → populate all forms ─────────────────────────────
async function loadConfig() {
  try {
    const res = await fetch('/api/config');
    if (!res.ok) return;
    const c = await res.json();

    // Populate every named input/select in all forms
    const allInputs = document.querySelectorAll('[name]');
    allInputs.forEach(el => {
      const key = el.name;
      if (!(key in c)) return;
      if (el.type === 'checkbox') {
        el.checked = !!c[key];
      } else if (el.type === 'range' || el.type === 'number' || el.tagName === 'SELECT') {
        el.value = c[key];
      } else {
        el.value = c[key] ?? '';
      }
    });

    // Sync range ↔ number display values
    syncNum(document.querySelector('[name="threshold"]'),   'threshold-num');
    syncNum(document.querySelector('[name="hysteresis"]'),  'hysteresis-num');
    syncNum(document.querySelector('[name="smoothing"]'),   'smoothing-num');
    syncNum(document.querySelector('[name="ledBrightness"]'), 'led-bri-num');

    // Sync color picker
    const r = c.ledR ?? 255, g = c.ledG ?? 120, b = c.ledB ?? 0;
    document.getElementById('color-picker').value = rgbToHex(r, g, b);

    // Update OSC path preview
    document.getElementById('osc-path-preview').textContent = c.oscPath || '—';

    updateDhcpVisibility();
    updateTBVisibility();
    updateTBSoloVisibility();
  } catch(e) {
    console.warn('Config load failed:', e);
  }
}

// ── Collect form data → object ────────────────────────────────────────────────
function collectForm(formId) {
  const form = document.getElementById(formId);
  const data = {};
  form.querySelectorAll('[name]').forEach(el => {
    const key = el.name;
    if (el.type === 'checkbox') {
      data[key] = el.checked;
    } else if (el.type === 'number' || el.tagName === 'SELECT') {
      data[key] = el.type === 'number' ? parseFloat(el.value) : parseInt(el.value, 10);
    } else {
      data[key] = el.value;
    }
  });
  return data;
}

// ── Form submit handlers ──────────────────────────────────────────────────────
function bindForm(formId) {
  document.getElementById(formId).addEventListener('submit', async e => {
    e.preventDefault();
    const data = collectForm(formId);
    await apiPost('/api/config', data);
    // Reload config to reflect any server-side changes (e.g. resolved OSC path)
    setTimeout(loadConfig, 800);
  });
}
['form-network', 'form-mixer', 'form-trigger', 'form-output', 'form-talkback'].forEach(bindForm);

// ── Live status updates via WebSocket ────────────────────────────────────────
let ws = null;
let wsRetryTimer = null;
let wsRetryDelay = 1000;

function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(proto + '//' + location.host + '/ws');

  ws.onopen = () => {
    console.log('[WS] Connected');
    wsRetryDelay = 1000;
    clearTimeout(wsRetryTimer);
  };

  ws.onmessage = e => {
    try {
      const d = JSON.parse(e.data);
      if (d.type === 'osc') {
        updateOSCMonitor(d);
      } else {
        updateStatus(d);
      }
    } catch(err) { /* ignore */ }
  };

  ws.onclose = ws.onerror = () => {
    console.log('[WS] Disconnected – retrying in', wsRetryDelay, 'ms');
    wsRetryTimer = setTimeout(() => {
      wsRetryDelay = Math.min(wsRetryDelay * 1.5, 15000);
      connectWS();
    }, wsRetryDelay);
  };
}

// ── Apply status JSON to DOM ──────────────────────────────────────────────────
function updateStatus(d) {
  // Helper: safe set text
  const setText = (id, val) => {
    const el = document.getElementById(id);
    if (el) el.textContent = val;
  };

  // Mixer connection
  const mixOk = !!d.mixerConnected;
  setText('st-mixer-con', mixOk ? 'Connected' : 'Disconnected');
  document.getElementById('st-mixer-con').style.color =
    mixOk ? 'var(--success)' : 'var(--danger)';

  // Level
  const lv = parseFloat(d.level  ?? 0).toFixed(3);
  const sv = parseFloat(d.smoothed ?? 0).toFixed(3);
  setText('st-level',  lv);
  setText('st-smooth', sv);
  const lvBar = document.getElementById('st-level-bar');
  const svBar = document.getElementById('st-smooth-bar');
  if (lvBar) lvBar.style.width = (parseFloat(lv) * 100) + '%';
  if (svBar) svBar.style.width = (parseFloat(sv) * 100) + '%';

  // Trigger
  const trig = !!d.triggered;
  const trigEl = document.getElementById('st-trig');
  if (trigEl) {
    trigEl.textContent = trig ? 'ACTIVE' : 'IDLE';
    trigEl.style.color = trig ? 'var(--warn)' : 'var(--text-dim)';
  }

  // WiFi
  const wifiOk = !!d.staConnected;
  setText('st-wifi', wifiOk ? (d.staIP || 'Connected') : 'Not connected');
  setText('st-rssi',  (d.rssi ?? '—') + ' dBm');
  setText('st-ap-clients', d.apClients ?? '—');

  // Uptime
  const up = parseInt(d.uptime ?? 0);
  const h = Math.floor(up / 3600), m = Math.floor((up % 3600) / 60), s = up % 60;
  setText('st-uptime', `${h}h ${m}m ${s}s`);

  // Misc
  setText('st-heap', d.freeHeap ? (Math.round(d.freeHeap / 1024) + ' KB') : '—');
  setText('st-fw', d.firmware ?? '—');
  setText('st-ap-ip', d.apIP ?? '—');
  setText('st-sta-ip', wifiOk ? (d.staIP ?? '—') : '—');

  // Header badges
  setBadge('hdr-mixer',   mixOk,   'ok',     'Mixer');
  setBadge('hdr-trigger', trig,    'active',  trig ? 'TRIGGERED' : 'Idle', !!trig);
  setBadge('hdr-wifi',    wifiOk,  'ok',     'WiFi');

  // Tools tab info
  const toolApIp = document.getElementById('tool-ap-ip');
  if (toolApIp) toolApIp.textContent = d.apIP ?? '—';
  const toolStaIp = document.getElementById('tool-sta-ip');
  if (toolStaIp) toolStaIp.textContent = wifiOk ? (d.staIP ?? '—') : '—';
  const toolApLink = document.getElementById('tool-ap-link');
  if (toolApLink && d.apIP) {
    toolApLink.href = 'http://' + d.apIP;
    toolApLink.textContent = 'http://' + d.apIP;
  }

  // Talkback state
  const tbEnabled = !!d.tbEnabled;
  const tbA = !!d.tbA;
  const tbB = !!d.tbB;

  // Status tab: small cards
  const stTbA = document.getElementById('st-tb-a');
  const stTbB = document.getElementById('st-tb-b');
  if (stTbA) {
    stTbA.textContent = tbEnabled ? (tbA ? 'ACTIVE' : 'Idle') : 'Disabled';
    stTbA.style.color = (tbEnabled && tbA) ? 'var(--warn)' : 'var(--text-dim)';
  }
  if (stTbB) {
    stTbB.textContent = tbEnabled ? (tbB ? 'ACTIVE' : 'Idle') : 'Disabled';
    stTbB.style.color = (tbEnabled && tbB) ? 'var(--warn)' : 'var(--text-dim)';
  }

  // Talkback tab: large circular indicators
  const tbIndA = document.getElementById('tb-ind-a');
  const tbIndB = document.getElementById('tb-ind-b');
  if (tbIndA) tbIndA.classList.toggle('active', tbEnabled && tbA);
  if (tbIndB) tbIndB.classList.toggle('active', tbEnabled && tbB);
}

function setBadge(id, condition, okClass, label, forceBadClass) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = label;
  el.className   = 'badge ' + (condition ? okClass : (forceBadClass === false ? '' : ''));
}

// ── Fallback HTTP polling (if WebSocket not available) ────────────────────────
async function pollStatus() {
  if (ws && ws.readyState === WebSocket.OPEN) return;
  try {
    const res = await fetch('/api/status');
    if (res.ok) updateStatus(await res.json());
  } catch(e) { /* ignore */ }
}

// ── OSC path preview (update on mixer form change) ────────────────────────────
document.getElementById('form-mixer').addEventListener('change', () => {
  // Debounced fetch of resolved path
  clearTimeout(document._oscPreviewTimer);
  document._oscPreviewTimer = setTimeout(async () => {
    try {
      const res = await fetch('/api/config');
      if (res.ok) {
        const c = await res.json();
        document.getElementById('osc-path-preview').textContent = c.oscPath || '—';
      }
    } catch(e) {}
  }, 400);
});

// ── OSC Monitor ───────────────────────────────────────────────────────────────
let monitorRunning = false;
let monitorMin = null, monitorMax = null, monitorSum = 0, monitorCount = 0;
const MON_LOG_MAX = 60;

async function toggleMonitor() {
  const res = await fetch('/api/monitor', { method: 'POST' });
  if (!res.ok) return;
  const json = await res.json();
  monitorRunning = json.active;

  const btn = document.getElementById('mon-toggle');
  const log = document.getElementById('mon-log');

  if (monitorRunning) {
    btn.textContent = '⏹ Stop Monitor';
    btn.className = 'btn-warning';
    log.classList.add('active');
    // Show path immediately
    fetch('/api/config').then(r => r.json()).then(c => {
      document.getElementById('mon-path').textContent = c.oscPath || '—';
    }).catch(() => {});
  } else {
    btn.textContent = '▶ Start Monitor';
    btn.className = 'btn-primary';
  }
}

function clearMonitor() {
  document.getElementById('mon-log').innerHTML = '';
  monitorMin = null; monitorMax = null;
  monitorSum = 0; monitorCount = 0;
  document.getElementById('mon-min').textContent = '—';
  document.getElementById('mon-max').textContent = '—';
  document.getElementById('mon-avg').textContent = '—';
}

function updateOSCMonitor(d) {
  const val = parseFloat(d.value ?? 0);

  // Large value display
  document.getElementById('mon-value').textContent = val.toFixed(4);
  document.getElementById('mon-path').textContent = d.address || '—';
  document.getElementById('mon-bar').style.width = (val * 100) + '%';

  // Running statistics
  if (monitorMin === null || val < monitorMin) monitorMin = val;
  if (monitorMax === null || val > monitorMax) monitorMax = val;
  monitorSum += val;
  monitorCount++;
  document.getElementById('mon-min').textContent = monitorMin.toFixed(4);
  document.getElementById('mon-max').textContent = monitorMax.toFixed(4);
  document.getElementById('mon-avg').textContent = (monitorSum / monitorCount).toFixed(4);

  // Scrolling log row
  const log = document.getElementById('mon-log');
  if (!log.classList.contains('active')) return;

  const ts = new Date().toLocaleTimeString('de-DE', { hour12: false,
    hour: '2-digit', minute: '2-digit', second: '2-digit' });
  const pct = Math.round(val * 100);
  const trig = d.triggered ? '<span class="log-trig">▲</span>' : '';

  const row = document.createElement('div');
  row.className = 'log-row';
  row.innerHTML =
    `<span class="log-ts">${ts}</span>` +
    `<span class="log-val">${val.toFixed(4)}</span>` +
    `<span class="log-bar"><span class="log-fill" style="width:${pct}%"></span></span>` +
    trig;

  // Prepend so newest is on top; cap at MON_LOG_MAX rows
  log.insertBefore(row, log.firstChild);
  while (log.children.length > MON_LOG_MAX) log.removeChild(log.lastChild);
}

// ── Init ──────────────────────────────────────────────────────────────────────
loadConfig();
connectWS();
setInterval(pollStatus, 3000);   // fallback poll every 3 s
