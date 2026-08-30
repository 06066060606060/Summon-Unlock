const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EU Summon Unlock</title>
<style>
  :root {
    --bg: #0d0d0d;
    --panel: #161618;
    --card: #1c1c1e;
    --line: #2c2c2e;
    --txt: #f5f5f7;
    --muted: #8e8e93;
    --accent: #0a84ff;
    --ok: #30d158;
    --warn: #ff9f0a;
    --bad: #ff453a;
  }
  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
  body {
    margin: 0;
    background: var(--bg);
    color: var(--txt);
    font: 15px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    -webkit-font-smoothing: antialiased;
  }
  header {
    padding: 18px 20px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  header h1 {
    margin: 0;
    font-size: 18px;
    font-weight: 700;
    color: var(--txt);
    display: flex;
    align-items: center;
    gap: 8px;
  }
  header h1::before {
    content: "⚡";
    font-size: 14px;
    opacity: 0.7;
  }
  header .pill {
    font-size: 12px;
    padding: 5px 12px;
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 20px;
    color: var(--muted);
    display: flex;
    align-items: center;
    gap: 6px;
    font-weight: 500;
  }
  header .pill::before {
    content: "";
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--warn);
    display: inline-block;
  }
  header .pill.ok::before { background: var(--ok); }
  header .pill.bad::before { background: var(--bad); }
  main {
    max-width: 420px;
    margin: 0 auto;
    padding: 0 16px 24px;
    display: grid;
    gap: 16px;
  }
  .panel {
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: 20px;
    padding: 18px;
  }
  .panel h2 {
    margin: 0 0 14px;
    font-size: 14px;
    font-weight: 700;
    color: var(--txt);
  }
  .row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .stat {
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 14px;
    padding: 14px 12px;
    min-height: 80px;
    display: flex;
    flex-direction: column;
    justify-content: center;
  }
  .stat .k {
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--muted);
    font-weight: 600;
    line-height: 1.3;
  }
  .stat .v {
    font-size: 22px;
    font-weight: 700;
    margin-top: 6px;
    color: var(--txt);
    letter-spacing: -0.02em;
  }
  .stat.full { grid-column: 1 / -1; }
  .big-state {
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 14px;
    padding: 18px 14px;
    font-size: 28px;
    font-weight: 700;
    letter-spacing: -0.02em;
  }
  .big-state.off { color: var(--txt); }
  .big-state.on  { color: var(--ok); }
  .tbar {
    display: flex;
    gap: 10px;
    margin-top: 14px;
    flex-wrap: wrap;
  }
  button {
    font: inherit;
    cursor: pointer;
    background: var(--card);
    color: var(--txt);
    border: 1px solid var(--line);
    border-radius: 12px;
    padding: 10px 20px;
    font-size: 13px;
    font-weight: 600;
    transition: all 0.15s ease;
  }
  button:hover { filter: brightness(1.2); }
  button:active { transform: scale(0.97); }
  button.primary {
    background: var(--txt);
    color: var(--bg);
    border-color: transparent;
  }
  button.danger {
    background: var(--card);
    color: var(--bad);
    border-color: #3a1f23;
  }
  button.warn {
    background: var(--warn);
    color: #000;
    border-color: transparent;
  }
  .desc {
    font-size: 12px;
    color: var(--muted);
    line-height: 1.6;
    margin-top: 8px;
  }
  .desc b { color: var(--txt); font-weight: 600; }
  .ok { color: var(--ok); }
  .warn { color: var(--warn); }
  .bad { color: var(--bad); }
  .footer {
    color: var(--muted);
    font-size: 11px;
    text-align: center;
    padding: 14px 0;
    line-height: 1.6;
  }
  .footer a { color: var(--muted); text-decoration: none; }
  input[type=file] {
    width: 100%;
    font-size: 12px;
    color: var(--muted);
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 12px;
    padding: 10px 12px;
  }
  .progress {
    width: 100%;
    height: 8px;
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 6px;
    overflow: hidden;
    margin-top: 12px;
    display: none;
  }
  .progress.show { display: block; }
  .progress-bar {
    height: 100%;
    width: 0%;
    background: var(--accent);
    transition: width 0.15s ease;
  }
  .ota-msg {
    font-size: 12px;
    margin-top: 10px;
    color: var(--muted);
  }
  .ota-msg.ok  { color: var(--ok); }
  .ota-msg.bad { color: var(--bad); }
  .toggle-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    margin-top: 14px;
  }
  .toggle-row .lbl { font-size: 14px; font-weight: 600; color: var(--txt); }
  .lc-box {
    margin-top: 16px;
    padding-top: 16px;
    border-top: 1px solid var(--line);
  }
  .lc-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    gap: 10px;
    margin-bottom: 9px;
  }
  .lc-title {
    font-size: 13px;
    font-weight: 700;
    color: var(--txt);
  }
  .lc-current {
    font-size: 10px;
    color: var(--accent);
    font-weight: 700;
    white-space: nowrap;
  }
  .lc-sub {
    font-size: 10px;
    color: var(--muted);
    letter-spacing: .05em;
    text-transform: uppercase;
    margin-bottom: 10px;
  }
  .lc-buttons {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 7px;
  }
  .lc-buttons button {
    padding: 11px 7px;
    min-width: 0;
    font-size: 11px;
    white-space: nowrap;
  }
  .lc-buttons button.primary {
    background: var(--accent);
    color: #fff;
    border-color: transparent;
  }
  .switch { position: relative; width: 50px; height: 30px; flex-shrink: 0; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider {
    position: absolute;
    inset: 0;
    cursor: pointer;
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 30px;
    transition: 0.2s;
  }
  .slider::before {
    content: "";
    position: absolute;
    height: 22px;
    width: 22px;
    left: 3px;
    top: 3px;
    background: var(--txt);
    border-radius: 50%;
    transition: 0.2s;
  }
  .switch input:checked + .slider { background: var(--ok); border-color: transparent; }
  .switch input:checked + .slider::before { transform: translateX(20px); background: #000; }
</style>
</head>
<body>
<header>
  <h1>EU Summon Unlock <span id="hdr_ver" style="color:var(--muted);font-weight:600;">V2.1</span></h1>
  <span class="pill" id="conn">connecting…</span>
</header>
<main>

  <section class="panel">
    <div class="stat full" style="min-height:auto;padding:16px 14px;">
      <div class="k">State</div>
      <div class="v" id="big">OFF</div>
    </div>
    <div class="tbar">
      <button class="primary" onclick="post('/api/enable')">Enable</button>
      <button class="danger" onclick="post('/api/disable')">Disable</button>
      <button class="warn" id="btnForceMode">AP injection</button>
    </div>
  </section>

  <section class="panel">
    <h2>Traffic Light &amp; Stop Sign Control</h2>
    <div class="toggle-row">
      <span class="lbl">Enable TLSSC</span>
      <label class="switch">
        <input type="checkbox" id="tlsscToggle">
        <span class="slider"></span>
      </label>
    </div>
    <div class="desc">
      Injects <b>UI_fsdStopsControlEnabled = 1</b> on <b>0x3FD</b> mux0 bit38.<br>
      Off by default. Applied only while the injection gate is open.
    </div>

    <div class="lc-box">
      <div class="lc-head">
        <span class="lc-title">Lane Change</span>
        <span class="lc-current" id="blindspotCurrent">STANDARD</span>
      </div>
      <div class="lc-sub">BLINDSPOT CONFIG · AP / NOA ONLY</div>
      <div class="lc-buttons">
        <button id="blindspotStandard">STANDARD</button>
        <button id="blindspotAggressive">AGGRESSIVE</button>
        <button id="blindspotMadMax">MAD_MAX</button>
      </div>
    </div>
  </section>

  <section class="panel">
    <h2>Injection Gate</h2>
    <div class="row">
      <div class="stat"><div class="k">Gate</div><div class="v" id="gate_status">CLOSED</div></div>
      <div class="stat"><div class="k">APActive (info only)</div><div class="v" id="g_ap_v">OFF</div></div>
      <div class="stat"><div class="k">Parked</div><div class="v" id="g_pk_v">OFF</div></div>
      <div class="stat"><div class="k">Summoning</div><div class="v" id="g_su_v">OFF</div></div>
    </div>
    <div class="desc">
      Gate open if Parked OR Summoning only.<br>
      APActive (AP/TACC) only, doesn't start injection.
    </div>
  </section>


  <section class="panel">
    <h2>Live</h2>
    <div class="row">
      <div class="stat"><div class="k">280 (gear/ACA)</div><div class="v" id="s_280">0</div></div>
      <div class="stat"><div class="k">390 (DIF gear)</div><div class="v" id="s_390">0</div></div>
      <div class="stat"><div class="k">921 (AP status)</div><div class="v" id="s_921">0</div></div>
      <div class="stat"><div class="k">1016 (SPR)</div><div class="v" id="s_1016">0</div></div>
      <div class="stat"><div class="k">1021 mux1 rx</div><div class="v" id="s_rx">0</div></div>
      <div class="stat"><div class="k">TX ok</div><div class="v ok" id="s_ok">0</div></div>
      <div class="stat"><div class="k">TX fail</div><div class="v" id="s_fail">0</div></div>
      <div class="stat"><div class="k">CAN bus</div><div class="v" id="s_can">running</div></div>
      <div class="stat"><div class="k">Last 1021</div><div class="v" id="s_l1021">no</div></div>
      <div class="stat full"><div class="k">Uptime</div><div class="v" id="s_up">0 s</div></div>
    </div>
  </section>

  <section class="panel">
    <h2>TLSSC Restore For Banned car only</h2>
    <div class="toggle-row">
      <span class="lbl">DAS_autopilotConfig = SELF_DRIVING</span>
      <label class="switch">
        <input type="checkbox" id="tlrstToggle">
        <span class="slider"></span>
      </label>
    </div>
    <div class="desc">
      Rewrites <b>0x331</b> DAS_autopilotConfig.<br>
      Off by default. Applied only while the injection gate is open. May trigger an MCU reboot on the car.
    </div>
  </section>

  <section class="panel">
    <h2>Firmware / OTA Update</h2>
    <div class="row" style="margin-bottom:12px;">
      <div class="stat"><div class="k">Version</div><div class="v" id="fw_ver">—</div></div>
      <div class="stat"><div class="k">Free heap</div><div class="v" id="fw_free">—</div></div>
    </div>
    <input type="file" id="otaFile" accept=".bin">
    <div class="tbar">
      <button class="primary" id="btnOtaUpload" onclick="uploadOta()">Upload &amp; Flash</button>
    </div>
    <div class="progress" id="otaProgressWrap">
      <div class="progress-bar" id="otaProgressBar"></div>
    </div>
    <div class="ota-msg" id="otaMsg">Select a compiled .bin firmware file, then upload. The device reboots automatically after a successful flash.</div>
  </section>

  <div class="footer">
    <a href="/api/stats" target="_blank">/api/stats</a> ·
    research / educational only · not for use on public roads
  </div>
</main>

<script>
const $ = id => document.getElementById(id);
const CAN_STATES = ['running','running','bus-off','stopped'];
let otaUploading = false;

async function fetchStats() {
  if (otaUploading) return;
  try {
    const s = await fetch('/api/stats').then(r => r.json());

    // Big state
    const big = $('big');
    if (s.forceMode) {
      big.textContent = 'FORCE';
      big.className = 'v warn';
    } else {
      big.textContent = s.enabled ? 'ON' : 'OFF';
      big.className = 'v ' + (s.enabled ? 'ok' : '');
    }

    // Force mode button
    const btnForceMode = $('btnForceMode');
    if (btnForceMode) {
      btnForceMode.textContent = s.forceMode ? 'AP Injection: ON' : 'AP Injection: OFF';
      btnForceMode.style.opacity = s.forceMode ? '1' : '0.7';
    }

    // Gate
    $('gate_status').textContent = s.gate ? 'OPEN' : 'CLOSED';
    $('gate_status').className = 'v ' + (s.gate ? 'ok' : 'bad');

    $('g_ap_v').textContent = s.ap ? 'ON' : 'OFF';
    $('g_ap_v').className = 'v ' + (s.ap ? 'ok' : '');
    $('g_pk_v').textContent = s.parked ? 'ON' : 'OFF';
    $('g_pk_v').className = 'v ' + (s.parked ? 'ok' : '');
    $('g_su_v').textContent = s.summon ? 'ON' : 'OFF';
    $('g_su_v').className = 'v ' + (s.summon ? 'ok' : '');

    // Compteurs
    $('s_280').textContent  = s.rx280;
    $('s_390').textContent  = s.rx390;
    $('s_921').textContent  = s.rx921;
    $('s_1016').textContent = s.rx1016;
    $('s_rx').textContent   = s.rxMux1;
    $('s_ok').textContent   = s.txOk;
    $('s_fail').textContent = s.txFail;
    $('s_fail').className   = 'v ' + (s.txFail > 0 ? 'warn' : '');

    const cs = CAN_STATES[s.canState] ?? String(s.canState);
    $('s_can').textContent = cs;
    $('s_can').className   = 'v ' + (s.canState === 0 ? 'ok' : s.canState === 2 ? 'bad' : 'warn');

    $('s_l1021').textContent = s.last1021 ? 'yes' : 'no';
    $('s_l1021').className = 'v ' + (s.last1021 ? 'ok' : 'warn');

    const u = s.uptimeS;
    $('s_up').textContent = u < 60 ? u + ' s' : Math.floor(u/60) + 'm' + (u%60) + 's';

    if (s.fwVersion) {
      $('fw_ver').textContent = s.fwVersion;
      $('hdr_ver').textContent = s.fwVersion;
    }
    if (s.freeHeap !== undefined) $('fw_free').textContent = Math.round(s.freeHeap/1024) + ' KB';

    const tg = $('tlsscToggle');
    if (tg && document.activeElement !== tg) tg.checked = !!s.tlssc;

    const gr = $('tlrstToggle');
    if (gr && document.activeElement !== gr) gr.checked = !!s.tlrst;

    const bs = Number(s.blindspotConfig ?? 0);
    const bsNames = ['STANDARD', 'AGGRESSIVE', 'MAD_MAX'];
    $('blindspotCurrent').textContent = bsNames[bs] || 'STANDARD';
    $('blindspotStandard').classList.toggle('primary', bs === 0);
    $('blindspotAggressive').classList.toggle('primary', bs === 1);
    $('blindspotMadMax').classList.toggle('primary', bs === 2);

    $('conn').textContent = 'connected';
    $('conn').className   = 'pill ok';
  } catch {
    $('conn').textContent = 'lost';
    $('conn').className   = 'pill bad';
  }
}

// ── OTA upload ───────────────────────────────────────────────
function uploadOta() {
  const input = $('otaFile');
  const file = input.files[0];
  const msg = $('otaMsg');
  const wrap = $('otaProgressWrap');
  const bar = $('otaProgressBar');
  const btn = $('btnOtaUpload');

  if (!file) {
    msg.textContent = 'Please choose a .bin file first.';
    msg.className = 'ota-msg bad';
    return;
  }

  const form = new FormData();
  form.append('update', file, file.name);

  otaUploading = true;
  btn.disabled = true;
  input.disabled = true;
  wrap.className = 'progress show';
  bar.style.width = '0%';
  msg.textContent = 'Uploading ' + file.name + '…';
  msg.className = 'ota-msg';

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update', true);

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      bar.style.width = pct + '%';
      msg.textContent = 'Uploading… ' + pct + '%';
    }
  };

  xhr.onload = () => {
    let ok = xhr.status === 200;
    let errText = '';
    try {
      const r = JSON.parse(xhr.responseText);
      ok = ok && r.ok;
      errText = r.error || '';
    } catch {}

    if (ok) {
      bar.style.width = '100%';
      msg.textContent = 'Flash successful — rebooting…';
      msg.className = 'ota-msg ok';
      setTimeout(() => location.reload(), 6000);
    } else {
      msg.textContent = 'OTA failed' + (errText ? ': ' + errText : '');
      msg.className = 'ota-msg bad';
      btn.disabled = false;
      input.disabled = false;
      otaUploading = false;
    }
  };

  xhr.onerror = () => {
    msg.textContent = 'Upload error — device likely rebooted or connection lost.';
    msg.className = 'ota-msg bad';
    btn.disabled = false;
    input.disabled = false;
    otaUploading = false;
  };

  xhr.send(form);
}

async function post(url) {
  await fetch(url, { method: 'POST' });
  fetchStats();
}

$('tlsscToggle').addEventListener('change', (e) => {
  post(e.target.checked ? '/api/tlssc-enable' : '/api/tlssc-disable');
});

$('tlrstToggle').addEventListener('change', (e) => {
  post(e.target.checked ? '/api/tlrst-enable' : '/api/tlrst-disable');
});

async function setBlindspotConfig(mode) {
  try {
    await fetch('/api/blindspot?mode=' + mode, { method: 'POST' });
    fetchStats();
  } catch {}
}

$('blindspotStandard').addEventListener('click', () => setBlindspotConfig(0));
$('blindspotAggressive').addEventListener('click', () => setBlindspotConfig(1));
$('blindspotMadMax').addEventListener('click', () => setBlindspotConfig(2));

fetchStats();
setInterval(fetchStats, 800);
</script>
</body>
</html>
)HTML";
