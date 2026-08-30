// Auto-loaded by SpaceWar_ESP32.ino via #include "index_html.h"
// Contains the entire game (HTML + CSS + JS) as one PROGMEM string served at "/"

const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>ESP32 Space War</title>
<style>
  html,body { margin:0; padding:0; overflow:hidden; background:#02040a; height:100%; touch-action:none;
              font-family:'Segoe UI', system-ui, sans-serif; }
  canvas { display:block; }
  #vignette {
    position:fixed; inset:0; pointer-events:none; z-index:4;
    background:radial-gradient(ellipse at center, rgba(0,0,0,0) 55%, rgba(0,0,0,0.6) 100%);
  }
  #hud {
    position:fixed; top:14px; left:14px; z-index:5; color:#fff;
    background:linear-gradient(#0d2038,#081326); border:3px solid #2fa6ff; border-radius:12px;
    padding:8px 16px; box-shadow:0 4px 14px rgba(0,140,255,0.35);
  }
  #hud .score { font-size:26px; font-weight:800; text-shadow:0 0 8px #2fd0ff; }
  #hearts { margin-top:4px; font-size:20px; letter-spacing:4px; }
  #hearts .h { color:#2fe0a0; }
  #hearts .e { color:#3a3a3a; }
  #topbar { position:fixed; top:14px; right:14px; z-index:5; display:flex; gap:8px; }
  #topbar button {
    background:linear-gradient(#5ad4ff,#0d8fdb); border:2px solid #0a5c8f; border-radius:10px;
    padding:9px 14px; font-size:13px; font-weight:700; cursor:pointer; color:#03202f;
    box-shadow:0 3px 0 #094768;
  }
  #topbar button:active { transform:translateY(2px); box-shadow:none; }
  #status { position:fixed; bottom:8px; left:12px; color:#7fefc0; font-family:monospace; font-size:11px; z-index:5; opacity:0.7; }
  #overlay {
    position:fixed; inset:0; display:flex; flex-direction:column; align-items:center; justify-content:center;
    background:radial-gradient(ellipse at center, rgba(10,20,45,0.88), rgba(0,0,2,0.94));
    color:#fff; text-align:center; z-index:10; padding:20px; box-sizing:border-box;
  }
  #overlay h1 {
    font-size:44px; margin:0 0 4px 0; letter-spacing:3px;
    background:linear-gradient(#eafcff,#2fd0ff); -webkit-background-clip:text; background-clip:text; color:transparent;
    text-shadow:0 0 30px rgba(47,208,255,0.55);
  }
  #overlay p { max-width:440px; line-height:1.5; color:#cfe8f5; font-size:15px; }
  #overlay .highscore { color:#2fd0ff; font-weight:700; margin-top:6px; }
  #overlay button.play {
    margin-top:20px; background:linear-gradient(#ff6a4d,#c22a12); color:#fff; border:none; border-radius:14px;
    padding:16px 36px; font-size:19px; font-weight:800; cursor:pointer; box-shadow:0 5px 0 #7a1a0a;
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
  <h1>SPACE WAR</h1>
  <p>Hold the sensor flat, tap <b>Calibrate</b>, then tilt left/right to slide your
     fighter along the defense line. Your guns fire automatically — dodge enemy fire,
     avoid ramming mines, and shoot them from a distance instead. Fly through the
     blue energy core for an overdrive boost!</p>
  <div class="highscore" id="highscoreLine">High Score: 0</div>
  <button class="play" onclick="startGame()">Launch</button>
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
// Short burst of filtered noise — used for explosions and impacts.
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

function sndFire(){ tone(920 + Math.random()*80, 0.045, 'square', 0.035); }
function sndHit(){
  tone(500 + Math.random()*400, 0.09, 'triangle', 0.10);
  noiseBurst(0.05, 0.05, 2600, 500);
}
function sndDefuse(){ tone(1400, 0.08, 'square', 0.09); setTimeout(()=>tone(700,0.1,'square',0.07),60); }
function sndExplosion(){
  noiseBurst(0.55, 0.4, 1500, 60);
  tone(90, 0.4, 'sawtooth', 0.22);
  setTimeout(()=>tone(50, 0.3, 'square', 0.16), 50);
}
function sndPowerUp(){ tone(1100,0.14,'sine',0.13); setTimeout(()=>tone(1500,0.16,'sine',0.11),90); }
function sndCombo(){ tone(1000 + Math.random()*200,0.1,'square',0.08); }
function sndLose(){ tone(280,0.55,'sawtooth',0.22); }
function sndEnemyHit(){ tone(200,0.12,'sawtooth',0.09); }

// Spawn cues — played the instant a threat appears, so you get an audio warning too
function sndSpawnEnemy(){
  if (!audioCtx) return;
  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  osc.type = 'sine';
  osc.frequency.setValueAtTime(620, audioCtx.currentTime);
  osc.frequency.exponentialRampToValueAtTime(280, audioCtx.currentTime + 0.13);
  gain.gain.setValueAtTime(0.05, audioCtx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.15);
  osc.connect(gain); gain.connect(audioCtx.destination);
  osc.start(); osc.stop(audioCtx.currentTime + 0.15);
}
function sndSpawnMine(){
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
function sndSpawnOrb(){
  tone(900, 0.08, 'sine', 0.06);
  setTimeout(()=>tone(1300, 0.1, 'sine', 0.06), 60);
}

// ---------------- Sensor polling ----------------
let sensor = {ax:0, ay:0, az:1, gx:0, gy:0, gz:0};
let offset = {ax:0, ay:0};
let smooth = {ax:0, ay:0};

// ---- Tilt mapping / feel config ----
// If the ship still moves the WRONG axis (left/right tilt doesn't move it),
// flip SWAP_AXES to true.
// If it moves left/right but backwards, flip INVERT_X.
// Tune these by testing after each flash — sensor mounting orientation can't
// be verified from code alone.
const SWAP_AXES   = false;
const INVERT_X    = true    ;
const INVERT_Y    = false;

const SENSITIVITY = 13;   // was 26 — lower = calmer ship
const DAMPING     = 0.80; // was 0.85 — lower = less overshoot/floatiness
const SMOOTHING   = 0.25; // 0-1, exponential smoothing on raw sensor noise; lower = smoother but laggier
const MAX_SPEED   = 16;   // px/frame cap so the ship can't rocket across the screen
const SHIP_BOTTOM_MARGIN = 90; // ship stays locked this many px above the bottom edge

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
  // Average ~400ms of samples instead of a single reading, so a hand-shake
  // while tapping this button doesn't get baked in as a permanent bias.
  document.getElementById('status').textContent = 'calibrating... hold still';
  const samples = [];
  const t0 = performance.now();
  (function collect(){
    samples.push({ax: sensor.ax, ay: sensor.ay});
    if (performance.now() - t0 < 400) {
      requestAnimationFrame(collect);
    } else {
      offset.ax = samples.reduce((s,v)=>s+v.ax,0)/samples.length;
      offset.ay = samples.reduce((s,v)=>s+v.ay,0)/samples.length;
      smooth.ax = offset.ax;
      smooth.ay = offset.ay;
      document.getElementById('status').textContent = 'calibrated!';
    }
  })();
}

// ---------------- High score ----------------
let highScore = parseInt(localStorage.getItem('sw_highscore') || '0');
document.getElementById('highscoreLine').textContent = 'High Score: ' + highScore;

// ---------------- Game state ----------------
let running = false;
let score = 0;
let lives = 3;
let enemies = [];       // enemy fighters, mines, and power orbs all live here
let bullets = [];       // player lasers, firing upward
let enemyBullets = [];  // enemy fire, aimed down at the player
let particles = [];
let floats = [];
let stars = [];         // background starfield
let nebulae = [];       // slow drifting background nebula clouds
let trail = [];
let shake = {t:0};
let timeScale = 1;
let slowmoUntil = 0;
let overdriveUntil = 0;
let lastHitTime = 0;
let comboCount = 0;

// Vector enemy ship definitions: body/light/dark drive the gradient, juice drives the explosion color
const ENEMY_TYPES = [
  {name:'scout',       body:'#ff3d4d', light:'#ff9aa3', dark:'#7a0010', juice:'#ff5677', shape:'fighter', score:10, r:26},
  {name:'interceptor', body:'#ff9f1c', light:'#ffd166', dark:'#8a4f00', juice:'#ffa500', shape:'wide',    score:10, r:28},
  {name:'saucer',      body:'#3fae6a', light:'#8de0b0', dark:'#0f4f2a', juice:'#5cffb0', shape:'saucer',  score:15, r:28},
  {name:'drone',       body:'#3fc7ff', light:'#b3ecff', dark:'#0a5f8a', juice:'#5cd6ff', shape:'diamond', score:10, r:22},
  {name:'cruiser',     body:'#b04dff', light:'#e0b3ff', dark:'#4a0f8a', juice:'#c98bff', shape:'hex',     score:22, r:36, shootsBack:true}
];

let ship = {x:0, y:0, vx:0, vy:0, speed:0, bank:0};

function initStars() {
  stars = [];
  for (let i=0;i<90;i++) {
    stars.push({x:Math.random()*canvas.width, y:Math.random()*canvas.height,
                r:Math.random()*1.6+0.4, tw:Math.random()*Math.PI*2, speed:0.3+Math.random()*1.4});
  }
  nebulae = [];
  for (let i=0;i<5;i++) {
    nebulae.push({
      x: Math.random()*canvas.width, y: 40 + Math.random()*(canvas.height*0.6),
      scale: 0.8 + Math.random()*1.6, speed: 0.1 + Math.random()*0.2,
      hue: Math.random() < 0.5 ? '80,140,255' : '150,80,255'
    });
  }
}
initStars();

function resetGame() {
  score = 0; lives = 3; comboCount = 0; timeScale = 1; slowmoUntil = 0; overdriveUntil = 0;
  enemies = []; bullets = []; enemyBullets = []; particles = []; floats = []; trail = [];
  ship.x = canvas.width/2; ship.y = canvas.height - SHIP_BOTTOM_MARGIN; ship.vx = 0; ship.vy = 0; ship.bank = 0;
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
  lastFire = performance.now();
}

function gameOver() {
  running = false;
  sndLose();
  if (score > highScore) {
    highScore = score;
    localStorage.setItem('sw_highscore', highScore);
  }
  document.getElementById('overlay').classList.remove('hidden');
  document.querySelector('#overlay h1').textContent = 'Ship Destroyed';
  document.querySelector('#overlay p').innerHTML =
    'Final Score: <b>' + score + '</b><br>Tilt left/right to steer, dodge fire, and shoot mines before they get close.';
  document.getElementById('highscoreLine').textContent = 'High Score: ' + highScore +
    (score >= highScore && score > 0 ? '  NEW RECORD!' : '');
  document.querySelector('#overlay button').textContent = 'Launch Again';
}

// ---------------- Spawning ----------------
let lastSpawn = 0;
let lastFire = 0;

function spawnWave() {
  const roll = Math.random();
  let isMine = false, isOrb = false, type = null;
  if (roll < 0.09) { isMine = true; }
  else if (roll < 0.15) { isOrb = true; }
  else { type = ENEMY_TYPES[Math.floor(Math.random()*ENEMY_TYPES.length)]; }

  const x = 60 + Math.random() * (canvas.width - 120);
  const speedBoost = Math.min(score * 0.02, 6);

  if (isMine) {
    enemies.push({
      x, y: -30, vx:(Math.random()-0.5)*1.5, vy: 2.2 + Math.random()*0.8,
      r: 26, rotation: Math.random()*Math.PI*2, rotSpeed: (Math.random()-0.5)*0.05,
      isMine:true, isOrb:false, dead:false
    });
    sndSpawnMine();
  } else if (isOrb) {
    enemies.push({
      x, y: -30, vx:(Math.random()-0.5)*1.2, vy: 2.4 + Math.random()*0.6,
      r: 24, rotation: 0, rotSpeed: 0.06,
      isMine:false, isOrb:true, dead:false
    });
    sndSpawnOrb();
  } else {
    const vy = 3 + Math.random()*2 + speedBoost*0.5;
    const vx = (Math.random()-0.5) * 3;
    enemies.push({
      x, y: -30, vx, vy,
      r: type.r, rotation: 0, wobble: Math.random()*Math.PI*2,
      body:type.body, light:type.light, dark:type.dark, shape:type.shape,
      color:type.juice, scoreValue:type.score, shootsBack: !!type.shootsBack,
      lastShot: performance.now() + Math.random()*600,
      isMine:false, isOrb:false, dead:false
    });
    sndSpawnEnemy();

    if (score > 60 && Math.random() < 0.18) {
      const x2 = 60 + Math.random() * (canvas.width - 120);
      const t2 = ENEMY_TYPES[Math.floor(Math.random()*ENEMY_TYPES.length)];
      enemies.push({
        x:x2, y:-30, vx:(Math.random()-0.5)*3, vy: 3+Math.random()*2+speedBoost*0.5,
        r:t2.r, rotation:0, wobble: Math.random()*Math.PI*2,
        body:t2.body, light:t2.light, dark:t2.dark, shape:t2.shape,
        color:t2.juice, scoreValue:t2.score, shootsBack: !!t2.shootsBack,
        lastShot: performance.now() + Math.random()*600,
        isMine:false, isOrb:false, dead:false
      });
      sndSpawnEnemy();
    }
  }
}

function firePlayerBullet() {
  bullets.push({x: ship.x, y: ship.y - 22, vy: -17});
  bullets.push({x: ship.x - 10, y: ship.y - 12, vy: -17});
  bullets.push({x: ship.x + 10, y: ship.y - 12, vy: -17});
  sndFire();
}

// ---------------- Effects ----------------
function burst(x, y, color, n) {
  for (let i=0;i<n;i++) {
    particles.push({
      x, y,
      vx:(Math.random()-0.5)*11, vy:(Math.random()-0.5)*11-2,
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
const SHIP_HIT_RADIUS = 20;

function update() {
  if (!running) return;
  const now = performance.now();

  if (now > slowmoUntil) timeScale = 1;
  const overdriveActive = now < overdriveUntil;

  for (const s of stars) {
    s.y += s.speed * timeScale;
    if (s.y > canvas.height) { s.y = 0; s.x = Math.random()*canvas.width; }
  }
  for (const nb of nebulae) {
    nb.y += nb.speed * timeScale;
    if (nb.y > canvas.height + 100) nb.y = -100;
  }

  // Smooth the raw sensor signal first so single noisy readings don't jerk the ship.
  smooth.ax += (sensor.ax - smooth.ax) * SMOOTHING;
  smooth.ay += (sensor.ay - smooth.ay) * SMOOTHING;

  let rawX = SWAP_AXES ? (smooth.ay - offset.ay) : (smooth.ax - offset.ax);
  if (INVERT_X) rawX = -rawX;

  ship.vx = ship.vx*DAMPING + rawX * SENSITIVITY;
  ship.vx = Math.max(-MAX_SPEED, Math.min(MAX_SPEED, ship.vx));

  ship.x += ship.vx;
  ship.speed = Math.abs(ship.vx);
  ship.x = Math.max(10, Math.min(canvas.width-10, ship.x));
  ship.y = canvas.height - SHIP_BOTTOM_MARGIN; // locked to bottom lane every frame — also handles window resize
  ship.bank = ship.bank*0.85 + Math.max(-1, Math.min(1, ship.vx/10))*0.15;

  trail.push({x:ship.x, y:ship.y});
  if (trail.length > 10) trail.shift();

  // ---- Spawning & firing ----
  const interval = Math.max(360, 950 - score*4);
  if (now - lastSpawn > interval) { spawnWave(); lastSpawn = now; }

  const fireInterval = overdriveActive ? 90 : 170;
  if (now - lastFire > fireInterval) { firePlayerBullet(); lastFire = now; }

  // ---- Player bullets ----
  for (const b of bullets) { b.y += b.vy * timeScale; }
  bullets = bullets.filter(b => b.y > -20);

  // ---- Enemy bullets ----
  for (const eb of enemyBullets) { eb.y += eb.vy * timeScale; }

  // ---- Enemies / mines / orbs ----
  for (const e of enemies) {
    if (e.dead) continue;
    e.x += e.vx * timeScale;
    e.y += e.vy * timeScale;
    if (e.isMine || e.isOrb) {
      e.rotation += e.rotSpeed * timeScale;
    } else {
      e.wobble += 0.05 * timeScale;
      e.x += Math.sin(e.wobble) * 0.6;
      e.vy += 0.012 * timeScale; // slow dive acceleration, ramps the tension

      if (e.shootsBack && now - e.lastShot > 1300 && e.y > 20 && e.y < canvas.height*0.7) {
        enemyBullets.push({x:e.x, y:e.y+e.r, vy: 6.5});
        e.lastShot = now;
      }
    }

    // player bullets vs this enemy
    if (!e.isOrb) {
      for (const b of bullets) {
        if (b.hit) continue;
        const d = Math.hypot(b.x - e.x, b.y - e.y);
        if (d < e.r) {
          b.hit = true;
          e.dead = true;
          if (e.isMine) {
            burst(e.x, e.y, '#ffaa55', 18);
            floatText(e.x, e.y-20, 'DEFUSED +15', '#ffcc66', 18);
            score += 15;
            sndDefuse();
          } else {
            burst(e.x, e.y, e.color, 14);
            sndHit();
            let gained = e.scoreValue;
            if (now - lastHitTime < 550) {
              comboCount++;
              if (comboCount >= 2) {
                const bonus = comboCount * 5;
                gained += bonus;
                floatText(e.x, e.y-20, 'COMBO x'+comboCount+'! +'+gained, '#7fffb0', 20);
                sndCombo();
              } else {
                floatText(e.x, e.y-20, '+'+gained, '#ffffff', 16);
              }
            } else {
              comboCount = 1;
              floatText(e.x, e.y-20, '+'+gained, '#ffffff', 16);
            }
            score += gained;
            lastHitTime = now;
          }
          document.getElementById('score').textContent = score;
          break;
        }
      }
    }

    if (e.dead) continue;

    // ship vs this enemy (direct contact)
    const dShip = Math.hypot(ship.x - e.x, ship.y - e.y);
    if (dShip < e.r + SHIP_HIT_RADIUS) {
      if (e.isMine) {
        e.dead = true;
        lives = 0;
        triggerShake(18);
        sndExplosion();
        burst(e.x, e.y, '#ff5533', 26);
        floatText(e.x, e.y-20, 'BOOM!', '#ff5533', 26);
        comboCount = 0;
      } else if (e.isOrb) {
        e.dead = true;
        score += 30;
        timeScale = 0.35;
        slowmoUntil = now + 2500;
        overdriveUntil = now + 2500;
        sndPowerUp();
        burst(e.x, e.y, '#5cd6ff', 16);
        floatText(e.x, e.y-20, '+30 OVERDRIVE!', '#5cd6ff', 20);
        comboCount = 0;
      } else {
        e.dead = true;
        lives -= 1;
        triggerShake(10);
        sndEnemyHit();
        burst(e.x, e.y, e.color, 16);
        floatText(e.x, e.y-20, 'HULL HIT', '#ff8866', 18);
        comboCount = 0;
      }
      document.getElementById('score').textContent = score;
      updateHearts();
    }
  }

  // enemies (and mines) that slip past the player = a breach in our defenses
  for (const e of enemies) {
    if (!e.dead && !e.isOrb && e.y > canvas.height + 60) {
      e.dead = true;
      if (!e.isMine) { lives -= 1; comboCount = 0; updateHearts(); }
    }
  }
  enemies = enemies.filter(e => !e.dead && e.y < canvas.height + 200);

  // enemy bullets vs ship
  for (const eb of enemyBullets) {
    if (eb.hit) continue;
    const d = Math.hypot(ship.x - eb.x, ship.y - eb.y);
    if (d < SHIP_HIT_RADIUS) {
      eb.hit = true;
      lives -= 1;
      comboCount = 0;
      triggerShake(8);
      sndEnemyHit();
      burst(eb.x, eb.y, '#ffaa55', 10);
      updateHearts();
    }
  }
  enemyBullets = enemyBullets.filter(eb => !eb.hit && eb.y < canvas.height + 30);

  for (const p of particles) { p.vy += 0.18*timeScale; p.x += p.vx; p.y += p.vy; p.life -= 1; }
  particles = particles.filter(p => p.life > 0);

  for (const t of floats) { t.y += t.vy; t.life -= 1; }
  floats = floats.filter(t => t.life > 0);

  if (shake.t > 0) shake.t -= 1;

  if (lives <= 0) gameOver();
}

// ---------------- Vector drawing (no emoji fonts — always crisp at any size) ----------------
function drawShipShape(bank) {
  const r = 22;
  ctx.save();
  ctx.rotate(bank * 0.35);

  // engine flame
  const flameLen = 14 + Math.random()*8;
  const fg = ctx.createLinearGradient(0, r*0.6, 0, r*0.6+flameLen);
  fg.addColorStop(0, 'rgba(120,200,255,0.9)');
  fg.addColorStop(1, 'rgba(120,200,255,0)');
  ctx.fillStyle = fg;
  ctx.beginPath();
  ctx.moveTo(-6, r*0.55);
  ctx.lineTo(0, r*0.55+flameLen);
  ctx.lineTo(6, r*0.55);
  ctx.closePath();
  ctx.fill();

  // hull
  ctx.shadowColor = '#5cd6ff'; ctx.shadowBlur = 12;
  const grad = ctx.createLinearGradient(0, -r, 0, r*0.7);
  grad.addColorStop(0, '#eafcff');
  grad.addColorStop(0.5, '#5cd6ff');
  grad.addColorStop(1, '#0a5c8f');
  ctx.beginPath();
  ctx.moveTo(0, -r);
  ctx.lineTo(r*0.6, r*0.55);
  ctx.lineTo(0, r*0.25);
  ctx.lineTo(-r*0.6, r*0.55);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();
  ctx.shadowBlur = 0;
  ctx.lineWidth = 2;
  ctx.strokeStyle = 'rgba(255,255,255,0.5)';
  ctx.stroke();

  // wings
  ctx.fillStyle = '#1a7fc9';
  ctx.beginPath();
  ctx.moveTo(0, -r*0.1); ctx.lineTo(r*1.05, r*0.35); ctx.lineTo(r*0.55, r*0.55); ctx.closePath();
  ctx.fill();
  ctx.beginPath();
  ctx.moveTo(0, -r*0.1); ctx.lineTo(-r*1.05, r*0.35); ctx.lineTo(-r*0.55, r*0.55); ctx.closePath();
  ctx.fill();

  // cockpit
  ctx.fillStyle = '#e0faff';
  ctx.beginPath();
  ctx.ellipse(0, -r*0.25, r*0.18, r*0.32, 0, 0, Math.PI*2);
  ctx.fill();

  ctx.restore();
}

function drawEnemyShape(e) {
  const r = e.r;
  ctx.shadowColor = e.light; ctx.shadowBlur = 12;
  const grad = ctx.createLinearGradient(0, -r, 0, r);
  grad.addColorStop(0, e.light);
  grad.addColorStop(0.55, e.body);
  grad.addColorStop(1, e.dark);
  ctx.fillStyle = grad;
  ctx.lineWidth = 2.2;
  ctx.strokeStyle = 'rgba(0,0,0,0.35)';

  ctx.beginPath();
  if (e.shape === 'fighter') {
    ctx.moveTo(0, -r); ctx.lineTo(r*0.75, r*0.7); ctx.lineTo(0, r*0.3); ctx.lineTo(-r*0.75, r*0.7);
  } else if (e.shape === 'wide') {
    ctx.moveTo(0, -r*0.8); ctx.lineTo(r, r*0.6); ctx.lineTo(0, r*0.15); ctx.lineTo(-r, r*0.6);
  } else if (e.shape === 'diamond') {
    ctx.moveTo(0, -r); ctx.lineTo(r*0.7, 0); ctx.lineTo(0, r); ctx.lineTo(-r*0.7, 0);
  } else if (e.shape === 'hex') {
    for (let i=0;i<6;i++) {
      const ang = Math.PI/6 + i*Math.PI/3;
      const px = Math.cos(ang)*r, py = Math.sin(ang)*r;
      if (i===0) ctx.moveTo(px,py); else ctx.lineTo(px,py);
    }
  } else if (e.shape === 'saucer') {
    ctx.ellipse(0, 0, r, r*0.5, 0, 0, Math.PI*2);
  }
  ctx.closePath();
  ctx.fill();
  ctx.stroke();

  if (e.shape === 'saucer') {
    ctx.fillStyle = 'rgba(255,255,255,0.55)';
    ctx.beginPath();
    ctx.ellipse(0, -r*0.15, r*0.45, r*0.28, 0, 0, Math.PI*2);
    ctx.fill();
  }

  // cockpit glow, common accent across all fighter types
  ctx.fillStyle = 'rgba(255,255,255,0.5)';
  ctx.beginPath();
  ctx.arc(0, -r*0.1, r*0.16, 0, Math.PI*2);
  ctx.fill();
  ctx.shadowBlur = 0;
}

function drawMineShape(e) {
  const r = e.r;
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

  // spikes
  ctx.strokeStyle = '#555';
  ctx.lineWidth = 3;
  for (let i=0;i<8;i++) {
    const ang = (i/8)*Math.PI*2;
    ctx.beginPath();
    ctx.moveTo(Math.cos(ang)*r, Math.sin(ang)*r);
    ctx.lineTo(Math.cos(ang)*r*1.35, Math.sin(ang)*r*1.35);
    ctx.stroke();
  }

  ctx.fillStyle = Math.sin(performance.now()*0.01) > 0 ? '#ff3333' : '#661111';
  ctx.beginPath(); ctx.arc(0, 0, 5, 0, Math.PI*2); ctx.fill();

  ctx.beginPath();
  ctx.ellipse(-r*0.3, -r*0.3, r*0.18, r*0.12, -0.6, 0, Math.PI*2);
  ctx.fillStyle = 'rgba(255,255,255,0.25)';
  ctx.fill();
}

function drawOrbShape(e) {
  const r = e.r;
  ctx.shadowColor = '#5cd6ff';
  ctx.shadowBlur = 26;
  const grad = ctx.createRadialGradient(0,0,0,0,0,r);
  grad.addColorStop(0, '#ffffff');
  grad.addColorStop(0.5, '#5cd6ff');
  grad.addColorStop(1, '#0a5c8f');
  ctx.beginPath();
  ctx.arc(0,0,r*0.65,0,Math.PI*2);
  ctx.fillStyle = grad;
  ctx.fill();
  ctx.shadowBlur = 0;

  ctx.strokeStyle = 'rgba(180,230,255,0.8)';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  for (let i=0;i<6;i++) {
    const ang = e.rotation + i*Math.PI/3;
    const px = Math.cos(ang)*r, py = Math.sin(ang)*r;
    if (i===0) ctx.moveTo(px,py); else ctx.lineTo(px,py);
  }
  ctx.closePath();
  ctx.stroke();
}

function drawBackground() {
  const g = ctx.createLinearGradient(0,0,0,canvas.height);
  g.addColorStop(0, '#04081c');
  g.addColorStop(0.55, '#0a1233');
  g.addColorStop(1, '#02030a');
  ctx.fillStyle = g;
  ctx.fillRect(0,0,canvas.width,canvas.height);

  for (const nb of nebulae) {
    ctx.save();
    ctx.translate(nb.x, nb.y);
    ctx.scale(nb.scale, nb.scale);
    ctx.fillStyle = `rgba(${nb.hue},0.10)`;
    ctx.beginPath();
    ctx.ellipse(0,0,50,26,0,0,Math.PI*2);
    ctx.ellipse(34,8,34,18,0,0,Math.PI*2);
    ctx.ellipse(-32,10,30,16,0,0,Math.PI*2);
    ctx.fill();
    ctx.restore();
  }

  for (const s of stars) {
    const tw = 0.5 + 0.5*Math.sin(performance.now()*0.003 + s.tw);
    ctx.globalAlpha = 0.4 + tw*0.6;
    ctx.fillStyle = '#ffffff';
    ctx.beginPath();
    ctx.arc(s.x, s.y, s.r, 0, Math.PI*2);
    ctx.fill();
  }
  ctx.globalAlpha = 1;
}

function draw() {
  ctx.save();
  if (shake.t > 0) {
    ctx.translate((Math.random()-0.5)*shake.t, (Math.random()-0.5)*shake.t);
  }

  drawBackground();

  if (trail.length > 1) {
    ctx.strokeStyle = 'rgba(120,200,255,0.7)';
    ctx.shadowColor = '#5cd6ff'; ctx.shadowBlur = 8;
    ctx.lineWidth = 4; ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(trail[0].x, trail[0].y);
    for (let i=1;i<trail.length;i++) ctx.lineTo(trail[i].x, trail[i].y);
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  // player lasers
  ctx.strokeStyle = '#ffe066';
  ctx.shadowColor = '#ffe066'; ctx.shadowBlur = 8;
  ctx.lineWidth = 3; ctx.lineCap = 'round';
  for (const b of bullets) {
    if (b.hit) continue;
    ctx.beginPath();
    ctx.moveTo(b.x, b.y);
    ctx.lineTo(b.x, b.y + 14);
    ctx.stroke();
  }
  ctx.shadowBlur = 0;

  // enemy fire
  ctx.strokeStyle = '#ff5577';
  ctx.shadowColor = '#ff5577'; ctx.shadowBlur = 8;
  for (const eb of enemyBullets) {
    if (eb.hit) continue;
    ctx.beginPath();
    ctx.moveTo(eb.x, eb.y);
    ctx.lineTo(eb.x, eb.y - 12);
    ctx.stroke();
  }
  ctx.shadowBlur = 0;

  for (const e of enemies) {
    if (e.dead) continue;
    ctx.save();
    ctx.beginPath();
    ctx.ellipse(e.x, e.y + e.r*0.9, e.r*0.7, e.r*0.22, 0, 0, Math.PI*2);
    ctx.fillStyle = 'rgba(0,0,0,0.3)';
    ctx.fill();
    ctx.translate(e.x, e.y);
    ctx.rotate(e.rotation || 0);
    if (e.isMine) drawMineShape(e);
    else if (e.isOrb) drawOrbShape(e);
    else drawEnemyShape(e);
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
    ctx.save();
    ctx.translate(ship.x, ship.y);
    drawShipShape(ship.bank);
    ctx.restore();
  }

  ctx.restore();
}

function loop() { update(); draw(); requestAnimationFrame(loop); }
loop();
</script>
</body>
</html>
)HTMLPAGE";