# Maltworks Controller — Fase 5.2.0

## Desenvolvimento com ESP-IDF

Esta versão foi preparada para o **ESP-IDF 5.3.5** no VS Code. Para preservar o
comportamento existente durante a migração, o Arduino Core 3.3.11 é usado como
componente do ESP-IDF. Os módulos serão convertidos gradualmente para APIs
nativas, sem trocar todo o firmware de uma só vez.

No VS Code:

1. abra uma pasta de caminho curto, por exemplo `C:\Projetos\maltworkscontroller`;
2. selecione o ESP-IDF 5.3.5 na extensão Espressif;
3. selecione o alvo `esp32` e a placa ESP32 DevKit;
4. use **ESP-IDF: Build your project** para compilar;
5. conecte o controlador, selecione a porta serial e use **UART** para gravar;
6. abra o monitor serial em 115200 baud.

Pelo terminal ESP-IDF, os comandos equivalentes são:

```powershell
idf.py build
idf.py -p COM6 flash monitor
```

Troque `COM6` pela porta exibida no seu computador. Antes do primeiro teste de
bancada, desconecte as cargas de aquecimento e refrigeração: os relés do
controlador são ativos em nível baixo. A gravação não apaga automaticamente as
configurações salvas na NVS; para um teste totalmente limpo, use
`idf.py -p COM6 erase-flash` antes de gravar.

Pinagem atual dos relés:

- `IN1` (resfriamento): GPIO 27;
- `IN2` (aquecimento): GPIO 26;
- os relés são ativos em nível baixo.

Pinagem dos sensores de temperatura:

- `DAT` dos DS18B20: GPIO 23;
- alimentação: 3,3 V;
- todos os sensores compartilham o mesmo barramento e o mesmo GND.

Pinagem do display OLED I²C:

- `SCL` / clock: GPIO 18;
- `SDA` / data: GPIO 19;
- orientação da imagem: rotação de 180°;
- alimentação conforme a especificação do módulo e GND comum.

## Portal de configuração inicial

A rede de contingência `MaltworksController` usa um portal cativo. Ao conectar
um celular ou computador a essa rede, todas as consultas DNS são direcionadas
ao ESP32 e o sistema operacional pode abrir automaticamente a interface de
configuração em `http://192.168.4.1`, como ocorre em redes Wi-Fi públicas.

Se a janela não aparecer automaticamente, abra esse endereço manualmente no
navegador. A senha inicial da rede é `maltworks`.

> Novidade 5.2.0: histerese, proteção mínima do compressor, offsets dos dois
> sensores e configuração completa dos alarmes podem ser enviados pela nuvem.
> O ESP32 valida o pacote inteiro, persiste localmente, tenta restaurar os
> valores anteriores se alguma gravação falhar e confirma o resultado. Alarmes
> ativos também podem ser reconhecidos remotamente.

> Novidade 5.1.0: o ESP32 recebe receitas criadas na nuvem, grava uma copia
> local e executa ate oito etapas mesmo sem internet. Inicio, pausa, retomada e
> interrupcao sao comandos autenticados e confirmados pelo controlador.

> Novidade 5.0.6: primeiro comando remoto seguro. O ESP32 recebe a solicitação
> de setpoint junto da resposta da telemetria, valida identidade, prazo, limites
> e perfil ativo, persiste o novo valor antes de aplicá-lo e confirma o resultado
> na telemetria seguinte. Os relés nunca são comandados diretamente pela nuvem.

> Melhoria 5.0.5: o intervalo mínimo de telemetria foi reduzido para cinco
> segundos. O agendamento agora considera o início da tentativa, evitando somar
> ao intervalo o tempo gasto na conexão HTTPS. O controle térmico local não foi
> alterado.
> Configurações existentes são preservadas; após atualizar, selecione 5 segundos
> na aba Nuvem para ativar a nova cadência.

> Correção 5.0.4: o registro persistente foi compactado de 100 para 20 eventos.
> No primeiro boot, somente o blob antigo de eventos é removido para liberar as
> páginas NVS que impediam novas gravações. Wi-Fi, token, receitas, perfis,
> calibrações, alarmes e ajustes térmicos são preservados. O namespace da
> configuração cloud é criado antes do primeiro novo evento.

> Correção 5.0.3: a configuração operacional da nuvem (URL, intervalo e
> habilitação) passa a usar o namespace dedicado `mwcloudcfg` pela API NVS
> nativa, com `commit` explícito e diagnóstico individual dos códigos de erro.
> O namespace `mwcloud` e o token existente permanecem inalterados, preservando
> o Device ID, o código de vínculo e todas as demais configurações.

> Correção 5.0.2: o namespace `mwcloud` agora é aberto em modo de leitura e
> gravação somente durante cada transação e fechado logo depois. Isso elimina
> a dependência de um handle NVS mantido desde o boot e preserva o Device ID,
> o token existente e as demais configurações.

> Correção 5.0.1: o salvamento cloud agora confirma os valores pela leitura
> da NVS, aceita URL vazia quando a nuvem está desabilitada e informa qual
> campo falhou caso a memória não persista a configuração.

> Revisão de compatibilidade: o estado interno de nuvem desativada usa
> `CLOUD_DISABLED`, evitando conflito com a macro `DISABLED` do ESP32 Arduino
> Core 3.3.11.

## Objetivo desta versão

A Fase 5.2.0 mantém o firmware **Cloud Ready** sem transferir para a nuvem nenhuma função crítica de controle. O ESP32 continua executando localmente:

- leitura e calibração dos dois DS18B20;
- setpoint, histerese e perfis de fermentação;
- acionamento de aquecimento e resfriamento;
- proteção contra partidas curtas do compressor;
- alarmes, display, interface web e histórico local.

Se o Wi-Fi, a internet ou a API falharem, o controle térmico continua funcionando normalmente.

## Novo `cloudmodule`

O módulo cloud adiciona:

- Device ID único derivado do eFuse MAC do ESP32, no formato `MW-XXXXXXXXXXXX`;
- token individual aleatório de 256 bits, criado no primeiro boot e salvo na NVS `mwcloud`;
- telemetria JSON versionada;
- autenticação `Authorization: Bearer` e identificação por `X-Maltworks-Device-ID`;
- transporte HTTPS com validação do certificado do servidor;
- envio em tarefa FreeRTOS separada, sem bloquear o loop de controle;
- intervalo configurável entre 5 e 3600 segundos;
- retentativa progressiva de 15, 30, 60, 120 e 240 segundos, limitada a 300 segundos;
- estados local, online/offline, último HTTP, erro e última sincronização;
- renovação local do token sem revelar o segredo completo na interface.

## Telemetria enviada

O payload de esquema `1` contém:

- identidade do dispositivo, boot e sequência;
- versão do firmware, data/hora e uptime;
- RSSI do Wi-Fi;
- leitura bruta, leitura calibrada, offset e conexão dos dois sensores;
- setpoint, histerese, estado do controle e relés;
- proteção restante do compressor;
- estado e progresso do perfil;
- resumo dos alarmes.

O payload também confirma comandos remotos de setpoint, perfis, configuração e
reconhecimento de alarmes. Perfis ativos bloqueiam alterações manuais e de
configuração; cada comando possui identidade, validade e confirmação explícita.

## Aba Nuvem

A interface local ganhou uma aba para:

- visualizar Device ID e estado da comunicação;
- cadastrar a URL HTTPS completa do endpoint de telemetria;
- habilitar ou desabilitar a sincronização;
- escolher o intervalo de envio;
- solicitar sincronização imediata;
- renovar o token do dispositivo.

O token completo nunca é retornado pela API local. A tela mostra somente os oito últimos caracteres como código de vínculo. Na próxima etapa, o fluxo previsto é: o dispositivo se apresenta à API por HTTPS, a API armazena apenas o hash do token e o usuário vincula o equipamento à própria conta usando o Device ID e esse código curto. Assim o segredo completo não precisa trafegar pela interface HTTP local.

## Contrato HTTP esperado

O endpoint configurado deve aceitar:

```text
POST <URL configurada>
Content-Type: application/json
Authorization: Bearer <token individual>
X-Maltworks-Device-ID: MW-XXXXXXXXXXXX
X-Maltworks-Firmware: 5.2.0
```

Qualquer resposta `2xx` confirma a telemetria. `401` e `403` são tratados como falha de autenticação. Demais falhas usam retentativa progressiva.

## Certificados TLS

`cloudroots.h` contém as raízes públicas das autoridades certificadoras usadas pela Cloudflare: Google Trust Services, Let's Encrypt, SSL.com e USERTrust/Sectigo. O firmware não usa `setInsecure()`.

## Arquivos da interface

- `websource/index.html`: fonte legível da interface;
- `websource/index.html.gz`: versão comprimida;
- `webassets.cpp`: array incorporado ao firmware.

## Próxima etapa

Ampliar gradualmente os comandos remotos depois de validar o setpoint no piloto.

## Pendências antes de produção comercial

Esta versão é uma fundação para piloto, não o fechamento de segurança do produto. Antes de comercializar ainda será necessário:

- autenticar a interface local e trocar a senha fixa do ponto de acesso por uma credencial individual;
- ativar Secure Boot e Flash Encryption;
- substituir o OTA local aberto por firmware assinado;
- armazenar somente o hash do token no backend;
- aplicar expiração, limitação de tentativas e vínculo multitenant ao código de pareamento;
- assinar e proteger contra repetição os futuros comandos remotos.
