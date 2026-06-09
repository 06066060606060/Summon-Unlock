// Dashboard HTML — PROGMEM, pas de CDN ni SPIFFS.

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SummonUnlock</title>
<style>
  :root {
    --bg:#0d1117; --panel:#161b22; --line:#30363d; --txt:#c9d1d9;
    --muted:#8b949e; --accent:#58a6ff; --ok:#3fb950; --warn:#d29922; --bad:#f85149;
  }
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--txt);font:13px/1.5 ui-monospace,Consolas,Menlo,monospace}
  header{padding:12px 16px;border-bottom:1px solid var(--line);display:flex;justify-content:space-between;align-items:center}
  header h1{margin:0;font-size:15px;font-weight:700;color:var(--accent)}
  .pill{font-size:11px;padding:2px 8px;border:1px solid var(--line);border-radius:10px;color:var(--muted)}
  main{max-width:560px;margin:0 auto;padding:14px;display:grid;gap:12px}

  /* Panel */
  .panel{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px}
  .panel h2{margin:0 0 10px;font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.08em;color:var(--muted)}

  /* Gros toggle */
  .big{font-size:52px;font-weight:700;text-align:center;padding:16px 0;border-radius:8px;border:2px solid var(--line);letter-spacing:2px;transition:color .2s,border-color .2s}
  .big.on {color:var(--ok);border-color:var(--ok)}
  .big.off{color:var(--bad);border-color:var(--bad)}
  .tbar{display:flex;gap:8px;justify-content:center;margin-top:10px}
  button{font:inherit;cursor:pointer;border-radius:6px;padding:9px 24px;font-size:13px;font-weight:600;border:1px solid var(--line);background:#21262d;color:var(--txt)}
  button.primary{background:var(--ok);color:#0d1117;border-color:transparent}
  button.danger{color:var(--bad);border-color:#3a1f23}
  button:hover{filter:brightness(1.15)}

  /* Injection Gate */
  .gate{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:10px}
  .gbox{border:1px solid var(--line);border-radius:6px;padding:8px 10px;text-align:center}
  .gbox .k{font-size:10px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}
  .gbox .v{font-size:15px;font-weight:700;margin-top:3px}
  .gbox.active{border-color:var(--ok);background:#0d2a15}
  .gbox.inactive{border-color:var(--line)}
  .gate-status{text-align:center;font-size:13px;font-weight:600;padding:8px;border-radius:6px;border:1px solid var(--line)}
  .gate-status.open{color:var(--ok);border-color:var(--ok);background:#0d2a15}
  .gate-status.closed{color:var(--bad);border-color:#3a1f23;background:#2a0d0d}

  /* Summon discrimination */
  .disc{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .dbox{border:1px solid var(--line);border-radius:6px;padding:8px 10px}
  .dbox .k{font-size:10px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}
  .dbox .v{font-size:13px;font-weight:600;margin-top:3px}

  /* Stats */
  .row{display:grid;grid-template-columns:repeat(auto-fit,minmax(100px,1fr));gap:8px}
  .stat{background:#0d1117;border:1px solid var(--line);border-radius:6px;padding:7px 10px}
  .stat .k{font-size:10px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}
  .stat .v{font-size:17px;font-weight:600;margin-top:2px}

  /* Règle */
  .rule{font-size:11px;color:var(--muted);line-height:1.75;margin-top:10px;border-top:1px solid var(--line);padding-top:10px}
  .rule code{color:var(--txt);background:#0d1117;padding:1px 5px;border-radius:3px;font-size:11px}

  .ok{color:var(--ok)} .warn{color:var(--warn)} .bad{color:var(--bad)}
  .footer{color:var(--muted);font-size:11px;text-align:center;padding:8px 0}
  .footer a{color:var(--muted)}
</style>
</head>
<body>
<header>
  <h1>SummonUnlock</h1>
  <span class="pill" id="conn">connecting&hellip;</span>
</header>
<main>

  <!-- Toggle principal -->
  <section class="panel">
    <h2>Summon Unlock</h2>
    <div class="big" id="big">—</div>
    <div class="tbar">
      <button class="primary" onclick="post('/api/enable')">Enable</button>
      <button class="danger"  onclick="post('/api/disable')">Disable</button>
    </div>
  </section>

  <!-- Injection Gate -->
  <section class="panel">
    <h2>Injection Gate</h2>
    <div class="gate-status" id="gate_status">—</div>
    <br>
    <div class="gate" style="grid-template-columns:1fr 1fr">
      <div class="gbox" id="g_pk">
        <div class="k">Parked</div>
        <div class="v" id="g_pk_v">—</div>
      </div>
      <div class="gbox" id="g_su">
        <div class="k">Summoning</div>
        <div class="v" id="g_su_v">—</div>
      </div>
    </div>
    <div style="margin-top:8px;font-size:11px;color:var(--muted);display:flex;align-items:center;gap:6px">
      <span>APActive (info only) :</span>
      <span id="g_ap_v" style="font-weight:600">—</span>
    </div>
    <p style="font-size:11px;color:var(--muted);margin:6px 0 0">
      Gate ouverte si Parked OR Summoning uniquement.<br>
      APActive (AP/TACC) seul ne déclenche pas l'injection.
    </p>
  </section>

  <!-- Discrimination Summon vs TACC -->
  <section class="panel">
    <h2>Discrimination Summon / TACC</h2>
    <div class="disc">
      <div class="dbox">
        <div class="k">ACA — DI_autonomyControlActive</div>
        <div class="v" id="d_aca">—</div>
        <div style="font-size:10px;color:var(--muted);margin-top:2px">CAN 280 · data[6] bit 2</div>
      </div>
      <div class="dbox">
        <div class="k">SPR — UI_selfParkRequest</div>
        <div class="v" id="d_spr">—</div>
        <div style="font-size:10px;color:var(--muted);margin-top:2px">CAN 1016 · data[3] bits 4-7</div>
      </div>
    </div>
    <p style="font-size:11px;color:var(--muted);margin:8px 0 0">
      Summoning = ACA &amp;&amp; SPR. TACC seul (ACA=1, SPR=0) ne déclenche pas l'injection.
    </p>
  </section>

  <!-- Compteurs CAN -->
  <section class="panel">
    <h2>Frames CAN</h2>
    <div class="row">
      <div class="stat"><div class="k">280 (gear/ACA)</div><div class="v" id="s_280">—</div></div>
      <div class="stat"><div class="k">390 (DIF gear)</div><div class="v" id="s_390">—</div></div>
      <div class="stat"><div class="k">921 (AP status)</div><div class="v" id="s_921">—</div></div>
      <div class="stat"><div class="k">1016 (SPR)</div><div class="v" id="s_1016">—</div></div>
      <div class="stat"><div class="k">1021 mux1 rx</div><div class="v" id="s_rx">—</div></div>
      <div class="stat"><div class="k">TX ok</div><div class="v ok" id="s_ok">—</div></div>
      <div class="stat"><div class="k">TX fail</div><div class="v" id="s_fail">—</div></div>
      <div class="stat"><div class="k">CAN bus</div><div class="v" id="s_can">—</div></div>
      <div class="stat"><div class="k">Uptime</div><div class="v" id="s_up">—</div></div>
    </div>
  </section>

  <!-- Règle -->
  <section class="panel">
    <h2>Règle appliquée</h2>
    <div class="rule">
      ID <code>0x3FD</code> (1021) &mdash; mux <code>1</code><br>
      &bull; bit <code>19</code> &rarr; <code>0</code> &mdash; Clears the summon EU restriction bit<br>
      &bull; bit <code>47</code> &rarr; <code>1</code> &mdash; Sets the summon enable bit<br><br>
      Condition d'injection :<br>
      &nbsp;&nbsp;<code>summonEnabled &amp;&amp; (Parked || Summoning)</code><br>
      Summoning :<br>
      &nbsp;&nbsp;<code>lastAca &amp;&amp; sprSeen</code> — ACA tombe → sprSeen effacé
    </div>
  </section>

  <div class="footer">
    <a href="/api/stats" target="_blank">/api/stats</a> &middot;
    research / educational only &middot; not for use on public roads
  </div>
</main>

<script>
const $ = id => document.getElementById(id);
const CAN_STATES = ['running','recovering','bus-off','stopped'];

function setGbox(id, active) {
  const el = $(id);
  el.className = 'gbox ' + (active ? 'active' : 'inactive');
}

async function fetchStats() {
  try {
    const s = await fetch('/api/stats').then(r => r.json());

    // Toggle
    const big = $('big');
    big.textContent = s.enabled ? 'ON' : 'OFF';
    big.className   = 'big ' + (s.enabled ? 'on' : 'off');

    // Gate
    const gate = s.gate;
    const gs = $('gate_status');
    gs.textContent = gate ? 'OPEN — injection autorisée' : 'CLOSED — injection bloquée';
    gs.className   = 'gate-status ' + (gate ? 'open' : 'closed');

    $('g_ap_v').textContent = s.ap ? 'ON' : 'off';
    $('g_ap_v').style.color = s.ap ? 'var(--ok)' : 'var(--muted)';
    setGbox('g_pk', s.parked); $('g_pk_v').textContent = s.parked ? 'ON' : 'off';
    setGbox('g_su', s.summon); $('g_su_v').textContent = s.summon ? 'ON' : 'off';

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

    const u = s.uptimeS;
    $('s_up').textContent = u < 60 ? u + 's' : Math.floor(u/60) + 'm' + (u%60) + 's';

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
