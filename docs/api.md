# Documentação dos Tópicos MQTT

## Tópicos Utilizados
| Tópico                  | Tipo      | Formato         | Frequência | Descrição             |
|-------------------------|-----------|-----------------|------------|-----------------------|
| mackenzie/cardiac/bpm  | Publicação | Inteiro (30-220)| 2s        | BPM calculado        |
| mackenzie/cardiac/state| Publicação | String          | Ao mudar  | Estado clínico atual |
| mackenzie/cardiac/alert| Publicação | String          | Ao mudar  | Tipo de alerta ativo |
| mackenzie/cardiac/status| Publicação| String          | Ao mudar  | Status do dispositivo|
| mackenzie/cardiac/raw_adc| Publicação| Inteiro (0-4095)| 2s       | Valor ADC não filtrado|

## Formatos das Mensagens
- BPM: "85", "120"
- State: "NORMAL", "TAQUICARDIA
