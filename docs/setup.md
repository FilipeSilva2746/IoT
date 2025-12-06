# Guia de Instalação

## Requisitos
- Arduino IDE (https://www.arduino.cc/en/software).
- Bibliotecas: PubSubClient, WiFi, ESP32 board support.
- Hardware: ESP32, AD8232, Buzzer KY-012, etc. (veja hardware/components.md).

## Passos
1. Instale o Arduino IDE.
2. Adicione suporte para ESP32: File > Preferences > Additional Boards Manager URLs: `https://dl.espressif.com/dl/package_esp32_index.json`.
3. Instale bibliotecas via Library Manager: PubSubClient, etc.
4. Abra src/main.cpp no IDE.
5. Configure WiFi SSID e senha no código.
6. Compile e upload para o ESP32.
7. Monitore via Serial Monitor.

Para simulação: Importe hardware/diagram.json no Wokwi (https://wokwi.com).
