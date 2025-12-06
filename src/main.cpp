#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================================
// CONFIGURAÇÃO WiFi e MQTT
// ============================================================================
namespace Config {
  // Configurações WiFi
  const char* SSID = "Wokwi-GUEST";
  const char* PASSWORD = "";
  
  // Configurações MQTT Broker - HiveMQ Public Broker
  const char* MQTT_BROKER = "broker.hivemq.com";  // Broker público HiveMQ
  const int MQTT_PORT = 1883;
  const char* MQTT_CLIENT_ID = "ESP32_CardiacMonitor_";  // Será concatenado com ID único
  
  // Tópicos MQTT (use um prefixo único para evitar conflitos)
  const char* TOPIC_BPM = "mackenzie/cardiac/bpm";
  const char* TOPIC_STATE = "mackenzie/cardiac/state";
  const char* TOPIC_ALERT = "mackenzie/cardiac/alert";
  const char* TOPIC_STATUS = "mackenzie/cardiac/status";
  const char* TOPIC_RAW = "mackenzie/cardiac/raw_adc";
}

// ============================================================================
// CONFIGURAÇÃO DE HARDWARE
// ============================================================================
namespace Pins {
  constexpr uint8_t POT = 34;
  constexpr uint8_t LO_PLUS = 18;
  constexpr uint8_t LO_MINUS = 19;
  constexpr uint8_t BUZZER = 25;
  constexpr uint8_t LED_WIFI = 26;
  constexpr uint8_t LED_PROCESS = 27;
  constexpr uint8_t LED_ALERT = 14;
}

// ============================================================================
// CONSTANTES CLÍNICAS
// ============================================================================
namespace Clinical {
  constexpr int BPM_MIN = 30;
  constexpr int BPM_MAX = 220;
  constexpr int TACHY_THRESHOLD = 100;
  constexpr int BRADY_THRESHOLD = 60;
  constexpr int HYSTERESIS = 5;
  constexpr int TACHY_TONE = 2500;
  constexpr int BRADY_TONE = 400;
  constexpr int ADC_MIN = 0;
  constexpr int ADC_MAX = 4095;
}

// ============================================================================
// CONSTANTES DE TEMPO
// ============================================================================
namespace Timing {
  constexpr unsigned long SERIAL_INTERVAL = 1000;
  constexpr unsigned long MQTT_PUBLISH_INTERVAL = 2000;  // Publica a cada 2s
  constexpr unsigned long LED_PULSE_DURATION = 100;
  constexpr unsigned long STARTUP_LED_DELAY = 200;
  constexpr unsigned long DEBOUNCE_DELAY = 50;
  constexpr unsigned long STATE_STABILITY = 3000;
  constexpr unsigned long WIFI_RETRY_DELAY = 5000;
  constexpr unsigned long MQTT_RETRY_DELAY = 5000;
}

// ============================================================================
// ENUMERAÇÕES
// ============================================================================
enum class ClinicalState : uint8_t {
  NORMAL,
  TACHYCARDIA,
  BRADYCARDIA
};

// ============================================================================
// CLASSE: FILTRO DE MÉDIA MÓVEL
// ============================================================================
template<size_t N>
class MovingAverageFilter {
private:
  int readings[N] = {0};
  size_t index = 0;
  long total = 0;
  size_t count = 0;

public:
  int addReading(int value) {
    total -= readings[index];
    readings[index] = value;
    total += value;
    
    if (count < N) count++;
    index = (index + 1) % N;
    
    return total / count;
  }

  void reset() {
    for (size_t i = 0; i < N; i++) readings[i] = 0;
    index = 0;
    total = 0;
    count = 0;
  }
};

// ============================================================================
// CLASSE: GERENCIADOR MQTT
// ============================================================================
class MQTTManager {
private:
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  unsigned long lastReconnectAttempt = 0;
  bool mqttConnected = false;

public:
  MQTTManager() : mqttClient(wifiClient) {
    mqttClient.setServer(Config::MQTT_BROKER, Config::MQTT_PORT);
  }

  bool connectWiFi() {
    Serial.println(F("\n[WiFi] Conectando..."));
    WiFi.begin(Config::SSID, Config::PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(F("."));
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("\n[WiFi] ✓ Conectado!"));
      Serial.print(F("[WiFi] IP: "));
      Serial.println(WiFi.localIP());
      return true;
    } else {
      Serial.println(F("\n[WiFi] ✗ Falha na conexão"));
      return false;
    }
  }

  bool connectMQTT() {
    if (mqttClient.connected()) {
      return true;
    }

    unsigned long now = millis();
    if (now - lastReconnectAttempt < Timing::MQTT_RETRY_DELAY) {
      return false;
    }
    lastReconnectAttempt = now;

    Serial.print(F("[MQTT] Conectando ao broker "));
    Serial.print(Config::MQTT_BROKER);
    Serial.print(F(":"));
    Serial.print(Config::MQTT_PORT);
    Serial.print(F("..."));

    // Cria ID único baseado no MAC address
    String clientId = String(Config::MQTT_CLIENT_ID) + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(F(" ✓ Conectado!"));
      Serial.print(F("[MQTT] Client ID: "));
      Serial.println(clientId);
      mqttConnected = true;
      
      // Publica mensagem de inicialização
      publish(Config::TOPIC_STATUS, "online");
      return true;
    } else {
      Serial.print(F(" ✗ Falha (rc="));
      Serial.print(mqttClient.state());
      Serial.println(F(")"));
      mqttConnected = false;
      return false;
    }
  }

  bool publish(const char* topic, const char* payload) {
    if (!mqttClient.connected()) {
      return false;
    }
    return mqttClient.publish(topic, payload);
  }

  bool publish(const char* topic, int value) {
    char buffer[16];
    itoa(value, buffer, 10);
    return publish(topic, buffer);
  }

  void loop() {
    if (WiFi.status() != WL_CONNECTED) {
      mqttConnected = false;
      return;
    }

    if (!mqttClient.connected()) {
      mqttConnected = false;
      connectMQTT();
    } else {
      mqttClient.loop();
    }
  }

  bool isConnected() {
    return mqttConnected && mqttClient.connected();
  }
};

// ============================================================================
// CLASSE: MONITOR CARDÍACO
// ============================================================================
class CardiacMonitor {
private:
  MovingAverageFilter<15> filter;
  MQTTManager mqtt;
  
  ClinicalState currentState = ClinicalState::NORMAL;
  ClinicalState targetState = ClinicalState::NORMAL;
  ClinicalState lastPublishedState = ClinicalState::NORMAL;
  
  int currentBPM = 0;
  int lastPublishedBPM = 0;
  int rawADC = 0;
  int filteredADC = 0;
  int minADC = 4095;
  int maxADC = 0;
  
  bool leadsOffDetected = false;
  bool lastLeadsOffState = false;
  
  unsigned long lastHeartbeat = 0;
  unsigned long lastPublication = 0;
  unsigned long lastMQTTPublish = 0;
  unsigned long lastLeadsCheck = 0;
  unsigned long stateChangeTime = 0;
  unsigned long wifiLedStartTime = 0;
  bool wifiLedActive = false;

  void testLEDs() {
    const uint8_t leds[] = {Pins::LED_WIFI, Pins::LED_PROCESS, Pins::LED_ALERT};
    for (uint8_t led : leds) {
      digitalWrite(led, HIGH);
      delay(Timing::STARTUP_LED_DELAY);
      digitalWrite(led, LOW);
    }
  }

  bool checkLeadsOff() {
    unsigned long now = millis();
    if (now - lastLeadsCheck < Timing::DEBOUNCE_DELAY) {
      return leadsOffDetected;
    }
    lastLeadsCheck = now;

    bool error = (digitalRead(Pins::LO_PLUS) == LOW) || 
                 (digitalRead(Pins::LO_MINUS) == LOW);

    if (error != leadsOffDetected) {
      leadsOffDetected = error;
      
      if (error) {
        handleLeadsOffError();
      } else {
        clearLeadsOffError();
      }
    }

    return leadsOffDetected;
  }

  void handleLeadsOffError() {
    manageAlarm(false, 0);
    digitalWrite(Pins::LED_ALERT, HIGH);
    Serial.println(F("\n[ERRO] Eletrodos desconectados!"));
    
    // Publica alerta via MQTT
    mqtt.publish(Config::TOPIC_ALERT, "LEADS_OFF");
    mqtt.publish(Config::TOPIC_STATUS, "leads_disconnected");
    
    currentState = ClinicalState::NORMAL;
    targetState = ClinicalState::NORMAL;
  }

  void clearLeadsOffError() {
    digitalWrite(Pins::LED_ALERT, LOW);
    Serial.println(F("[INFO] Eletrodos reconectados"));
    
    // Publica status via MQTT
    mqtt.publish(Config::TOPIC_ALERT, "LEADS_OK");
    mqtt.publish(Config::TOPIC_STATUS, "monitoring");
  }

  int readAndFilterBPM() {
    rawADC = analogRead(Pins::POT);
    
    if (rawADC < minADC) minADC = rawADC;
    if (rawADC > maxADC) maxADC = rawADC;
    
    filteredADC = filter.addReading(rawADC);
    
    int bpm = map(filteredADC, Clinical::ADC_MIN, Clinical::ADC_MAX, 
                  Clinical::BPM_MIN, Clinical::BPM_MAX);
    
    return constrain(bpm, Clinical::BPM_MIN, Clinical::BPM_MAX);
  }

  void updateHeartbeatLED() {
    unsigned long now = millis();
    
    if (currentBPM <= 0) return;
    
    long beatInterval = 60000L / currentBPM;

    if (now - lastHeartbeat >= (unsigned long)beatInterval) {
      lastHeartbeat = now;
      digitalWrite(Pins::LED_PROCESS, HIGH);
      delayMicroseconds(100000);
      digitalWrite(Pins::LED_PROCESS, LOW);
    }
  }

  void updateWifiLED() {
    unsigned long now = millis();
    if (wifiLedActive && (now - wifiLedStartTime >= 50)) {
      digitalWrite(Pins::LED_WIFI, LOW);
      wifiLedActive = false;
    }
  }

  ClinicalState calculateTargetState() {
    if (currentBPM > Clinical::TACHY_THRESHOLD) {
      return ClinicalState::TACHYCARDIA;
    } 
    else if (currentBPM < Clinical::BRADY_THRESHOLD) {
      return ClinicalState::BRADYCARDIA;
    }
    
    if (currentState == ClinicalState::TACHYCARDIA) {
      if (currentBPM < (Clinical::TACHY_THRESHOLD - Clinical::HYSTERESIS)) {
        return ClinicalState::NORMAL;
      }
      return ClinicalState::TACHYCARDIA;
    }
    
    if (currentState == ClinicalState::BRADYCARDIA) {
      if (currentBPM > (Clinical::BRADY_THRESHOLD + Clinical::HYSTERESIS)) {
        return ClinicalState::NORMAL;
      }
      return ClinicalState::BRADYCARDIA;
    }
    
    return ClinicalState::NORMAL;
  }

  void evaluateClinicalState() {
    unsigned long now = millis();
    
    ClinicalState newTarget = calculateTargetState();
    
    if (newTarget != targetState) {
      targetState = newTarget;
      stateChangeTime = now;
      return;
    }
    
    if (targetState != currentState) {
      if (now - stateChangeTime >= Timing::STATE_STABILITY) {
        currentState = targetState;
        handleStateChange();
      }
    }
  }

  void handleStateChange() {
    const char* stateStr = "";
    const char* alertStr = "";
    
    switch (currentState) {
      case ClinicalState::TACHYCARDIA:
        Serial.println(F("\n⚠️  ALERTA: TAQUICARDIA DETECTADA!"));
        manageAlarm(true, Clinical::TACHY_TONE);
        stateStr = "TACHYCARDIA";
        alertStr = "HIGH_HR";
        break;

      case ClinicalState::BRADYCARDIA:
        Serial.println(F("\n⚠️  ALERTA: BRADICARDIA DETECTADA!"));
        manageAlarm(true, Clinical::BRADY_TONE);
        stateStr = "BRADYCARDIA";
        alertStr = "LOW_HR";
        break;

      case ClinicalState::NORMAL:
        Serial.println(F("\n✓ Ritmo cardíaco normalizado"));
        manageAlarm(false, 0);
        stateStr = "NORMAL";
        alertStr = "NORMAL";
        break;
    }
    
    // Publica mudança de estado via MQTT
    mqtt.publish(Config::TOPIC_STATE, stateStr);
    mqtt.publish(Config::TOPIC_ALERT, alertStr);
  }

  void manageAlarm(bool enable, int frequency) {
    if (enable) {
      digitalWrite(Pins::LED_ALERT, HIGH);
      tone(Pins::BUZZER, frequency);
    } else {
      digitalWrite(Pins::LED_ALERT, LOW);
      noTone(Pins::BUZZER);
    }
  }

  const char* getStateString() const {
    switch (currentState) {
      case ClinicalState::NORMAL:      return "NORMAL";
      case ClinicalState::TACHYCARDIA: return "TAQUICARDIA";
      case ClinicalState::BRADYCARDIA: return "BRADICARDIA";
      default: return "UNKNOWN";
    }
  }

  void publishToMQTT() {
    unsigned long now = millis();
    
    if (now - lastMQTTPublish < Timing::MQTT_PUBLISH_INTERVAL) {
      return;
    }
    
    if (!mqtt.isConnected()) {
      return;
    }
    
    lastMQTTPublish = now;
    
    // Pisca LED WiFi ao publicar
    digitalWrite(Pins::LED_WIFI, HIGH);
    wifiLedStartTime = now;
    wifiLedActive = true;
    
    // Publica BPM (sempre)
    mqtt.publish(Config::TOPIC_BPM, currentBPM);
    
    // Publica ADC raw (sempre)
    mqtt.publish(Config::TOPIC_RAW, rawADC);
    
    // Publica estado apenas se mudou
    if (currentState != lastPublishedState) {
      mqtt.publish(Config::TOPIC_STATE, getStateString());
      lastPublishedState = currentState;
    }
    
    Serial.print(F("[MQTT] Publicado → BPM: "));
    Serial.print(currentBPM);
    Serial.print(F(", Estado: "));
    Serial.println(getStateString());
  }

  void publishData() {
    unsigned long now = millis();
    if (now - lastPublication >= Timing::SERIAL_INTERVAL) {
      lastPublication = now;

      Serial.println(F("┌─────────────────────────────────────┐"));
      Serial.print(F("│ RAW ADC:      ")); 
      Serial.print(rawADC);
      Serial.print(F(" (min: ")); 
      Serial.print(minADC);
      Serial.print(F(", max: ")); 
      Serial.print(maxADC);
      Serial.println(F(")"));
      
      Serial.print(F("│ FILTERED ADC: ")); 
      Serial.println(filteredADC);
      
      Serial.print(F("│ BPM:          ")); 
      Serial.print(currentBPM);
      Serial.println(F(" bpm"));
      
      Serial.print(F("│ ESTADO:       "));
      Serial.print(getStateString());
      
      if (targetState != currentState) {
        Serial.print(F(" → "));
        switch (targetState) {
          case ClinicalState::NORMAL:      Serial.print(F("NORMAL")); break;
          case ClinicalState::TACHYCARDIA: Serial.print(F("TAQUI")); break;
          case ClinicalState::BRADYCARDIA: Serial.print(F("BRADI")); break;
        }
        unsigned long elapsed = now - stateChangeTime;
        unsigned long remaining = Timing::STATE_STABILITY - elapsed;
        Serial.print(F(" ("));
        Serial.print(remaining / 1000);
        Serial.print(F("s)"));
      }
      
      Serial.println();
      
      Serial.print(F("│ WiFi:         "));
      Serial.println(WiFi.status() == WL_CONNECTED ? F("Conectado") : F("Desconectado"));
      
      Serial.print(F("│ MQTT:         "));
      Serial.println(mqtt.isConnected() ? F("Conectado") : F("Desconectado"));
      
      Serial.println(F("└─────────────────────────────────────┘"));
    }
  }

public:
  void begin() {
    Serial.begin(115200);
    delay(1000);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    pinMode(Pins::POT, INPUT);
    pinMode(Pins::LO_PLUS, INPUT_PULLUP);
    pinMode(Pins::LO_MINUS, INPUT_PULLUP);
    pinMode(Pins::BUZZER, OUTPUT);
    pinMode(Pins::LED_WIFI, OUTPUT);
    pinMode(Pins::LED_PROCESS, OUTPUT);
    pinMode(Pins::LED_ALERT, OUTPUT);

    testLEDs();
    
    Serial.println(F("\n╔════════════════════════════════════╗"));
    Serial.println(F("║   MONITOR CARDÍACO ESP32 v4.0     ║"));
    Serial.println(F("║        com MQTT Support            ║"));
    Serial.println(F("╚════════════════════════════════════╝"));
    
    // Conecta WiFi
    if (!mqtt.connectWiFi()) {
      Serial.println(F("\n⚠️  Continuando sem WiFi..."));
    }
    
    // Conecta MQTT
    if (WiFi.status() == WL_CONNECTED) {
      mqtt.connectMQTT();
    }
    
    Serial.println(F("\n📋 Limites clínicos:"));
    Serial.print(F("  • Bradicardia: < ")); Serial.print(Clinical::BRADY_THRESHOLD); Serial.println(F(" BPM"));
    Serial.print(F("  • Normal: ")); Serial.print(Clinical::BRADY_THRESHOLD); 
    Serial.print(F(" - ")); Serial.print(Clinical::TACHY_THRESHOLD); Serial.println(F(" BPM"));
    Serial.print(F("  • Taquicardia: > ")); Serial.print(Clinical::TACHY_THRESHOLD); Serial.println(F(" BPM"));
    
    Serial.println(F("\n📡 Tópicos MQTT:"));
    Serial.print(F("  • BPM: ")); Serial.println(Config::TOPIC_BPM);
    Serial.print(F("  • Estado: ")); Serial.println(Config::TOPIC_STATE);
    Serial.print(F("  • Alertas: ")); Serial.println(Config::TOPIC_ALERT);
    Serial.print(F("  • Status: ")); Serial.println(Config::TOPIC_STATUS);
    
    Serial.println(F("\n▶ Sistema iniciado!\n"));
  }

  void update() {
    // Mantém conexão MQTT
    mqtt.loop();
    
    if (checkLeadsOff()) {
      return;
    }

    currentBPM = readAndFilterBPM();
    updateHeartbeatLED();
    evaluateClinicalState();
    publishData();
    publishToMQTT();
    updateWifiLED();
  }
};

// ============================================================================
// INSTÂNCIA GLOBAL
// ============================================================================
CardiacMonitor monitor;

// ============================================================================
// SETUP E LOOP
// ============================================================================
void setup() {
  monitor.begin();
}

void loop() {
  monitor.update();
}
