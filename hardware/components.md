# Lista de Componentes

## Componentes Principais (Protótipo Físico)
| Componente          | Descrição                      | Quantidade |
|---------------------|--------------------------------|------------|
| AD8232             | Sensor ECG integrado           | 1         |
| ESP32              | Microcontrolador Wi-Fi/Bluetooth | 1       |
| Buzzer KY-012      | Atuador sonoro ativo           | 1         |
| Eletrodos biomédicos | Pads adesivos para contato cutâneo | 3    |
| Breadboard         | Placa de prototipagem          | 1         |

## Componentes no Simulador Wokwi
### Sensores
| Componente       | GPIO | Função                  | Detalhes Técnicos |
|------------------|------|-------------------------|-------------------|
| Potenciômetro (simula ECG) | 34 | Simular sinal analógico de ECG | ADC 12 bits (0–4095); usado para estimar BPM (30–220 bpm); filtrado por média móvel (15 amostras) |
| Botão Lead-Off RA/LA | 18 | Detectar desconexão de eletrodos | Pull-up interno; LOW = eletrodo desconectado; debounce de 50 ms |
| Botão Lead-Off RL | 19 | Detectar desconexão de eletrodos | Pull-up interno; LOW = eletrodo desconectado; debounce de 50 ms |

### Atuadores
| Componente | GPIO | Função          | Comportamento |
|------------|------|-----------------|---------------|
| LED Verde | 26  | Indicar status WiFi/MQTT | Pisca brevemente a cada publicação MQTT; indica comunicação ativa |
| LED Amarelo | 27 | Indicar batimento cardíaco | Pisca com período 60000/BPM ms sincronizado com o batimento |
| LED Vermelho | 14 | Alerta crítico | Ativa em taquicardia, bradicardia ou Leads-Off |
| Buzzer    | 25  | Alarme sonoro   | 2500 Hz (taquicardia), 400 Hz (bradicardia), silencioso no estado normal |

### Outros
| Componente               | Quantidade | Função          | Observações |
|--------------------------|------------|-----------------|-------------|
| Pads de eletrodos biomédicos (RA, LA, RL) | 3 | Contato com pele para ECG | Pads adesivos com gel condutor, baixa impedância |
| Breadboard              | 1         | Prototipagem    | Conexões sem solda |
| Cabos jumper            | —         | Conexão dos circuitos | Macho-macho na simulação |
| Fonte 3.3V (do próprio ESP32) | 1   | Alimentação     | Fornece energia para sensores e LEDs |
