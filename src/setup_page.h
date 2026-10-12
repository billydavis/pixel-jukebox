#pragma once

#include <Arduino.h>

// Pages served by the Wi-Fi setup hotspot. They are plain HTML forms with no
// requests from script, so they work in the cut-down browsers phones use for
// sign-in pages. {{...}} markers are filled in by wifi_setup.cpp.

#define SETUP_STYLE \
  "<style>" \
  ":root{--bg:oklch(0.22 0.04 270);--panel:oklch(0.28 0.05 270);--edge:oklch(0.15 0.03 270);" \
  "--ink:oklch(0.94 0.015 270);--dim:oklch(0.74 0.04 270);--accent:oklch(0.84 0.13 170);" \
  "--accent-ink:oklch(0.27 0.05 180);--held:oklch(0.58 0.2 355);" \
  "--mono:ui-monospace,\"Cascadia Mono\",\"SF Mono\",Menlo,Consolas,monospace}" \
  "*{box-sizing:border-box}" \
  "body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.45 system-ui,-apple-system,\"Segoe UI\",sans-serif;padding:24px 18px 32px}" \
  "main{max-width:420px;margin:0 auto;display:grid;gap:18px}" \
  "h1{margin:0;font:700 14px/1 var(--mono);letter-spacing:.28em;text-transform:uppercase;color:var(--accent)}" \
  "h2{margin:0;font-size:24px;line-height:1.15}" \
  "p{margin:0}" \
  ".note{font:13px/1.5 var(--mono);color:var(--dim)}" \
  ".alert{padding:12px 14px;background:var(--held);font-weight:700}" \
  "form{display:grid;gap:10px}" \
  "label,.cap{font:13px/1 var(--mono);letter-spacing:.14em;text-transform:uppercase;color:var(--dim);margin-top:10px}" \
  ".nets{display:grid;gap:10px}" \
  "button{font:inherit;color:var(--ink);background:var(--panel);border:0;border-radius:0;cursor:pointer;" \
  "min-height:52px;padding:8px 14px;box-shadow:4px 4px 0 var(--edge)}" \
  "button:active{transform:translate(4px,4px);box-shadow:none}" \
  ".net{display:flex;justify-content:space-between;align-items:center;gap:12px;text-align:left}" \
  ".net span{font-weight:700;overflow-wrap:anywhere}" \
  ".net small{font:12px/1.2 var(--mono);color:var(--dim);white-space:nowrap}" \
  ".net.on{outline:3px solid var(--accent);outline-offset:-3px}" \
  "input[type=text],input[type=password]{width:100%;min-height:52px;padding:0 14px;border:0;border-radius:0;" \
  "font:17px system-ui,-apple-system,\"Segoe UI\",sans-serif;color:var(--ink);background:var(--edge)}" \
  ".show{display:flex;align-items:center;gap:10px;min-height:44px;margin:0;text-transform:none;letter-spacing:0;font:15px system-ui,sans-serif;color:var(--ink)}" \
  ".show input{width:22px;height:22px;accent-color:var(--accent)}" \
  ".go{margin-top:8px;background:var(--accent);color:var(--accent-ink);font:700 15px/1 var(--mono);letter-spacing:.14em;text-transform:uppercase}" \
  "a{color:var(--accent)}" \
  ":focus-visible{outline:3px solid var(--accent);outline-offset:3px}" \
  "</style>"

#define SETUP_HEAD(title) \
  "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">" \
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">" \
  "<title>" title "</title>" SETUP_STYLE "</head><body><main><h1>Pixel Jukebox</h1>"

static const char SETUP_PAGE[] PROGMEM = SETUP_HEAD("Pixel Jukebox setup") R"HTML(
<h2>Connect the panel to Wi-Fi</h2>
<p>Pick your home network and enter its password. The panel checks it works before saving.</p>
{{ALERT}}
<form method="post" action="/save">
  <p class="cap">Networks nearby</p>
  <div class="nets">{{NETWORKS}}</div>
  <p class="note"><a href="/?scan=1">Scan again</a>. Only 2.4 GHz networks appear; the panel can't use 5 GHz.</p>
  <label for="ssid">Network name</label>
  <input id="ssid" name="ssid" type="text" value="{{SSID}}" maxlength="32" required autocapitalize="none" autocorrect="off" spellcheck="false">
  <label for="pw">Password</label>
  <input id="pw" name="password" type="password" maxlength="63" autocomplete="off" autocapitalize="none" autocorrect="off" spellcheck="false">
  <label class="show"><input type="checkbox" id="show"> Show password</label>
  <button class="go">Connect</button>
  <p class="note">Leave the password empty for an open network.</p>
</form>
</main>
<script>
var ssid=document.getElementById('ssid'),pw=document.getElementById('pw'),nets=document.querySelectorAll('.net');
nets.forEach(function(b){b.onclick=function(){
  nets.forEach(function(n){n.classList.remove('on')});
  b.classList.add('on');ssid.value=b.dataset.ssid;pw.focus();
}});
document.getElementById('show').onchange=function(){pw.type=this.checked?'text':'password'};
</script>
</body></html>
)HTML";

static const char SETUP_CHECK_PAGE[] PROGMEM = SETUP_HEAD("Pixel Jukebox setup") R"HTML(
<h2>Checking the password</h2>
<p>The panel is trying to join <strong>{{SSID}}</strong>. This takes up to 15 seconds.</p>
<p class="note">This page updates by itself. If your phone leaves this network, rejoin Pixel-Jukebox to see the result. <a href="/result">Check now</a></p>
</main>
<script>setTimeout(function(){location.replace('/result')},2500);</script>
</body></html>
)HTML";

static const char SETUP_DONE_PAGE[] PROGMEM = SETUP_HEAD("Pixel Jukebox setup") R"HTML(
<h2>Connected</h2>
<p>The panel joined <strong>{{SSID}}</strong> and saved the details. It is restarting now.</p>
<p>This setup network is about to disappear. Put your phone back on your own Wi-Fi.</p>
</main></body></html>
)HTML";
