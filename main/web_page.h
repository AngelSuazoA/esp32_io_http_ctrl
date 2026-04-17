#pragma once

static const char index_html[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='utf-8'>"
"<title>ESP32 Signal Verification</title>"
"<style>"
"body{font-family:Arial;margin:20px}"
"fieldset{margin-bottom:15px;padding:10px}"
"input,button{margin:4px}"
".locked{opacity:0.5;pointer-events:none}"
".val{font-weight:bold}"
"</style>"
"</head>"

"<body>"
"<h2>Signal Verification</h2>"

/* ================= MODE ================= */
"<fieldset id='modeBox'>"
"<legend>Mode</legend>"
"<input type='radio' name='mode' value='calc' checked> CALC (compute Ton)<br>"
"<input type='radio' name='mode' value='direct'> DIRECT<br>"
"</fieldset>"

/* ================= DIRECT SUBMODE ================= */
"<fieldset id='directModeBox'>"
"<legend>Direct Mode</legend>"
"<input type='radio' name='direct_mode' value='ton' checked> Ton (s)<br>"
"<input type='radio' name='direct_mode' value='duty'> Duty (%)<br>"
"</fieldset>"

/* ================= INPUTS ================= */
"<fieldset id='paramBox'>"
"<legend>Input Parameters</legend>"
"I: <input id='I'><br>"
"Rs (Ohm): <input id='Rs'><br>"
"Frequency (Hz): <input id='freq'><br>"
"Vpwm (V): <input id='Vpwm'><br>"
"R3 (Ohm): <input id='R3'><br>"
"R5 (Ohm): <input id='R5'><br>"
"Ton (s): <input id='Ton'><br>"
"Duty (%): <input id='Duty'><br>"
"<button onclick='sendParams()'>Send Parameters</button>"
"</fieldset>"

/* ================= PHASE ================= */
"<fieldset id='phaseBox'>"
"<legend>Phase Selection</legend>"
"<button onclick=\"selectPhase('A')\">Phase A</button>"
"<button onclick=\"selectPhase('B')\">Phase B</button>"
"<button onclick=\"selectPhase('C')\">Phase C</button>"
"</fieldset>"

/* ================= TEST ================= */
"<fieldset>"
"<legend>Test Control</legend>"
"<button onclick='startTest()'>TEST START</button>"
"<button onclick='stopTest()'>STOP</button>"
"</fieldset>"

/* ================= STATUS ================= */
"<fieldset>"
"<legend>Status (ESP32 – REAL STATE)</legend>"
"Running: <span id='running' class='val'>-</span><br>"
"Mode: <span id='modeOut' class='val'>-</span><br>"
"Direct Mode: <span id='directModeOut' class='val'>-</span><br>"
"Phase: <span id='phaseOut' class='val'>-</span><br><br>"

"<u>Inputs received by ESP</u><br>"
"I: <span id='I_out'>-</span><br>"
"Rs: <span id='Rs_out'>-</span><br>"
"Frequency: <span id='Freq_out'>-</span><br>"
"Vpwm: <span id='Vpwm_out'>-</span><br>"
"R3: <span id='R3_out'>-</span><br>"
"R5: <span id='R5_out'>-</span><br>"
"Ton (direct): <span id='Ton_direct_out'>-</span><br>"
"Duty (direct): <span id='Duty_direct_out'>-</span><br><br>"

"<u>Calculated / Used by ESP</u><br>"
"Gain: <span id='Gain'>-</span><br>"
"Ts: <span id='Ts'>-</span><br>"
"Ton (used): <span id='Ton_used'>-</span><br>"
"Toff: <span id='Toff'>-</span><br>"
"Duty (used %): <span id='Duty_used'>-</span><br>"
"</fieldset>"

"<script>"

/* ================= SEND PARAMS ================= */
"function sendParams(){"

"const mode = document.querySelector(\"input[name='mode']:checked\").value;"
"const payload = {"
"  mode: mode,"
"  Frequency: parseFloat(document.getElementById('freq').value) || 0"
"};"

"if(mode === 'calc'){"
"  payload.I    = parseFloat(document.getElementById('I').value) || 0;"
"  payload.Rs   = parseFloat(document.getElementById('Rs').value) || 0;"
"  payload.Vpwm = parseFloat(document.getElementById('Vpwm').value) || 0;"
"  payload.R3   = parseFloat(document.getElementById('R3').value) || 0;"
"  payload.R5   = parseFloat(document.getElementById('R5').value) || 0;"
"} else {"
"  const dm = document.querySelector(\"input[name='direct_mode']:checked\").value;"
"  payload.direct_mode = dm;"
"  if(dm === 'ton'){"
"    payload.Ton = parseFloat(document.getElementById('Ton').value) || 0;"
"  } else {"
"    payload.Duty = parseFloat(document.getElementById('Duty').value) || 0;"
"  }"
"}"

"fetch('/params',{"
"  method:'POST',"
"  headers:{'Content-Type':'application/json'},"
"  body:JSON.stringify(payload)"
"});"
"}"

/* ================= PHASE ================= */
"function selectPhase(p){"
"fetch('/phase',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({phase:p})});"
"}"

/* ================= TEST ================= */
"function startTest(){fetch('/test/start',{method:'POST'});}"
"function stopTest(){fetch('/test/stop',{method:'POST'});}"

/* ================= STATE UPDATE ================= */
"function updateState(){"
"fetch('/state').then(r=>r.json()).then(s=>{"

"document.getElementById('running').innerText = s.running ? 'RUNNING' : 'IDLE';"
"document.getElementById('modeOut').innerText = s.mode;"
"document.getElementById('directModeOut').innerText = s.direct_mode;"
"document.getElementById('phaseOut').innerText = s.phase;"

"document.getElementById('I_out').innerText = Number(s.I).toFixed(3);"
"document.getElementById('Rs_out').innerText = Number(s.Rs).toFixed(3);"
"document.getElementById('Freq_out').innerText = Number(s.Frequency).toFixed(2);"
"document.getElementById('Vpwm_out').innerText = Number(s.Vpwm).toFixed(3);"
"document.getElementById('R3_out').innerText = Number(s.R3).toFixed(2);"
"document.getElementById('R5_out').innerText = Number(s.R5).toFixed(2);"

"document.getElementById('Ton_direct_out').innerText = Number(s.Ton_direct).toFixed(6);"
"document.getElementById('Duty_direct_out').innerText = Number(s.Duty_direct).toFixed(2);"

"document.getElementById('Gain').innerText = Number(s.Gain).toFixed(4);"
"document.getElementById('Ts').innerText = Number(s.Ts).toFixed(6);"
"document.getElementById('Ton_used').innerText = Number(s.Ton).toFixed(6);"
"document.getElementById('Toff').innerText = Number(s.Toff).toFixed(6);"
"document.getElementById('Duty_used').innerText = Number(s.Duty_used).toFixed(2);"

"const lock = s.running;"
"['modeBox','directModeBox','paramBox','phaseBox'].forEach(id=>{"
"  document.getElementById(id).classList.toggle('locked',lock);"
"});"

"});"
"}"

"setInterval(updateState,500);"

"</script>"
"</body>"
"</html>";