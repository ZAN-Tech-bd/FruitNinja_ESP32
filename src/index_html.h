// Auto-loaded by FruitNinja_ESP32.ino via #include "index_html.h"
// Contains the entire game (HTML + CSS + JS) as one PROGMEM string served at "/"

const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>ESP32 Fruit Ninja</title>
<style>
  html,body { margin:0; padding:0; overflow:hidden; background:#0a0e1a; height:100%; touch-action:none;
              font-family:'Segoe UI', system-ui, sans-serif; }
  canvas { display:block; }
  #vignette {
    position:fixed; inset:0; pointer-events:none; z-index:4;
    background:radial-gradient(ellipse at center, rgba(0,0,0,0) 55%, rgba(0,0,0,0.55) 100%);
  }
  #hud {
    position:fixed; top:14px; left:14px; z-index:5; color:#fff;
    background:linear-gradient(#3e2a17,#2a1c10); border:3px solid #6b4423; border-radius:12px;
    padding:8px 16px; box-shadow:0 4px 10px rgba(0,0,0,0.5);
  }
  #hud .score { font-size:26px; font-weight:800; text-shadow:0 0 8px #ffcc00; }
  #hearts { margin-top:4px; font-size:20px; letter-spacing:4px; }
  #hearts .h { color:#ff3355; }
  #hearts .e { color:#3a3a3a; }
  #topbar { position:fixed; top:14px; right:14px; z-index:5; display:flex; gap:8px; }
  #topbar button {
    background:linear-gradient(#ffe066,#ffb700); border:2px solid #a3690a; border-radius:10px;
    padding:9px 14px; font-size:13px; font-weight:700; cursor:pointer; color:#4a2e00;
    box-shadow:0 3px 0 #8a5600;
  }
  #topbar button:active { transform:translateY(2px); box-shadow:none; }
  #status { position:fixed; bottom:8px; left:12px; color:#7fef8f; font-family:monospace; font-size:11px; z-index:5; opacity:0.7; }
  #overlay {
    position:fixed; inset:0; display:flex; flex-direction:column; align-items:center; justify-content:center;
    background:radial-gradient(ellipse at center, rgba(20,20,40,0.85), rgba(0,0,0,0.92));
    color:#fff; text-align:center; z-index:10; padding:20px; box-sizing:border-box;
  }
  #overlay h1 {
    font-size:44px; margin:0 0 4px 0; letter-spacing:2px;
    background:linear-gradient(#fff6cc,#ffcc00); -webkit-background-clip:text; background-clip:text; color:transparent;
    text-shadow:0 0 30px rgba(255,204,0,0.5);
  }
  #overlay p { max-width:420px; line-height:1.5; color:#ddd; font-size:15px; }
  #overlay .highscore { color:#ffcc00; font-weight:700; margin-top:6px; }
  #overlay button.play {
    margin-top:20px; background:linear-gradient(#ff7a5c,#e6432a); color:#fff; border:none; border-radius:14px;
    padding:16px 36px; font-size:19px; font-weight:800; cursor:pointer; box-shadow:0 5px 0 #a12813;
  }
  #overlay button.play:active { transform:translateY(3px); box-shadow:none; }
  .hidden { display:none !important; }
</style>
</head>
<body>

<canvas id="game"></canvas>
<div id="vignette"></div>

<div id="hud">
  <div class="score">Score: <span id="score">0</span></div>
  <div id="hearts"></div>
</div>
<div id="topbar"><button onclick="calibrate()">Calibrate</button></div>
<div id="status">connecting to sensor...</div>

<div id="overlay">
  <h1>FRUIT NINJA</h1>
  <p>Hold the sensor flat, tap <b>Calibrate</b>, then tilt to steer the blade.
     Swipe fast across fruit to slice it. Avoid the bombs — and watch for the
     golden star for a slow-motion bonus!</p>
  <div class="highscore" id="highscoreLine">High Score: 0</div>
  <button class="play" onclick="startGame()">Start Game</button>
</div>

<script>
const canvas = document.getElementById('game');
const ctx = canvas.getContext('2d');
function resize(){ canvas.width = window.innerWidth; canvas.height = window.innerHeight; }
window.addEventListener('resize', resize);
resize();

// ---------------- Sound (synthesized, no files needed) ----------------
let audioCtx = null;
function ensureAudio() { if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)(); }
function tone(freq, dur, type, vol) {
  if (!audioCtx) return;
  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  osc.type = type; osc.frequency.value = freq;
  gain.gain.setValueAtTime(vol, audioCtx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + dur);
  osc.connect(gain); gain.connect(audioCtx.destination);
  osc.start(); osc.stop(audioCtx.currentTime + dur);
}
// Short burst of filtered noise — used for slice "shick" and the bomb blast.
// Synthesized on the fly (no audio files), so it still works fully offline.
function noiseBurst(duration, volume, startFreq, endFreq) {
  if (!audioCtx) return;
  const bufferSize = Math.floor(audioCtx.sampleRate * duration);
  const buffer = audioCtx.createBuffer(1, bufferSize, audioCtx.sampleRate);
  const data = buffer.getChannelData(0);
  for (let i=0;i<bufferSize;i++) data[i] = (Math.random()*2-1) * (1 - i/bufferSize);
  const noise = audioCtx.createBufferSource();
  noise.buffer = buffer;
  const filter = audioCtx.createBiquadFilter();
  filter.type = 'lowpass';
  filter.frequency.setValueAtTime(startFreq || 1200, audioCtx.currentTime);
  filter.frequency.exponentialRampToValueAtTime(endFreq || 100, audioCtx.currentTime + duration);
  const gain = audioCtx.createGain();
  gain.gain.setValueAtTime(volume, audioCtx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + duration);
  noise.connect(filter); filter.connect(gain); gain.connect(audioCtx.destination);
  noise.start();
}

function sndSlice(){
  tone(700 + Math.random()*500, 0.11, 'triangle', 0.10);
  noiseBurst(0.06, 0.05, 3000, 500); // quick "shick" transient on top of the tone
}
function sndBomb(){
  noiseBurst(0.55, 0.4, 1500, 60);    // low rumbling explosion
  tone(90, 0.4, 'sawtooth', 0.22);
  setTimeout(()=>tone(50, 0.3, 'square', 0.16), 50);
}
function sndStar(){ tone(1100,0.14,'sine',0.13); setTimeout(()=>tone(1500,0.16,'sine',0.11),90); }
function sndCombo(){ tone(1000 + Math.random()*200,0.1,'square',0.08); }
function sndLose(){ tone(300,0.5,'sawtooth',0.2); }

// Launch cues — played the instant something is thrown up, so bombs give an audio warning too
function sndLaunchFruit(){
  if (!audioCtx) return;
  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  osc.type = 'sine';
  osc.frequency.setValueAtTime(280, audioCtx.currentTime);
  osc.frequency.exponentialRampToValueAtTime(620, audioCtx.currentTime + 0.13);
  gain.gain.setValueAtTime(0.05, audioCtx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.15);
  osc.connect(gain); gain.connect(audioCtx.destination);
  osc.start(); osc.stop(audioCtx.currentTime + 0.15);
}
function sndLaunchBomb(){
  if (!audioCtx) return;
  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  osc.type = 'square';
  osc.frequency.setValueAtTime(180, audioCtx.currentTime);
  osc.frequency.exponentialRampToValueAtTime(110, audioCtx.currentTime + 0.18);
  gain.gain.setValueAtTime(0.07, audioCtx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.2);
  osc.connect(gain); gain.connect(audioCtx.destination);
  osc.start(); osc.stop(audioCtx.currentTime + 0.2);
}
function sndLaunchStar(){
  tone(900, 0.08, 'sine', 0.06);
  setTimeout(()=>tone(1300, 0.1, 'sine', 0.06), 60);
}

// ---------------- Sensor polling ----------------
let sensor = {ax:0, ay:0, az:1, gx:0, gy:0, gz:0};
let offset = {ax:0, ay:0};

async function pollSensor() {
  try {
    const r = await fetch('/data', {cache:'no-store'});
    const d = await r.json();
    sensor = d;
    document.getElementById('status').textContent = 'sensor OK  ax:'+d.ax.toFixed(2)+' ay:'+d.ay.toFixed(2);
  } catch(e) {
    document.getElementById('status').textContent = 'sensor connection lost, retrying...';
  }
  setTimeout(pollSensor, 20);
}
pollSensor();

function calibrate() {
  offset.ax = sensor.ax;
  offset.ay = sensor.ay;
  document.getElementById('status').textContent = 'calibrated!';
}

// ---------------- High score ----------------
let highScore = parseInt(localStorage.getItem('fn_highscore') || '0');
document.getElementById('highscoreLine').textContent = 'High Score: ' + highScore;

// ---------------- Game state ----------------
let running = false;
let score = 0;
let lives = 3;
let fruits = [];
let particles = [];
let floats = [];
let clouds = [];
let trail = [];
let shake = {t:0};
let timeScale = 1;
let slowmoUntil = 0;
let lastSliceTime = 0;
let comboCount = 0;

// Vector fruit definitions: body/light/dark drive the gradient, juice drives splatter color
const FRUIT_TYPES = [
  {name:'watermelon', body:'#3fae49', light:'#8de07a', dark:'#164f22', juice:'#ff5677', shape:'stripe'},
  {name:'orange',      body:'#ff9f1c', light:'#ffd166', dark:'#c25e00', juice:'#ffa500', shape:'plain'},
  {name:'apple',       body:'#ef2d3d', light:'#ff8a80', dark:'#8e0000', juice:'#ff3b30', shape:'plain'},
  {name:'strawberry',  body:'#ff3d63', light:'#ff8fa3', dark:'#8f0030', juice:'#ff2d55', shape:'dot'},
  {name:'pineapple',   body:'#e6b800', light:'#fff176', dark:'#8a6500', juice:'#ffd700', shape:'cross'},
  {name:'kiwi',        body:'#7cb518', light:'#c6e97a', dark:'#3c5a0a', juice:'#8bc34a', shape:'dot'},
  {name:'lemon',       body:'#f4e04d', light:'#fffbb0', dark:'#b3960a', juice:'#fff176', shape:'plain'}
];

let cursor = {x:0, y:0, vx:0, vy:0, speed:0};

function initClouds() {
  clouds = [];
  for (let i=0;i<6;i++) {
    clouds.push({
      x: Math.random()*canvas.width, y: 40 + Math.random()*(canvas.height*0.4),
      scale: 0.6 + Math.random()*1.2, speed: 0.15 + Math.random()*0.25
    });
  }
}
initClouds();

function resetGame() {
  score = 0; lives = 3; comboCount = 0; timeScale = 1; slowmoUntil = 0;
  fruits = []; particles = []; floats = []; trail = [];
  cursor.x = canvas.width/2; cursor.y = canvas.height/2;
  document.getElementById('score').textContent = score;
  updateHearts();
}

function updateHearts() {
  const el = document.getElementById('hearts');
  el.innerHTML = '';
  for (let i=0;i<3;i++) {
    const s = document.createElement('span');
    s.className = i < lives ? 'h' : 'e';
    s.textContent = '\u25CF '; // solid colored dot, renders identically everywhere
    el.appendChild(s);
  }
}

function startGame() {
  ensureAudio();
  resetGame();
  document.getElementById('overlay').classList.add('hidden');
  running = true;
  lastSpawn = performance.now();
}

function gameOver() {
  running = false;
  sndLose();
  if (score > highScore) {
    highScore = score;
    localStorage.setItem('fn_highscore', highScore);
  }
  document.getElementById('overlay').classList.remove('hidden');
  document.querySelector('#overlay h1').textContent = 'Game Over';
  document.querySelector('#overlay p').innerHTML =
    'Final Score: <b>' + score + '</b><br>Tilt to move the blade, swipe fast to slice.';
  document.getElementById('highscoreLine').textContent = 'High Score: ' + highScore +
    (score >= highScore && score > 0 ? '  NEW RECORD!' : '');
  document.querySelector('#overlay button').textContent = 'Play Again';
}

// ---------------- Spawning ----------------
let lastSpawn = 0;
function spawnFruit() {
  const roll = Math.random();
  let isBomb = false, isStar = false, type = null;
  if (roll < 0.10) { isBomb = true; }
  else if (roll < 0.16) { isStar = true; }
  else { type = FRUIT_TYPES[Math.floor(Math.random()*FRUIT_TYPES.length)]; }

  const x = 60 + Math.random() * (canvas.width - 120);
  const speedBoost = Math.min(score * 0.02, 6);
  const vy = -(14 + Math.random()*5 + speedBoost);
  const vx = (Math.random()-0.5) * 6;

  fruits.push({
    x, y: canvas.height + 30, vx, vy,
    r: 32, rotation: Math.random()*Math.PI*2, rotSpeed: (Math.random()-0.5)*0.15,
    body: type ? type.body : null, light: type ? type.light : null, dark: type ? type.dark : null,
    shape: type ? type.shape : null, color: type ? type.juice : (isBomb ? '#ff8800' : '#ffe066'),
    isBomb, isStar, sliced:false
  });
  if (isBomb) sndLaunchBomb();
  else if (isStar) sndLaunchStar();
  else sndLaunchFruit();

  if (score > 60 && Math.random() < 0.18) {
    const x2 = 60 + Math.random() * (canvas.width - 120);
    const t2 = FRUIT_TYPES[Math.floor(Math.random()*FRUIT_TYPES.length)];
    fruits.push({
      x:x2, y: canvas.height + 30, vx:(Math.random()-0.5)*6, vy:-(13+Math.random()*5+speedBoost),
      r:32, rotation:Math.random()*Math.PI*2, rotSpeed:(Math.random()-0.5)*0.15,
      body:t2.body, light:t2.light, dark:t2.dark, shape:t2.shape, color:t2.juice,
      isBomb:false, isStar:false, sliced:false
    });
    sndLaunchFruit();
  }
}

// ---------------- Effects ----------------
function burst(x, y, color, n) {
  for (let i=0;i<n;i++) {
    particles.push({
      x, y,
      vx:(Math.random()-0.5)*11, vy:(Math.random()-0.5)*11-3,
      life: 26+Math.random()*18, color
    });
  }
  if (particles.length > 220) particles.splice(0, particles.length-220);
}
function floatText(x, y, text, color, size) {
  floats.push({x, y, text, color, size: size||18, life:45, vy:-1.4});
}
function triggerShake(amount) { shake.t = amount; }

// ---------------- Update ----------------
const GRAVITY = 0.45;
const SENSITIVITY = 26;
const DAMPING = 0.85;
const SLICE_SPEED_THRESHOLD = 6;

function update() {
  if (!running) return;
  const now = performance.now();

  if (now > slowmoUntil) timeScale = 1;

  for (const c of clouds) {
    c.x += c.speed;
    if (c.x > canvas.width + 80) c.x = -80;
  }

  const tiltX = sensor.ay - offset.ay;
  const tiltY = sensor.ax - offset.ax;
  cursor.vx = cursor.vx*DAMPING + tiltX * SENSITIVITY;
  cursor.vy = cursor.vy*DAMPING + (-tiltY) * SENSITIVITY;
  cursor.x += cursor.vx;
  cursor.y += cursor.vy;
  cursor.speed = Math.hypot(cursor.vx, cursor.vy);
  cursor.x = Math.max(10, Math.min(canvas.width-10, cursor.x));
  cursor.y = Math.max(10, Math.min(canvas.height-10, cursor.y));

  trail.push({x:cursor.x, y:cursor.y});
  if (trail.length > 10) trail.shift();

  const interval = Math.max(340, 900 - score*4);
  if (now - lastSpawn > interval) { spawnFruit(); lastSpawn = now; }

  for (const f of fruits) {
    if (f.sliced) continue;
    f.vy += GRAVITY * timeScale;
    f.x += f.vx * timeScale;
    f.y += f.vy * timeScale;
    f.rotation += f.rotSpeed * timeScale;

    const d = Math.hypot(cursor.x - f.x, cursor.y - f.y);
    if (d < f.r + 12 && cursor.speed > SLICE_SPEED_THRESHOLD) {
      f.sliced = true;
      burst(f.x, f.y, f.color, f.isBomb ? 26 : 14);

      if (f.isBomb) {
        lives = 0;
        triggerShake(18);
        sndBomb();
        floatText(f.x, f.y-20, 'BOOM!', '#ff5533', 26);
        comboCount = 0;
      } else if (f.isStar) {
        score += 30;
        timeScale = 0.35;
        slowmoUntil = now + 2500;
        sndStar();
        floatText(f.x, f.y-20, '+30 SLOW-MO!', '#ffd700', 20);
        comboCount = 0;
      } else {
        score += 10;
        sndSlice();
        if (now - lastSliceTime < 550) {
          comboCount++;
          if (comboCount >= 2) {
            const bonus = comboCount * 5;
            score += bonus;
            floatText(f.x, f.y-20, 'COMBO x'+comboCount+'! +'+bonus, '#7fffb0', 20);
            sndCombo();
          } else {
            floatText(f.x, f.y-20, '+10', '#ffffff', 16);
          }
        } else {
          comboCount = 1;
          floatText(f.x, f.y-20, '+10', '#ffffff', 16);
        }
        lastSliceTime = now;
      }
      document.getElementById('score').textContent = score;
      updateHearts();
    }

    if (!f.isBomb && !f.isStar && !f.sliced && f.y > canvas.height + 60 && f.vy > 0) {
      f.missed = true;
      lives -= 1;
      comboCount = 0;
      updateHearts();
    }
  }
  fruits = fruits.filter(f => !f.missed && f.y < canvas.height + 200);

  for (const p of particles) { p.vy += GRAVITY*0.4; p.x += p.vx; p.y += p.vy; p.life -= 1; }
  particles = particles.filter(p => p.life > 0);

  for (const t of floats) { t.y += t.vy; t.life -= 1; }
  floats = floats.filter(t => t.life > 0);

  if (shake.t > 0) shake.t -= 1;

  if (lives <= 0) gameOver();
}

// ---------------- Vector drawing (no emoji fonts — always colorful) ----------------
function drawFruitShape(f) {
  const r = f.r;
  ctx.shadowColor = f.light; ctx.shadowBlur = 14;

  const grad = ctx.createRadialGradient(-r*0.35, -r*0.35, r*0.15, 0, 0, r);
  grad.addColorStop(0, f.light);
  grad.addColorStop(0.6, f.body);
  grad.addColorStop(1, f.dark);
  ctx.beginPath();
  ctx.arc(0, 0, r, 0, Math.PI*2);
  ctx.fillStyle = grad;
  ctx.fill();
  ctx.shadowBlur = 0;
  ctx.lineWidth = 2.5;
  ctx.strokeStyle = 'rgba(0,0,0,0.3)';
  ctx.stroke();

  if (f.shape === 'stripe') {
    ctx.strokeStyle = f.dark;
    ctx.lineWidth = 4;
    for (let a=-1; a<=1; a++) {
      ctx.beginPath();
      ctx.moveTo(a*r*0.5, -r*0.9);
      ctx.quadraticCurveTo(a*r*0.75, 0, a*r*0.5, r*0.9);
      ctx.stroke();
    }
  } else if (f.shape === 'dot') {
    ctx.fillStyle = f.dark;
    for (let i=0;i<10;i++) {
      const ang = (i/10)*Math.PI*2;
      const rr = r*0.55;
      ctx.beginPath();
      ctx.arc(Math.cos(ang)*rr, Math.sin(ang)*rr, 2.4, 0, Math.PI*2);
      ctx.fill();
    }
  } else if (f.shape === 'cross') {
    ctx.strokeStyle = f.dark;
    ctx.lineWidth = 2;
    for (let i=-2;i<=2;i++) {
      ctx.beginPath(); ctx.moveTo(-r, i*r*0.3); ctx.lineTo(r, i*r*0.3+r*0.6); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(-r, i*r*0.3+r*0.6); ctx.lineTo(r, i*r*0.3); ctx.stroke();
    }
  }

  // stem + leaf
  ctx.fillStyle = '#3f7d20';
  ctx.beginPath();
  ctx.ellipse(-r*0.15, -r*0.95, r*0.28, r*0.14, -0.5, 0, Math.PI*2);
  ctx.fill();
  ctx.fillStyle = '#6b4423';
  ctx.fillRect(-2, -r*1.08, 4, r*0.22);

  // shine highlight
  ctx.beginPath();
  ctx.ellipse(-r*0.32, -r*0.35, r*0.22, r*0.14, -0.6, 0, Math.PI*2);
  ctx.fillStyle = 'rgba(255,255,255,0.6)';
  ctx.fill();
}

function drawBombShape() {
  const r = 30;
  const grad = ctx.createRadialGradient(-r*0.3, -r*0.3, r*0.1, 0, 0, r);
  grad.addColorStop(0, '#6a6a6a');
  grad.addColorStop(0.6, '#2b2b2b');
  grad.addColorStop(1, '#000000');
  ctx.beginPath();
  ctx.arc(0, 0, r, 0, Math.PI*2);
  ctx.fillStyle = grad;
  ctx.fill();
  ctx.lineWidth = 2.5;
  ctx.strokeStyle = '#ff4444';
  ctx.stroke();

  ctx.strokeStyle = '#c99b52';
  ctx.lineWidth = 4;
  ctx.lineCap = 'round';
  ctx.beginPath();
  ctx.moveTo(0, -r);
  ctx.quadraticCurveTo(r*0.35, -r*1.4, r*0.12, -r*1.7);
  ctx.stroke();

  ctx.fillStyle = '#ffcc33';
  ctx.beginPath(); ctx.arc(r*0.12, -r*1.75, 6, 0, Math.PI*2); ctx.fill();
  ctx.fillStyle = '#ff5500';
  ctx.beginPath(); ctx.arc(r*0.12, -r*1.75, 3, 0, Math.PI*2); ctx.fill();

  ctx.beginPath();
  ctx.ellipse(-r*0.3, -r*0.3, r*0.18, r*0.12, -0.6, 0, Math.PI*2);
  ctx.fillStyle = 'rgba(255,255,255,0.3)';
  ctx.fill();
}

function drawStarShape() {
  const spikes = 5, outerR = 30, innerR = 13;
  ctx.shadowColor = '#ffe066';
  ctx.shadowBlur = 26;
  ctx.beginPath();
  for (let i=0;i<spikes*2;i++) {
    const ang = (i*Math.PI)/spikes - Math.PI/2;
    const rad = i % 2 === 0 ? outerR : innerR;
    const px = Math.cos(ang)*rad, py = Math.sin(ang)*rad;
    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
  }
  ctx.closePath();
  const grad = ctx.createRadialGradient(0,0,0,0,0,outerR);
  grad.addColorStop(0, '#fffbe0');
  grad.addColorStop(1, '#ffcc00');
  ctx.fillStyle = grad;
  ctx.fill();
  ctx.shadowBlur = 0;
  ctx.strokeStyle = '#ffaa00';
  ctx.lineWidth = 2;
  ctx.stroke();
}

function drawBackground() {
  const g = ctx.createLinearGradient(0,0,0,canvas.height);
  g.addColorStop(0, '#1b2a4a');
  g.addColorStop(0.55, '#2c3f6b');
  g.addColorStop(1, '#0d1220');
  ctx.fillStyle = g;
  ctx.fillRect(0,0,canvas.width,canvas.height);

  ctx.fillStyle = 'rgba(255,255,255,0.10)';
  for (const c of clouds) {
    ctx.save();
    ctx.translate(c.x, c.y);
    ctx.scale(c.scale, c.scale);
    ctx.beginPath();
    ctx.ellipse(0,0,40,18,0,0,Math.PI*2);
    ctx.ellipse(28,6,28,14,0,0,Math.PI*2);
    ctx.ellipse(-26,8,26,13,0,0,Math.PI*2);
    ctx.fill();
    ctx.restore();
  }

  ctx.fillStyle = '#0f1830';
  ctx.beginPath();
  ctx.moveTo(0, canvas.height);
  ctx.lineTo(0, canvas.height*0.82);
  for (let x=0;x<=canvas.width;x+=60) {
    ctx.lineTo(x, canvas.height*0.82 - Math.sin(x*0.01)*18);
  }
  ctx.lineTo(canvas.width, canvas.height);
  ctx.closePath();
  ctx.fill();
}

function draw() {
  ctx.save();
  if (shake.t > 0) {
    ctx.translate((Math.random()-0.5)*shake.t, (Math.random()-0.5)*shake.t);
  }

  drawBackground();

  if (trail.length > 1) {
    ctx.strokeStyle = 'rgba(255,255,255,0.9)';
    ctx.shadowColor = '#8ff'; ctx.shadowBlur = 10;
    ctx.lineWidth = 5; ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(trail[0].x, trail[0].y);
    for (let i=1;i<trail.length;i++) ctx.lineTo(trail[i].x, trail[i].y);
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  for (const f of fruits) {
    if (f.sliced) continue;
    ctx.save();
    ctx.beginPath();
    ctx.ellipse(f.x, f.y + f.r*0.9, f.r*0.7, f.r*0.22, 0, 0, Math.PI*2);
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    ctx.fill();
    ctx.translate(f.x, f.y);
    ctx.rotate(f.rotation);
    if (f.isBomb) drawBombShape();
    else if (f.isStar) drawStarShape();
    else drawFruitShape(f);
    ctx.restore();
  }

  for (const p of particles) {
    ctx.fillStyle = p.color;
    ctx.globalAlpha = Math.max(p.life/40, 0);
    ctx.beginPath();
    ctx.arc(p.x, p.y, 4, 0, Math.PI*2);
    ctx.fill();
    ctx.globalAlpha = 1;
  }

  for (const t of floats) {
    ctx.globalAlpha = Math.max(t.life/45, 0);
    ctx.fillStyle = t.color;
    ctx.font = 'bold ' + t.size + 'px sans-serif';
    ctx.fillText(t.text, t.x, t.y);
    ctx.globalAlpha = 1;
  }

  if (running) {
    ctx.beginPath();
    ctx.fillStyle = '#fff';
    ctx.shadowColor = '#8ff'; ctx.shadowBlur = 12;
    ctx.arc(cursor.x, cursor.y, 6, 0, Math.PI*2);
    ctx.fill();
    ctx.shadowBlur = 0;
  }

  ctx.restore();
}

function loop() { update(); draw(); requestAnimationFrame(loop); }
loop();
</script>
</body>
</html>
)HTMLPAGE";