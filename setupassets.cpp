#include "setupassets.h"

const char SETUP_HTML[] PROGMEM = R"MWSETUP(<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#17130f">
  <title>Configurar Maltworks</title>
  <style>
    :root{color-scheme:dark;--bg:#17130f;--card:#241e18;--line:#493d31;--gold:#d69f45;--text:#f5eee5;--muted:#b9aa9a;--ok:#73c991;--bad:#ed826f}
    *{box-sizing:border-box}body{margin:0;min-height:100vh;background:radial-gradient(circle at 15% 0,#382919 0,transparent 36%),var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,Segoe UI,sans-serif}
    main{width:min(560px,100%);margin:auto;padding:28px 18px 44px}.brand{display:flex;align-items:center;gap:12px;margin-bottom:28px}.mark{width:38px;height:38px;border:2px solid var(--gold);border-radius:50%;display:grid;place-items:center;color:var(--gold);font-weight:800}.brand b{display:block;letter-spacing:.14em}.brand small{color:var(--muted);letter-spacing:.18em}
    .eyebrow{color:var(--gold);font-size:.76rem;font-weight:800;letter-spacing:.16em;text-transform:uppercase}h1{font-size:clamp(2rem,8vw,3.2rem);line-height:1.02;margin:.35rem 0 1rem}p{color:var(--muted)}
    .card{background:color-mix(in srgb,var(--card) 94%,transparent);border:1px solid var(--line);border-radius:20px;padding:22px;margin-top:18px;box-shadow:0 18px 45px #0006}.steps{display:flex;gap:8px;margin:20px 0}.step{height:4px;flex:1;border-radius:9px;background:var(--line)}.step.on{background:var(--gold)}
    label{display:block;margin:16px 0 7px;font-weight:700}input{width:100%;border:1px solid var(--line);border-radius:12px;background:#120f0c;color:var(--text);padding:14px;font:inherit;outline:none}input:focus{border-color:var(--gold);box-shadow:0 0 0 3px #d69f4528}.password{display:flex;gap:8px}.password input{flex:1}.password button{width:auto;padding:0 14px;background:#30271f}
    button,.button{display:inline-flex;align-items:center;justify-content:center;width:100%;border:0;border-radius:12px;padding:14px 18px;background:var(--gold);color:#1b140c;font:800 .9rem/1 system-ui;text-decoration:none;cursor:pointer;letter-spacing:.04em}button:disabled{opacity:.55;cursor:wait}.message{min-height:24px;margin-top:12px;color:var(--muted)}.message.ok{color:var(--ok)}.message.bad{color:var(--bad)}
    .identity{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:16px 0}.identity div{padding:13px;border:1px solid var(--line);border-radius:12px;background:#17120e}.identity span{display:block;color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.08em}.identity strong{display:block;margin-top:4px;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:.92rem;word-break:break-all}.note{font-size:.88rem}.hidden{display:none!important}
    @media(max-width:430px){main{padding-top:20px}.identity{grid-template-columns:1fr}.card{padding:18px}}
  </style>
</head>
<body>
<main>
  <div class="brand"><span class="mark">M</span><span><b>MALTWORKS</b><small>INSTALAÇÃO</small></span></div>
  <p class="eyebrow">Primeiro acesso</p>
  <h1>Vamos conectar seu controlador.</h1>
  <p>Informe a rede Wi-Fi do local. A senha fica armazenada somente no ESP32 e nunca será usada como senha da rede do controlador.</p>
  <div class="steps"><span class="step on"></span><span class="step"></span><span class="step"></span></div>

  <section class="card" id="wifiCard">
    <p class="eyebrow">Etapa 1 de 3 · Rede</p>
    <form id="wifiForm">
      <label for="ssid">Nome da rede Wi-Fi</label>
      <input id="ssid" name="ssid" maxlength="32" autocomplete="off" required>
      <label for="password">Senha da rede</label>
      <div class="password"><input id="password" name="password" type="password" minlength="8" maxlength="63" autocomplete="new-password"><button type="button" id="showPassword" aria-label="Mostrar senha">Ver</button></div>
      <p class="note">Deixe a senha vazia somente se a rede doméstica também for aberta.</p>
      <button id="saveButton" type="submit">CONECTAR CONTROLADOR</button>
      <div id="wifiMessage" class="message" role="status"></div>
    </form>
  </section>

  <section class="card" id="linkCard">
    <p class="eyebrow">Etapas 2 e 3 · Identificação</p>
    <h2>Vincule ao Maltworks Cloud</h2>
    <p>Guarde estes dados. A rede de instalação continuará aberta até você confirmar que está pronto para acessar o painel.</p>
    <div class="identity"><div><span>Device ID</span><strong id="deviceId">carregando...</strong></div><div><span>Código de vínculo</span><strong id="pairingCode">--------</strong></div></div>
    <button id="claimButton" type="button" disabled>AGUARDANDO CONEXÃO WI-FI</button>
    <div id="claimMessage" class="message" role="status">Primeiro conecte o controlador ao Wi-Fi.</div>
    <p class="note">Ao continuar, a rede MaltworksController será fechada. Seu celular voltará para a internet e o portal abrirá com estes dados preenchidos.</p>
  </section>
</main>
<script>
  const $=id=>document.getElementById(id);
  let identity={deviceId:"",pairingCode:""};
  let claimUrl="https://app.maltworks.com.br";
  let statusTimer=null;
  async function loadStatus(){
    try{
      const response=await fetch("/api",{cache:"no-store"});
      const data=await response.json();
      identity.deviceId=data.cloud?.deviceId||"";
      identity.pairingCode=data.cloud?.tokenHint||"";
      $("deviceId").textContent=identity.deviceId||"indisponível";
      $("pairingCode").textContent=identity.pairingCode||"--------";
      const query=new URLSearchParams({claimDevice:identity.deviceId,pairingCode:identity.pairingCode});
      claimUrl="https://app.maltworks.com.br/?"+query.toString();
      const ready=Boolean(data.stationConnected&&data.configurationCompletionPending);
      $("claimButton").disabled=!ready;
      $("claimButton").textContent=ready?"CONTINUAR PARA O MALTWORKS CLOUD":"AGUARDANDO CONEXÃO WI-FI";
      if(ready){
        $("claimMessage").textContent="Wi-Fi confirmado. Confira os dados e continue quando estiver pronto.";
        $("claimMessage").className="message ok";
        if(statusTimer){clearInterval(statusTimer);statusTimer=null;}
      }
    }catch(error){
      if(!statusTimer){
        $("claimMessage").textContent="Não foi possível confirmar a conexão. Tente novamente.";
        $("claimMessage").className="message bad";
      }
    }
  }
  $("showPassword").addEventListener("click",()=>{
    const field=$("password");
    field.type=field.type==="password"?"text":"password";
    $("showPassword").textContent=field.type==="password"?"Ver":"Ocultar";
  });
  $("wifiForm").addEventListener("submit",async event=>{
    event.preventDefault();
    const button=$("saveButton");
    const message=$("wifiMessage");
    button.disabled=true;
    message.textContent="Salvando e tentando conectar...";
    message.className="message";
    try{
      const body=new URLSearchParams({ssid:$("ssid").value,password:$("password").value});
      const response=await fetch("/wifi/save",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});
      const result=await response.json();
      if(!response.ok||!result.success)throw new Error(result.message||"Falha ao salvar a rede.");
      message.textContent="Credenciais salvas. Validando o Wi-Fi sem fechar esta página...";
      message.className="message ok";
      if(!statusTimer)statusTimer=setInterval(()=>void loadStatus(),1000);
      void loadStatus();
    }catch(error){
      message.textContent=error.message||"Não foi possível configurar o Wi-Fi.";
      message.className="message bad";
      button.disabled=false;
    }
  });
  $("claimButton").addEventListener("click",async()=>{
    const button=$("claimButton");
    const message=$("claimMessage");
    button.disabled=true;
    button.textContent="PREPARANDO O PORTAL...";
    message.textContent="A rede do controlador será fechada. Aguarde seu celular recuperar a internet.";
    message.className="message";
    try{
      const response=await fetch("/setup/complete",{method:"POST"});
      const result=await response.json();
      if(!response.ok||!result.success)throw new Error(result.message||"Wi-Fi ainda não confirmado.");
      message.textContent="Tudo pronto. Redirecionando para o Maltworks Cloud...";
      message.className="message ok";
      setTimeout(()=>window.location.replace(claimUrl),4500);
    }catch(error){
      message.textContent=error.message||"Não foi possível concluir a instalação.";
      message.className="message bad";
      button.disabled=false;
      button.textContent="TENTAR NOVAMENTE";
      if(!statusTimer)statusTimer=setInterval(()=>void loadStatus(),1000);
    }
  });
  void loadStatus();
</script>
</body>
</html>)MWSETUP";
