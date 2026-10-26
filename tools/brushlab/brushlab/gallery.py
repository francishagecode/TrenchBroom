"""Generates the review gallery: one self-contained HTML page."""

from __future__ import annotations

import base64
import html
import json
import os
from typing import Any, Dict, List

from . import project


def _data_uri(path: str) -> str:
    with open(path, "rb") as f:
        return "data:image/png;base64," + base64.b64encode(f.read()).decode()


def collect(force: bool = False) -> List[Dict[str, Any]]:
    review = project.load_review()
    cards = []
    for name in project.list_designs():
        results = project.build_design(name, force=force)
        variants = []
        for r in results:
            v = r.get("variant", "?")
            status, revised = project.effective_status(review, name, v, r)
            entry = project.review_entry(review, name, v)
            item = dict(r)
            item.update({"status": status, "revised": revised, "review": entry})
            if r.get("paste") and os.path.exists(r["paste"]):
                with open(r["paste"]) as f:
                    item["paste_text"] = f.read()
            mesh_path = os.path.join(project.out_dir(name, v), "mesh.json")
            if not r.get("error") and os.path.exists(mesh_path):
                with open(mesh_path) as f:
                    item["mesh"] = json.load(f)
            variants.append(item)
        defaults = {}
        try:
            defaults = dict(project.load_module(name).get("PARAMS", {}))
        except Exception:
            pass
        cards.append({"name": name, "variants": variants, "defaults": defaults})
    return cards


def write_gallery(path: str, force: bool = False, inline_images: bool = False,
                  server: bool = False) -> str:
    cards = collect(force)
    base = os.path.dirname(os.path.abspath(path))
    for card in cards:
        for v in card["variants"]:
            for key in ("sheet", "thumb"):
                if v.get(key) and os.path.exists(v[key]):
                    v[key + "_url"] = (
                        _data_uri(v[key]) if inline_images
                        else os.path.relpath(v[key], base).replace(os.sep, "/")
                    )
            for k in ("map", "paste", "sheet", "thumb", "key", "traceback"):
                if k == "traceback" and v.get("error"):
                    continue
                v.pop(k, None)
    payload = json.dumps({"cards": cards, "server": server}, separators=(",", ":"))
    payload = payload.replace("</", "<\\/")
    page = TEMPLATE.replace("/*DATA*/", payload)
    with open(path, "w") as f:
        f.write(page)
    return path


TEMPLATE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Brush Lab review</title>
<style>
:root { --bg:#16191f; --panel:#1f242c; --line:#2e3540; --text:#e6e9ef; --dim:#9aa4b2;
  --ok:#3fb67a; --warn:#e0a83a; --bad:#e05a5a; --info:#5aa0e0; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text); font:14px/1.45 -apple-system,
  BlinkMacSystemFont, "Segoe UI", sans-serif; }
header { position:sticky; top:0; z-index:5; display:flex; gap:16px; align-items:center;
  padding:10px 20px; background:#111419; border-bottom:1px solid var(--line); }
header h1 { font-size:17px; margin:0; font-weight:600; }
header .filters button { margin-right:4px; }
header .counts { color:var(--dim); margin-left:auto; }
button { background:#2a313b; color:var(--text); border:1px solid #3a4350; border-radius:6px;
  padding:5px 10px; cursor:pointer; font:inherit; }
button:hover { background:#343c48; }
button.active { background:#3d4a5c; border-color:#5a6a80; }
button.approve { border-color:var(--ok); } button.changes { border-color:var(--warn); }
button.reject { border-color:var(--bad); }
main { padding:16px 20px 60px; display:flex; flex-direction:column; gap:18px; }
.card { background:var(--panel); border:1px solid var(--line); border-radius:10px; overflow:hidden; }
.card-head { display:flex; align-items:baseline; gap:12px; padding:12px 16px;
  border-bottom:1px solid var(--line); flex-wrap:wrap; }
.card-head h2 { margin:0; font-size:16px; } .card-head code { color:var(--dim); }
.tag { font-size:12px; color:var(--dim); border:1px solid var(--line); border-radius:10px; padding:0 7px; }
.tabs { display:flex; gap:6px; padding:10px 16px 0; flex-wrap:wrap; }
.tab { display:flex; gap:6px; align-items:center; }
.pill { font-size:11px; text-transform:uppercase; letter-spacing:.04em; border-radius:9px;
  padding:1px 7px; font-weight:600; }
.pill.proposed { background:#2b3f57; color:#9cc6f0; } .pill.approved { background:#1f4a35; color:#7fe0ac; }
.pill.changes { background:#55431c; color:#f0cc7a; } .pill.rejected { background:#572626; color:#f09a9a; }
.pill.revised { background:#3d2b57; color:#c9a6f0; } .pill.error { background:#572626; color:#f09a9a; }
.body { display:grid; grid-template-columns:minmax(0,1.6fr) minmax(300px,1fr); gap:16px; padding:14px 16px 16px; }
.visual { position:relative; background:#111; border-radius:8px; overflow:hidden; aspect-ratio: 1200/1254; }
.visual img, .visual canvas { width:100%; height:100%; display:block; object-fit:contain; }
.visual .mode { position:absolute; top:8px; right:8px; display:flex; gap:4px; }
.visual .hint { position:absolute; bottom:8px; left:10px; color:var(--dim); font-size:12px; pointer-events:none; }
.side h3 { font-size:12px; text-transform:uppercase; letter-spacing:.06em; color:var(--dim); margin:14px 0 6px; }
.side h3:first-child { margin-top:0; }
.desc { white-space:pre-wrap; }
table.params { border-collapse:collapse; width:100%; font-size:13px; }
table.params td { border-bottom:1px solid var(--line); padding:3px 6px; }
table.params td:first-child { color:var(--dim); width:45%; }
table.params tr.changed td:last-child { color:#f0cc7a; font-weight:600; }
.issues li.error { color:#f09a9a; } .issues li.warning { color:#f0cc7a; } .issues li.info { color:var(--dim); }
ul { margin:0; padding-left:18px; }
.actions { display:flex; flex-wrap:wrap; gap:6px; margin-top:8px; }
textarea { width:100%; min-height:62px; background:#151920; color:var(--text);
  border:1px solid var(--line); border-radius:6px; padding:6px; font:inherit; }
.history { font-size:12px; color:var(--dim); }
.feedback { border-left:3px solid var(--warn); padding:4px 8px; background:#2a2618; margin-top:6px; }
pre.err { white-space:pre-wrap; color:#f09a9a; font-size:12px; background:#1a1416; padding:8px; border-radius:6px; }
.toast { position:fixed; bottom:18px; left:50%; transform:translateX(-50%); background:#2f3a48;
  padding:8px 14px; border-radius:8px; opacity:0; transition:opacity .2s; pointer-events:none; }
.toast.show { opacity:1; }
.cli { font-family:ui-monospace,Menlo,monospace; font-size:12px; background:#151920; padding:6px;
  border-radius:6px; word-break:break-all; margin-top:6px; color:#c8d0dc; }
@media (max-width: 1000px) { .body { grid-template-columns:1fr; } }
</style>
</head>
<body>
<header>
  <h1>Brush Lab review</h1>
  <div class="filters" id="filters"></div>
  <div class="counts" id="counts"></div>
</header>
<main id="main"></main>
<div class="toast" id="toast"></div>
<script id="data" type="application/json">/*DATA*/</script>
<script>
const DATA = JSON.parse(document.getElementById('data').textContent);
const state = { filter: 'review', selected: {} };
const esc = s => String(s ?? '').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

function toast(msg) {
  const t = document.getElementById('toast'); t.textContent = msg; t.classList.add('show');
  clearTimeout(toast.h); toast.h = setTimeout(() => t.classList.remove('show'), 1800);
}

function needsReview(v) { return v.error || v.status === 'proposed' || v.revised; }
function visible(card) {
  if (state.filter === 'all') return true;
  return card.variants.some(v => state.filter === 'review' ? needsReview(v) : v.status === state.filter);
}

function renderFilters() {
  const all = DATA.cards.flatMap(c => c.variants);
  const n = f => all.filter(v => f === 'review' ? needsReview(v) : v.status === f).length;
  const opts = [['review', 'Needs review'], ['approved', 'Approved'], ['changes', 'Changes requested'],
                ['rejected', 'Rejected'], ['all', 'All']];
  document.getElementById('filters').innerHTML = opts.map(([k, label]) =>
    `<button class="${state.filter === k ? 'active' : ''}" onclick="setFilter('${k}')">${label}${k === 'all' ? '' : ' (' + n(k) + ')'}</button>`).join('');
  document.getElementById('counts').textContent =
    `${DATA.cards.length} designs, ${all.length} variants` + (DATA.server ? '' : ' (static: use the CLI commands to record reviews)');
}
function setFilter(f) { state.filter = f; render(); }

function pill(v) {
  if (v.error) return '<span class="pill error">error</span>';
  let s = `<span class="pill ${v.status}">${v.status === 'changes' ? 'changes' : v.status}</span>`;
  if (v.revised) s += '<span class="pill revised">revised</span>';
  return s;
}

function render() {
  renderFilters();
  const main = document.getElementById('main');
  const cards = DATA.cards.filter(visible);
  if (!cards.length) { main.innerHTML = '<p style="color:var(--dim)">Nothing here. Try "All".</p>'; return; }
  main.innerHTML = cards.map(card => {
    const sel = state.selected[card.name] || (card.variants.find(needsReview) || card.variants[0]).variant;
    state.selected[card.name] = sel;
    const v = card.variants.find(x => x.variant === sel) || card.variants[0];
    const tags = (v.tags || []).map(t => `<span class="tag">${esc(t)}</span>`).join(' ');
    return `<section class="card" id="card-${card.name}">
      <div class="card-head"><h2>${esc(v.title || card.name)}</h2><code>${esc(card.name)}.py</code>${tags}</div>
      <div class="tabs">${card.variants.map(x => `<button class="tab ${x.variant === sel ? 'active' : ''}"
         onclick="selectVariant('${card.name}','${x.variant}')">${esc(x.variant)} ${pill(x)}</button>`).join('')}</div>
      ${variantBody(card, v)}
    </section>`;
  }).join('');
  for (const card of cards) {
    const v = card.variants.find(x => x.variant === state.selected[card.name]);
    if (v && v.mesh && state['mode-' + card.name] === '3d') startViewer(card.name, v.mesh);
  }
}

function selectVariant(name, variant) { state.selected[name] = variant; render(); }

function variantBody(card, v) {
  const ref = `${card.name}@${v.variant}`;
  if (v.error) {
    return `<div class="body"><div><pre class="err">${esc(v.traceback || v.error)}</pre></div>
      <div class="side"><h3>Build failed</h3><p>${esc(v.error)}</p></div></div>`;
  }
  const mode = state['mode-' + card.name] || 'sheet';
  const visual = mode === '3d'
    ? `<canvas id="gl-${card.name}"></canvas><div class="hint">drag: orbit · shift-drag / right-drag: pan · wheel: zoom</div>`
      + (v.mesh && v.mesh.roof.pos.length ? `<button style="position:absolute;top:8px;left:8px" onclick="toggleRoof('${card.name}')">${state.hideRoof ? 'Show' : 'Hide'} ceilings / roofs</button>` : '')
    : `<img src="${esc(v.sheet_url)}" alt="preview sheet" onclick="window.open(this.src)">`;
  const params = Object.entries(v.params || {}).map(([k, val]) => {
    const changed = JSON.stringify(card.defaults[k]) !== JSON.stringify(val);
    return `<tr class="${changed ? 'changed' : ''}"><td>${esc(k)}</td><td>${esc(JSON.stringify(val))}</td></tr>`;
  }).join('');
  const issues = (v.issues || []).map(i => `<li class="${i.level}">${esc(i.message)}</li>`).join('') || '<li>none</li>';
  const notes = (v.notes || []).map(n => `<li>${esc(n)}</li>`).join('');
  const r = v.review || {};
  const fb = r.note ? `<div class="feedback"><b>${esc(r.status)}</b> (${esc(r.time)}): ${esc(r.note)}</div>` : '';
  const hist = (r.history || []).slice(-4).reverse().map(h => `<div>${esc(h.time)} · ${esc(h.status)}${h.note ? ' · ' + esc(h.note) : ''}</div>`).join('');
  const groups = Object.entries(v.groups || {}).map(([g, n]) => `${esc(g)} (${n})`).join(', ');
  const actions = DATA.server
    ? `<textarea id="note-${card.name}" placeholder="Feedback for the agent (what to change, what you like)…"></textarea>
       <div class="actions">
         <button class="approve" onclick="review('${card.name}','${v.variant}','approved')">Approve</button>
         <button class="changes" onclick="review('${card.name}','${v.variant}','changes')">Request changes</button>
         <button class="reject" onclick="review('${card.name}','${v.variant}','rejected')">Reject</button>
       </div>`
    : `<div class="cli">brushlab review ${esc(ref)} approved -m "…"</div>
       <div class="cli">brushlab review ${esc(ref)} changes -m "make the railings taller"</div>`;
  return `<div class="body">
    <div class="visual">${visual}
      <div class="mode"><button class="${mode === 'sheet' ? 'active' : ''}" onclick="setMode('${card.name}','sheet')">Sheet</button>
      <button class="${mode === '3d' ? 'active' : ''}" onclick="setMode('${card.name}','3d')">3D</button></div></div>
    <div class="side">
      ${v.description ? `<h3>Description</h3><div class="desc">${esc(v.description)}</div>` : ''}
      <h3>Stats</h3><div>${v.brushes} brushes · ${v.size.map(x => +x.toFixed(1)).join(' × ')} units${v.entities.length ? ' · entities: ' + esc(v.entities.join(', ')) : ''}</div>
      <div style="color:var(--dim);font-size:12px">groups: ${groups}</div>
      ${notes ? `<h3>Agent notes</h3><ul>${notes}</ul>` : ''}
      <h3>Parameters</h3><table class="params">${params}</table>
      <h3>Checks</h3><ul class="issues">${issues}</ul>
      <h3>Review</h3>${fb}${actions}
      <div class="actions">
        <button onclick="copyPaste('${card.name}','${v.variant}')">Copy for TrenchBroom</button>
        <button onclick="downloadMap('${card.name}','${v.variant}')">Download .map</button>
      </div>
      ${hist ? `<h3>History</h3><div class="history">${hist}</div>` : ''}
    </div></div>`;
}

function toggleRoof(name) { state.hideRoof = !state.hideRoof; render(); }
function setMode(name, mode) { state['mode-' + name] = mode; render(); }
function findVariant(name, variant) { return DATA.cards.find(c => c.name === name).variants.find(v => v.variant === variant); }

async function copyPaste(name, variant) {
  const v = findVariant(name, variant);
  try { await navigator.clipboard.writeText(v.paste_text); toast('Copied. Paste into TrenchBroom with Cmd/Ctrl+V'); }
  catch (e) { toast('Clipboard blocked; use: brushlab copy ' + name + '@' + variant); }
}
function downloadMap(name, variant) {
  const v = findVariant(name, variant);
  const text = '// Game: Quake\n// Format: Standard\n// entity 0\n{\n"classname" "worldspawn"\n}\n' + v.paste_text;
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([text], {type: 'text/plain'}));
  a.download = `${name}--${variant}.map`; a.click();
}

async function review(name, variant, status) {
  const note = (document.getElementById('note-' + name) || {}).value || '';
  if (status !== 'approved' && !note.trim() && !confirm('No feedback note. Record "' + status + '" anyway?')) return;
  const res = await fetch('/api/review', { method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({design: name, variant, status, note}) });
  const body = await res.json();
  if (!res.ok) { toast('Error: ' + body.error); return; }
  const v = findVariant(name, variant);
  v.status = status; v.revised = false; v.review = body.entry;
  toast(status === 'approved' ? 'Approved → ' + (body.entry.approved_map || '') : 'Recorded: ' + status);
  render();
}

// ---- minimal WebGL viewer ------------------------------------------------------------
const viewers = {};
function startViewer(name, mesh) {
  const canvas = document.getElementById('gl-' + name);
  if (!canvas) return;
  const dpr = window.devicePixelRatio || 1;
  canvas.width = canvas.clientWidth * dpr; canvas.height = canvas.clientHeight * dpr;
  const gl = canvas.getContext('webgl', {antialias: true});
  if (!gl) { canvas.replaceWith(Object.assign(document.createElement('p'), {textContent: 'WebGL unavailable'})); return; }
  const vs = `attribute vec3 p; attribute vec3 n; attribute vec3 c; uniform mat4 m; varying vec3 vc;
    void main(){ gl_Position = m * vec4(p,1.0); vec3 L = normalize(vec3(0.35,-0.55,0.75));
    float s = 0.42 + 0.58*max(dot(n,L),0.0); s *= 0.92 + 0.08*abs(n.x); vc = c*s; }`;
  const fs = `precision mediump float; varying vec3 vc; uniform float e; void main(){ gl_FragColor = vec4(vc*e,1.0); }`;
  const sh = (t, s) => { const o = gl.createShader(t); gl.shaderSource(o, s); gl.compileShader(o); return o; };
  const prog = gl.createProgram();
  gl.attachShader(prog, sh(gl.VERTEX_SHADER, vs)); gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, fs));
  gl.linkProgram(prog); gl.useProgram(prog);
  const buf = (data, attr, size) => { const b = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, b);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(data), gl.STATIC_DRAW); return () => {
    const loc = gl.getAttribLocation(prog, attr); gl.bindBuffer(gl.ARRAY_BUFFER, b);
    gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, size, gl.FLOAT, false, 0, 0); }; };
  const parts = ['base', 'roof'].map(k => {
    const m = mesh[k], nE = m.edges.length / 3;
    return { k, nT: m.pos.length / 3, nE,
      tri: [buf(m.pos, 'p', 3), buf(m.nrm, 'n', 3), buf(m.col, 'c', 3)],
      edge: [buf(m.edges, 'p', 3), buf(new Array(nE * 3).fill(0).map((_, i) => i % 3 === 2 ? 1 : 0), 'n', 3),
             buf(new Array(nE * 3).fill(0.12), 'c', 3)] };
  });
  const [lo, hi] = mesh.bounds;
  const center = [0, 1, 2].map(i => (lo[i] + hi[i]) / 2);
  const radius = Math.max(64, Math.hypot(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]) / 2);
  const cam = { yaw: -2.35, pitch: 0.55, dist: radius * 2.6, target: center.slice() };
  const uM = gl.getUniformLocation(prog, 'm'), uE = gl.getUniformLocation(prog, 'e');
  gl.enable(gl.DEPTH_TEST);

  function matrix() {
    const cp = Math.cos(cam.pitch), eye = [cam.target[0] + cam.dist * cp * Math.cos(cam.yaw),
      cam.target[1] + cam.dist * cp * Math.sin(cam.yaw), cam.target[2] + cam.dist * Math.sin(cam.pitch)];
    const f = norm(sub(cam.target, eye)), s = norm(cross(f, [0, 0, 1])), u = cross(s, f);
    const view = [s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0,
      -dot(s, eye), -dot(u, eye), dot(f, eye), 1];
    const aspect = canvas.width / canvas.height, near = cam.dist / 50, far = cam.dist * 10;
    const t = 1 / Math.tan(0.5 * 0.9);
    const proj = [t / aspect, 0, 0, 0, 0, t, 0, 0, 0, 0, (far + near) / (near - far), -1, 0, 0, 2 * far * near / (near - far), 0];
    return mul(proj, view);
  }
  function draw() {
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clearColor(0.133, 0.149, 0.18, 1); gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.uniformMatrix4fv(uM, false, new Float32Array(matrix()));
    gl.uniform1f(uE, 1.0);
    for (const part of parts) {
      if (part.k === 'roof' && state.hideRoof) continue;
      if (!part.nT) continue;
      gl.enable(gl.POLYGON_OFFSET_FILL); gl.polygonOffset(1, 1);
      part.tri.forEach(f => f()); gl.drawArrays(gl.TRIANGLES, 0, part.nT);
      gl.disable(gl.POLYGON_OFFSET_FILL);
      part.edge.forEach(f => f()); gl.drawArrays(gl.LINES, 0, part.nE);
    }
  }
  viewers[name] = draw;
  let drag = null;
  canvas.oncontextmenu = e => e.preventDefault();
  canvas.onmousedown = e => { drag = { x: e.clientX, y: e.clientY, pan: e.shiftKey || e.button === 2 }; };
  window.addEventListener('mouseup', () => drag = null);
  canvas.onmousemove = e => {
    if (!drag) return;
    const dx = e.clientX - drag.x, dy = e.clientY - drag.y; drag.x = e.clientX; drag.y = e.clientY;
    if (drag.pan) {
      const k = cam.dist / canvas.clientHeight, cy = Math.cos(cam.yaw), sy = Math.sin(cam.yaw);
      cam.target[0] += (-sy * -dx) * k + (-cy * Math.sin(cam.pitch) * dy) * k;
      cam.target[1] += (cy * -dx) * k + (-sy * Math.sin(cam.pitch) * dy) * k;
      cam.target[2] += Math.cos(cam.pitch) * dy * k;
    } else {
      cam.yaw -= dx * 0.008; cam.pitch = Math.max(-1.5, Math.min(1.5, cam.pitch + dy * 0.008));
    }
    draw();
  };
  canvas.onwheel = e => { e.preventDefault(); cam.dist *= Math.exp(e.deltaY * 0.001); draw(); };
  draw();
}
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = a => { const l = Math.hypot(...a); return a.map(x => x / l); };
function mul(a, b) { const r = new Array(16).fill(0);
  for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) for (let k = 0; k < 4; k++) r[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
  return r; }

render();
</script>
</body>
</html>
"""
