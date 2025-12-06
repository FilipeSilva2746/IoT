# Documentação MQTT

## Integração
O sistema usa MQTT para comunicação IoT. ESP32 conecta ao broker HiveMQ (broker.hivemq.com:1883).

## Arquitetura
- Modelo publish/subscribe.
- QoS 0 por padrão, QoS 1 para alertas críticos.

## Funcionamento
1. Setup: Conecta WiFi, gera Client ID, conecta broker, publica "online".
2. Loop: Publica a cada 2s, reconexão automática.
3. Desconexão: Keep-alive 15s, funcionamento local continua.

## Vantagens
- Leve para ESP32.
- Suporta múltiplos subscribers (ex.: dashboard https://filipesilva2746.github.io/IoT/).

Veja api.md para tópicos.
