// Trang cài đặt mạch chủ phục vụ ở http://192.168.4.1. Trạng thái và danh sách mạng lấy từ /status,
// hiển thị bằng textContent nên tên Wi-Fi lạ (có ký tự HTML) không chèn được mã vào trang.
#pragma once
#include <Arduino.h>

static const char WIFISHARE_PAGE[] PROGMEM = R"rawpage(<!doctype html>
<html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Cài Wi-Fi</title>
<style>
:root{--bg:#f3f4f6;--card:#fff;--text:#1f2328;--muted:#5f6670;--line:#d0d4da;--accent:#0b63c5;--ok:#1a7f37;--bad:#c4321c}
@media (prefers-color-scheme:dark){:root{--bg:#121417;--card:#1d2025;--text:#e9ecef;--muted:#a3abb5;--line:#3a3f46;--accent:#4c9bf0;--ok:#4fbf6a;--bad:#f2785e}}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:440px;margin:0 auto}
h1{font-size:21px;margin:4px 0}
.muted{color:var(--muted);font-size:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin:14px 0}
#msg{font-weight:600}
label{display:block;font-size:14px;margin:14px 0 6px}
input,select{width:100%;font:inherit;padding:10px 12px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text)}
.row{display:flex;align-items:center;gap:8px;margin-top:10px;font-size:14px;color:var(--muted)}
.row input{width:auto}
button{width:100%;margin-top:18px;padding:12px;font:inherit;font-weight:600;border:0;border-radius:8px;background:var(--accent);color:#fff}
button:disabled{opacity:.5}
ul{margin:8px 0 0;padding-left:20px}
.ok{color:var(--ok)}
</style></head><body><main>
<h1>Cài Wi-Fi cho các mạch</h1>
<div class="muted" id="who"></div>
<div class="card"><div id="msg">Đang tải…</div><div class="muted" id="sub"></div></div>
<form class="card" id="f">
<label for="net">Mạng xung quanh</label>
<select id="net"><option value="">Đang quét…</option></select>
<label for="ssid">Tên Wi-Fi</label>
<input id="ssid" maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false" required>
<label for="pass">Mật khẩu</label>
<input id="pass" type="password" maxlength="64" autocomplete="off">
<div class="row"><input type="checkbox" id="show"><label for="show" style="margin:0">Hiện mật khẩu</label></div>
<button id="go">Lưu và chuyển mạng</button>
</form>
<div class="card"><b>Các mạch</b><ul id="boards"></ul></div>
</main><script>
var $=function(i){return document.getElementById(i)},nets='',sending=false;
function show(m,s){$('msg').textContent=m;$('sub').textContent=s||''}
function bars(r){return r>-60?'sóng mạnh':r>-75?'sóng vừa':'sóng yếu'}
$('show').onchange=function(){$('pass').type=this.checked?'text':'password'};
$('net').onchange=function(){if(this.value){$('ssid').value=this.value;$('pass').focus()}};
$('f').onsubmit=function(e){
  e.preventDefault();var p=$('pass').value;
  if(p&&p.length<8){show('Mật khẩu Wi-Fi phải có ít nhất 8 ký tự.');return}
  sending=true;$('go').disabled=true;
  fetch('/save',{method:'POST',body:new URLSearchParams({ssid:$('ssid').value,pass:p})})
    .then(function(r){return r.text().then(function(t){if(!r.ok)show(t)})})
    .catch(function(){}).then(function(){sending=false;tick()});
};
function tick(){
  fetch('/status?v='+(document.hidden?0:1),{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){
    $('who').textContent='Mạch chủ: '+d.name;show(d.msg,d.sub);
    var ul=$('boards');ul.textContent='';
    d.boards.forEach(function(b){var li=document.createElement('li');li.textContent=b.n;
      if(b.t){var s=document.createElement('span');s.className=b.ok?'ok':'muted';s.textContent=' — '+b.t;li.appendChild(s)}ul.appendChild(li)});
    var k=JSON.stringify(d.nets);
    if(k!==nets){nets=k;var sel=$('net'),cur=sel.value;sel.textContent='';
      sel.add(new Option(d.nets.length?'Chọn mạng…':'Không thấy mạng nào',''));
      d.nets.forEach(function(n){sel.add(new Option(n.s+' ('+bars(n.r)+(n.o?', không mật khẩu':'')+')',n.s))});
      sel.value=cur;if(sel.value!==cur)sel.value=''}
    if(!sending)$('go').disabled=d.busy;
  }).catch(function(){show('Mất kết nối với mạch chủ.','Nếu điện thoại rời Wi-Fi cài đặt, hãy vào lại rồi mở 192.168.4.1.')});
}
tick();setInterval(tick,2000);
</script></body></html>)rawpage";
