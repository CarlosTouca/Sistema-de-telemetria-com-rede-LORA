#include "DHT.h"

// ===============================
// CONFIGURAÇÃO DO DHT22
// ===============================
#define DHTPIN 26
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  delay(1000);
}

void loop() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  Serial.println();
  //Serial.println("================================");
  Serial.println("LEITURA DHT22");
 // Serial.println("================================");

  if (isnan(umidade) || isnan(temperatura)) {
    Serial.println("Erro na leitura do sensor.");
  } else {
    Serial.printf("Temperatura : %6.2f °C\n", temperatura);
    Serial.printf("Umidade     : %6.2f %%\n", umidade);
  }

  //Serial.println("================================");
    
  //Aqui podemos mudar o tempo em que a informação é mostrada na tela 
  delay(1500);
}