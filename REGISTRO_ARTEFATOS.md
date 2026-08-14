# Registro de artefatos de firmware

Este arquivo preserva a rastreabilidade das compilações validadas. Um registro
de validação não autoriza publicação nem rollout OTA: esses passos exigem uma
campanha piloto separada.

## 2026-08-14 — recuperação de conectividade

| Campo | Valor |
| --- | --- |
| Produto | MaltworksController |
| Versão incorporada | 5.5.1 |
| Fase | Fase5_5_1 |
| Commit de origem | `4ceecb3` (`feat: recover controller connectivity automatically`) |
| Ambiente | ESP-IDF 6.0.2, Arduino Core 3.3.11, alvo `esp32` |
| Resultado | Compilação completa concluída com `idf.py build` |
| Binário | `maltworks_controller.bin` |
| Tamanho | 1.334.192 bytes (`0x145bb0`) |
| Menor partição OTA | 1.966.080 bytes (`0x1e0000`) |
| Espaço livre | 631.888 bytes (`0x9a450`, 32%) |
| SHA-256 | `220daf5bcb3c5c4412835399675bf4596ea3c89074efe4f738d06d8424334dec` |
| Estado de distribuição | Não publicado; requer campanha piloto de um controlador |

## 2026-08-14 — teste do fluxo OTA pelo painel

| Campo | Valor |
| --- | --- |
| Produto | MaltworksController |
| Versão incorporada | 5.5.2 |
| Fase | Fase5_5_2 |
| Commit de origem | `709fc02` (`feat: prepare firmware 5.5.2 OTA test`) |
| Ambiente | ESP-IDF 6.0.2, Arduino Core 3.3.11, alvo `esp32` |
| Resultado | Compilação completa concluída com `idf.py build`; metadados internos validados |
| Alteração observável | Texto da interface local tornado independente da versão |
| Binário | `maltworks_controller.bin` |
| Tamanho | 1.334.400 bytes (`0x145c80`) |
| Menor partição OTA | 1.966.080 bytes (`0x1e0000`) |
| Espaço livre | 631.680 bytes (`0x9a380`, 32%) |
| SHA-256 | `0d2f40ec32099ed97f72db9a1b829db59cfc7708870b9eafcdb07a265940fc7b` |
| Estado de distribuição | Publicado no catálogo; aguardando teste em um controlador piloto |

### Procedimento mínimo para próximos registros

1. Registrar o commit, versões de ESP-IDF e Arduino Core, data e SHA-256.
2. Confirmar a compilação completa e o espaço disponível na menor partição OTA.
3. Publicar o binário sob uma versão nova; nunca substituir um artefato já
   catalogado.
4. Iniciar o rollout por um controlador piloto e registrar o resultado antes de
   ampliar a campanha.
