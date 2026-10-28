# Autenticacao local do Controller piloto (sem display)

Status: trabalho iniciado na branch; **nao pronto para instalar**.

## Decisao de produto (28/10/2026)
- O primeiro usuario cria uma senha administrativa local, diferente da senha do Wi-Fi e da senha do Maltworks Cloud.
- A versao piloto nao tem display. O primeiro acesso sera pelo portal cativo usando celular ou notebook.
- Controle de temperatura, segurancas dos reles e protecao de compressor permanecem locais, independentes de login, Wi-Fi ou nuvem.

## Fluxo-alvo
1. Dispositivo nao reivindicado cria AP de provisionamento protegido por WPA2 com segredo unico por unidade, impresso em etiqueta junto ao equipamento. Nao manter SSID aberto como na versao atual.
2. Acesso a http://192.168.4.1 via essa rede; criar senha de administrador (12-64 caracteres) e confirmar. O aplicativo deve evitar que a senha seja enviada a uma rede aberta. **HTTP no AP WPA2 ainda nao equivale a HTTPS ponta a ponta**.
3. So apos criar a senha, mostrar identidade de cadastro e configurar Wi-Fi. Confirmar Wi-Fi, depois vincular na conta cloud.
4. Ao acessar via rede domestica, exigir login para dashboard, dados detalhados, rotas de configuracao, eventos, alteracoes de operacao e OTA.
5. Proteger `/firmware/update` antes de `UPLOAD_FILE_START` (antes de `relays.allOff()`, `Update.begin` e qualquer flash write). Nao basta validar o callback final.
6. Usar sessoes aleatorias com expiração e protecao CSRF/Origin nas rotas de escrita; limitar tentativas e invalidar sessoes apos troca/reset de senha.
7. Recuperacao de senha so por gesto fisico documentado (botao/contato dedicado no hardware piloto), sem apagar automaticamente Wi-Fi, perfis, receitas ou identidade/token cloud. Nao assumir botao existente.
8. Compatibilidade: equipamentos com Wi-Fi e NVS existentes devem entrar em modo de migração autenticado por procedimento fisico, e **nunca** permitir re-claim apenas por ter senha ausente.

## Implementacao parcial nesta branch
- `SettingsModule` oferece geracao de salt aleatorio, PBKDF2-HMAC-SHA256 e verificacao de senha com comparacao constante; guarda apenas sal/hash em NVS.
- Nenhum endpoint ou pagina web foi protegido nesta entrega parcial.
- Nenhuma alteracao em `main` ou deploy/firmware OTA foi realizada.
- Precisa compilar com ESP-IDF 5.3.5 + Arduino Core 3.3.11 e validar assinatura de `mbedtls_pkcs5_pbkdf2_hmac`.

## Bloqueadores para ativacao
- Definir AP WPA2 por unidade e metodo de entrega do segredo fisico; o AP atual e aberto.
- Implementar sessao, interface de primeiro acesso/login, auth guard de todas as rotas e upload.
- Definir hardware de recuperacao (sem display), e caminho de migracao de dispositivos existentes.
- Testar primeiro uso, perda de Wi-Fi, reinicio, perda de sessao, brute force, CSRF, upload nao autorizado e preservacao dos relés.
