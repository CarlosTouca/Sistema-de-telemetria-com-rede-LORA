#include "Arduino.h"
#include "LoRaWan_APP.h"
#include "driver/board-config.h"

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// OLED
// =====================================================
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RESET 21
#define VEXT_PIN 36

Adafruit_SSD1306 oled(
  OLED_WIDTH,
  OLED_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledFuncionando = false;

// =====================================================
// WIFI
// =====================================================
const char* NOME_REDE = "CENTRAL_LORA";

WebServer servidor(80);

// =====================================================
// LORA
// =====================================================
#define RF_FREQUENCY 915000000
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 9
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 12
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define TEMPO_OFFLINE 7500

char rxpacket[150];

static RadioEvents_t RadioEvents;

volatile bool pacoteRecebido = false;
volatile bool radioRecebendo = false;
volatile int16_t ultimoRSSI = 0;

// =====================================================
// DADOS DOS TRANSMISSORES
// =====================================================
struct DadosESP {
  const char* id;
  bool recebeu;
  unsigned long sequencia;
  float temperatura;
  float umidade;
  float bateria;
  int rssi;
  unsigned long ultimoPacote;
};

DadosESP dispositivos[3] = {
  {"ESP_01", false, 0, 0.0, 0.0, 0.0, 0, 0},
  {"ESP_02", false, 0, 0.0, 0.0, 0.0, 0, 0},
  {"ESP_03", false, 0, 0.0, 0.0, 0.0, 0, 0}
};

// =====================================================
// OLED MOSTRA SOMENTE O IP
// =====================================================
void mostrarSomenteIP(String ip) {
  if (!oledFuncionando) {
    return;
  }

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);

  oled.setCursor(20, 28);
  oled.println(ip);

  oled.display();
}

// =====================================================
// CALLBACKS DO RADIO
// =====================================================
void OnRxDone(
  uint8_t* payload,
  uint16_t size,
  int16_t rssi,
  int8_t snr
) {
  radioRecebendo = false;

  if (size >= sizeof(rxpacket)) {
    size = sizeof(rxpacket) - 1;
  }

  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';

  ultimoRSSI = rssi;
  pacoteRecebido = true;

  Radio.Sleep();
}

void OnRxTimeout(void) {
  radioRecebendo = false;
}

void OnRxError(void) {
  radioRecebendo = false;
}

// =====================================================
// EXTRAI CAMPOS
// Exemplo:
// ID=ESP_01;SEQ=1;TEMP=25.4;UMID=61.2;BAT=0.00
// =====================================================
String obterCampo(String pacote, String nomeCampo) {
  int inicio = pacote.indexOf(nomeCampo);

  if (inicio < 0) {
    return "";
  }

  inicio += nomeCampo.length();

  int fim = pacote.indexOf(';', inicio);

  if (fim < 0) {
    fim = pacote.length();
  }

  return pacote.substring(inicio, fim);
}

// =====================================================
// LOCALIZA O TRANSMISSOR
// =====================================================
int localizarDispositivo(String id) {
  for (int i = 0; i < 3; i++) {
    if (id == dispositivos[i].id) {
      return i;
    }
  }

  return -1;
}

// =====================================================
// PROCESSA PACOTE SEM ALTERAR O OLED
// =====================================================
void processarPacote() {
  String pacote = String(rxpacket);

  String id = obterCampo(pacote, "ID=");
  String seq = obterCampo(pacote, "SEQ=");
  String temp = obterCampo(pacote, "TEMP=");
  String umid = obterCampo(pacote, "UMID=");
  String bat = obterCampo(pacote, "BAT=");

  int indice = localizarDispositivo(id);

  if (indice < 0) {
    return;
  }

  dispositivos[indice].recebeu = true;
  dispositivos[indice].sequencia = seq.toInt();
  dispositivos[indice].temperatura = temp.toFloat();
  dispositivos[indice].umidade = umid.toFloat();
  dispositivos[indice].bateria = bat.toFloat();
  dispositivos[indice].rssi = ultimoRSSI;
  dispositivos[indice].ultimoPacote = millis();
}

// =====================================================
// RECEPÇÃO LORA
// =====================================================
void iniciarRecepcao() {
  if (radioRecebendo) {
    return;
  }

  radioRecebendo = true;
  Radio.Rx(0);
}

// =====================================================
// JSON DE CADA DISPOSITIVO
// =====================================================
String gerarJSONDispositivo(int indice) {
  DadosESP& dispositivo = dispositivos[indice];

  bool online =
    dispositivo.recebeu &&
    (millis() - dispositivo.ultimoPacote <= TEMPO_OFFLINE);

  String json = "{";

  json += "\"id\":\"";
  json += dispositivo.id;
  json += "\",";

  json += "\"online\":";
  json += online ? "true" : "false";
  json += ",";

  json += "\"seq\":";
  json += String(dispositivo.sequencia);
  json += ",";

  json += "\"temp\":";
  json += String(dispositivo.temperatura, 1);
  json += ",";

  json += "\"umid\":";
  json += String(dispositivo.umidade, 1);
  json += ",";

  json += "\"bat\":";
  json += String(dispositivo.bateria, 2);
  json += ",";

  json += "\"rssi\":";
  json += String(dispositivo.rssi);

  json += "}";

  return json;
}

// =====================================================
// DASHBOARD HTML
// =====================================================
String paginaHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">

<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>Central LoRa</title>

<style>
* {
  box-sizing: border-box;
  font-family: Arial, sans-serif;
}

body {
  margin: 0;
  padding: 20px;
  background: #101820;
  color: white;
}

h1 {
  text-align: center;
  color: #00d4ff;
}

.subtitulo {
  text-align: center;
  color: #bbbbbb;
  margin-bottom: 25px;
}

.dashboard {
  max-width: 1100px;
  margin: auto;
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 20px;
}

.card {
  padding: 20px;
  border-radius: 15px;
  background: #202b35;
  border: 3px solid #777;
  box-shadow: 0 0 15px #000;
}

.card.online {
  border-color: #00e676;
}

.card.offline {
  border-color: #ff1744;
}

.card h2 {
  margin-top: 0;
}

.status {
  font-size: 20px;
  font-weight: bold;
  margin-bottom: 15px;
}

.online .status {
  color: #00e676;
}

.offline .status {
  color: #ff1744;
}

.dado {
  margin: 10px 0;
  font-size: 18px;
}

.rodape {
  text-align: center;
  margin-top: 30px;
  color: #999;
}
</style>
</head>

<body>

<h1>Dashboard Central LoRa</h1>

<div class="subtitulo">
  Atualização automática a cada 1 segundo
</div>

<div class="dashboard">

  <div id="card-ESP_01" class="card offline">
    <h2>ESP_01</h2>
    <div id="status-ESP_01" class="status">OFFLINE</div>
    <div id="dados-ESP_01">Aguardando dados...</div>
  </div>

  <div id="card-ESP_02" class="card offline">
    <h2>ESP_02</h2>
    <div id="status-ESP_02" class="status">OFFLINE</div>
    <div id="dados-ESP_02">Aguardando dados...</div>
  </div>

  <div id="card-ESP_03" class="card offline">
    <h2>ESP_03</h2>
    <div id="status-ESP_03" class="status">OFFLINE</div>
    <div id="dados-ESP_03">Aguardando dados...</div>
  </div>

</div>

<div class="rodape">
  Rede estrela LoRa — Central Heltec V3
</div>

<script>
function atualizarDispositivo(dados) {
  const id = dados.id;

  const card = document.getElementById("card-" + id);
  const status = document.getElementById("status-" + id);
  const painel = document.getElementById("dados-" + id);

  if (!card || !status || !painel) {
    return;
  }

  if (dados.online) {
    card.className = "card online";
    status.innerHTML = "ONLINE";
  } else {
    card.className = "card offline";
    status.innerHTML = "OFFLINE";
  }

  painel.innerHTML =
    "<div class='dado'>Sequência: " + dados.seq + "</div>" +
    "<div class='dado'>Temperatura: " + dados.temp + " °C</div>" +
    "<div class='dado'>Umidade: " + dados.umid + " %</div>" +
    "<div class='dado'>Bateria: " + dados.bat + " V</div>" +
    "<div class='dado'>RSSI: " + dados.rssi + " dBm</div>";
}

async function atualizarDashboard() {
  try {
    const resposta = await fetch("/api/dados");
    const dados = await resposta.json();

    dados.dispositivos.forEach(atualizarDispositivo);
  } catch (erro) {
    console.log("Erro ao atualizar dashboard:", erro);
  }
}

atualizarDashboard();
setInterval(atualizarDashboard, 1000);
</script>

</body>
</html>
)rawliteral";

  return html;
}

// =====================================================
// ROTAS DO SERVIDOR
// =====================================================
void enviarDashboard() {
  servidor.send(
    200,
    "text/html",
    paginaHTML()
  );
}

void enviarDadosAPI() {
  String json = "{\"dispositivos\":[";

  json += gerarJSONDispositivo(0);
  json += ",";
  json += gerarJSONDispositivo(1);
  json += ",";
  json += gerarJSONDispositivo(2);

  json += "]}";

  servidor.send(
    200,
    "application/json",
    json
  );
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Mcu.begin(
    HELTEC_BOARD,
    SLOW_CLK_TPYE
  );

  // Alimentação do OLED
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(200);

  // Reset do OLED
  pinMode(OLED_RESET, OUTPUT);
  digitalWrite(OLED_RESET, LOW);
  delay(20);
  digitalWrite(OLED_RESET, HIGH);
  delay(50);

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  oledFuncionando = oled.begin(
    SSD1306_SWITCHCAPVCC,
    0x3C
  );

  // Configuração do rádio
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetRxConfig(
    MODEM_LORA,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    0,
    LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT,
    LORA_FIX_LENGTH_PAYLOAD_ON,
    0,
    true,
    0,
    0,
    LORA_IQ_INVERSION_ON,
    true
  );

  // Cria rede Wi-Fi aberta
  WiFi.mode(WIFI_AP);
  WiFi.softAP(NOME_REDE);

  // Configura dashboard
  servidor.on(
    "/",
    HTTP_GET,
    enviarDashboard
  );

  servidor.on(
    "/api/dados",
    HTTP_GET,
    enviarDadosAPI
  );

  servidor.begin();

  iniciarRecepcao();

  // OLED mostra somente o IP
  mostrarSomenteIP(
    WiFi.softAPIP().toString()
  );

  // Monitor Serial mostra somente o IP
  Serial.println(
    "http://" + WiFi.softAPIP().toString()
  );
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  Radio.IrqProcess();

  servidor.handleClient();

  if (pacoteRecebido) {
    pacoteRecebido = false;
    processarPacote();
  }

  if (!radioRecebendo) {
    iniciarRecepcao();
  }

  delay(2);
}