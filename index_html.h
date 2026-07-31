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
</style>
</head>
<body>
<header>
  <h1>EU Summon Unlock V2</h1>
  <span class="pill" id="conn">connecting…</span>
</header>
<main>

  <section class="panel">
    <h2>Summon Unlock V2</h2>
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

  <div class="footer">
    <a href="/api/stats" target="_blank">/api/stats</a> ·
    research / educational only · not for use on public roads
  </div>
</main>

<script>
const $ = id => document.getElementById(id);
const CAN_STATES = ['running','running','bus-off','stopped'];

async function fetchStats() {
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

    // Discrimination
    $('d_aca').textContent = s.aca ? 'ACTIVE' : 'inactive';
    $('d_aca').className = 'v ' + (s.aca ? 'ok' : '');
    $('d_spr').textContent = s.spr ? 'SEEN' : 'not seen';
    $('d_spr').className = 'v ' + (s.spr ? 'ok' : '');

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

    $('conn').textContent = 'connected';
    $('conn').className   = 'pill ok';
  } catch {
    $('conn').textContent = 'lost';
    $('conn').className   = 'pill bad';
  }
}

async function post(url) {
  await fetch(url, { method: 'POST' });
  fetchStats();
}

fetchStats();
setInterval(fetchStats, 800);
</script>
</body>
</html>
)HTML";
