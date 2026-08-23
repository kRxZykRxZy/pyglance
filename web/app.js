const $=id=>document.getElementById(id);

async function api(url,options={}){
  const r=await fetch(url,{credentials:'same-origin',...options});
  if(r.status===401){location='/';throw new Error('Session expired');}
  return r.json();
}

function bytes(n){
  if(n<1024)return `${n} B`;
  if(n<1048576)return `${(n/1024).toFixed(1)} KB`;
  if(n<1073741824)return `${(n/1048576).toFixed(1)} MB`;
  return `${(n/1073741824).toFixed(2)} GB`;
}

async function refresh(){
  try{
    const x=await api('/api/status');
    $('cpu').textContent=`${x.cpu.toFixed(1)}%`;
    $('ram').textContent=`${bytes(x.ram_used)} / ${bytes(x.ram_total)}`;
    $('disk').textContent=`${x.disk_percent.toFixed(1)}%`;
    $('uptime').textContent=`${x.uptime}s`;
    $('rx').textContent=bytes(x.rx);
    $('tx').textContent=bytes(x.tx);
  }catch(e){}
}

async function loadProcesses(){
  const list=await api('/api/processes');
  $('processList').innerHTML='<table><tr><th>PID</th><th>Name</th><th>State</th><th>Actions</th></tr>'+list.map(p=>
    `<tr><td>${p.pid}</td><td>${p.name}</td><td>${p.state}</td><td>`+
    `<button onclick="signalProcess(${p.pid},19)">Stop</button> `+
    `<button onclick="signalProcess(${p.pid},18)">Resume</button> `+
    `<button onclick="signalProcess(${p.pid},15)">Terminate</button></td></tr>`
  ).join('')+'</table>';
}

async function signalProcess(pid,sig){
  await api(`/api/signal?pid=${pid}&sig=${sig}`,{method:'POST'});
  loadProcesses();
}

async function loadPorts(){
  const list=await api('/api/ports');
  $('portList').innerHTML='<table><tr><th>Protocol</th><th>Port</th></tr>'+list.map(p=>
    `<tr><td>${p.proto}</td><td>${p.port}</td></tr>`).join('')+'</table>';
}

function selectTab(id){
  document.querySelectorAll('.tab').forEach(x=>x.classList.remove('show'));
  document.querySelectorAll('nav button').forEach(x=>x.classList.remove('active'));
  document.getElementById(id).classList.add('show');
  document.querySelector(`[data-tab="${id}"]`).classList.add('active');
  if(id==='processes')loadProcesses();
  if(id==='ports')loadPorts();
}

document.querySelectorAll('nav button').forEach(button=>button.onclick=()=>selectTab(button.dataset.tab));

const form=$('loginForm');
if(form){
  form.onsubmit=async e=>{
    e.preventDefault();
    const button=$('loginButton');
    const error=$('error');
    button.disabled=true;
    button.textContent='Signing in...';
    error.textContent='';
    try{
      const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(new FormData(form))});
      const data=await r.json();
      if(!r.ok||!data.ok)throw new Error(data.error||'Login failed');
      location='/dashboard';
    }catch(err){error.textContent=err.message;}
    finally{button.disabled=false;button.textContent='Sign in';}
  };
}

if($('cpu')){refresh();setInterval(refresh,3000);}
