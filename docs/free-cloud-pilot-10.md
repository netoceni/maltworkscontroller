# Controller — cadência do piloto gratuito Maltworks (até 10 dispositivos)

Esta branch **não grava firmware nem executa OTA**. Requer validação em
ESP32 físico antes do lançamento.

## Novo padrão para configurações ainda não gravadas na NVS
- Telemetria HTTP: **a cada 30 segundos**.
- Heartbeat WebSocket: **a cada 30 segundos**.
- A API correspondente deve usar **180 segundos** entre gravações normais
  no D1 e **90 segundos** para considerar o dispositivo offline.
- A segurança térmica local (sensor, relés, compressor e etapas) não
  depende do Cloud, do heartbeat ou do plano de hospedagem.

O intervalo HTTP configurado previamente fica armazenado em NVS.
**Controladores já configurados em 5 segundos não mudam automaticamente
para 30 segundos após atualização.** Reconfigurar manualmente em
`/cloud` ou pelo painel de instalação quando o ESP32 estiver disponível.
O PR independente de autenticação do portal (`Controller #2`)
continua pendente e não foi integrado a esta branch.

## Capacidade gratuita alvo (10 dispositivos 24h)
| Operação | Requisições/dia |
| --- | ---: |
| HTTP 30 s | 28.800 (Workers) |
| WebSocket heartbeat 30 s | 28.800 (DO) |
| Repasses HTTP ao DO | aproximadamente 28.800 (DO) |
| Total DO antes do uso do painel | aproximadamente 57.600 |

Workers Free e Durable Objects Free oferecem 100.000 requisições/dia.
O plano ainda depende de manter consumo adicional moderado e medir os
indicadores do Cloudflare. Consulte a estratégia D1 correspondente
no API PR #17, e o histórico no API PR #14.

**Validações pendentes:** compilação CI, conectividade real do WebSocket
a 30s, perda de Wi-Fi, 90s offline e reconexão, comandos, OTA, config
NVS antiga, vida útil da conexão, alertas, comportamento da geladeira.
