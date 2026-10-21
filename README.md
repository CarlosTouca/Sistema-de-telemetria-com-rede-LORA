# Sistema de telemetria com rede LoRa

Projeto experimental de telemetria com ESP32, sensores ambientais, comunicação LoRa, Wi-Fi e dashboard web.

O objetivo é instalar dispositivos em campo para coletar informações como temperatura, umidade do ar e, futuramente, umidade do solo por meio de um infiltrômetro. Os dados são enviados pela rede LoRa até uma central, que disponibiliza as informações em uma dashboard.

## Evolução da arquitetura

O projeto está sendo desenvolvido em etapas:

```text
Teste do sensor → Comunicação LoRa → Rede estrela → Central com dashboard → Rede mesh
```

### 1. Teste do sensor

O ESP32 lê o DHT22 e mostra os valores no Monitor Serial. Essa etapa confirma se o sensor e a ligação elétrica estão funcionando.

### 2. Comunicação LoRa

Um ESP32 transmite os dados e outro recebe. Essa etapa serve para testar frequência, antena, distância, RSSI, estabilidade e perda de pacotes.

### 3. Rede estrela

Vários ESP32 transmissores enviam diretamente para uma central LoRa. Os nós de campo não encaminham dados uns pelos outros.

```text
ESP_01 ─┐
ESP_02 ─┼──> Central LoRa ──> Dashboard
ESP_03 ─┘
```

### 4. Rede mesh planejada

Na versão mesh, qualquer ESP poderá gerar uma leitura ou retransmitir dados de outro dispositivo. Assim, a mensagem não dependerá de um caminho fixo.

Exemplo de rota possível:

```text
ESP_04 → ESP_02 → ESP_01 → ESP_05 (dashboard)
```

Essa rota é apenas um exemplo. O próprio algoritmo deverá analisar os nós disponíveis e escolher o melhor caminho considerando a qualidade do sinal, a distância estimada, a perda de pacotes e a disponibilidade de cada ESP.

## Como a comunicação funciona

Cada dispositivo possui uma identificação, como `ESP_01`, `ESP_02` ou `ESP_03`, e envia os dados do sensor em um pacote.

Exemplo de pacote:

```text
ID=ESP_01;SEQ=1;TEMP=25.4;UMID=61.2;BAT=0.00
```

| Campo | Função |
|---|---|
| `ID` | Identifica o ESP que originou a leitura. |
| `SEQ` | Número sequencial do pacote. |
| `TEMP` | Temperatura medida. |
| `UMID` | Umidade relativa do ar. |
| `BAT` | Campo reservado para a bateria. |

Na mesh, o pacote também deverá carregar informações sobre a origem, os nós pelos quais passou e o destino final. Isso permitirá que a central saiba de onde o dado veio e que a rede evite repetições ou loops.

## Central e dashboard

A central LoRa é responsável por:

- receber os dados enviados pelos ESP32;
- identificar a origem de cada leitura;
- registrar temperatura, umidade, sequência e qualidade do sinal;
- acompanhar quais dispositivos estão ativos;
- criar uma rede Wi-Fi local;
- disponibilizar a dashboard para computador ou celular.

O LoRa transporta os dados entre os dispositivos. O Wi-Fi da central serve apenas para acessar a dashboard.

## Estrutura do repositório

| Pasta | Descrição |
|---|---|
| `Codigo_DHT22_Apenas_Serial_Esp32_Normal` | Teste básico do DHT22 pelo Monitor Serial. |
| `Codigo_Lora_Transmissoras` | Código dos ESP32 que leem o sensor e enviam dados por LoRa. |
| `Codigo_Da_Lora_Central` | Código da central LoRa e da dashboard local. |
| `Codigo_dht22_WebSocket_Esp32_Normal` | Teste separado do DHT22 usando Wi-Fi e WebSocket. |

### `Codigo_DHT22_Apenas_Serial_Esp32_Normal`

Contém o teste inicial do sensor. Não utiliza LoRa nem dashboard.

### `Codigo_Lora_Transmissoras`

Contém o código-base dos nós de campo. Cada placa lê o DHT22, monta um pacote com sua identificação e transmite a leitura periodicamente.

Para usar o mesmo código em outra placa, basta alterar a identificação do dispositivo no próprio sketch, por exemplo:

```cpp
#define DEVICE_ID "ESP_01"
```

### `Codigo_Da_Lora_Central`

Contém o código do ESP32 que recebe os pacotes LoRa e hospeda a dashboard local.

### `Codigo_dht22_WebSocket_Esp32_Normal`

É um experimento independente de comunicação por Wi-Fi. Ele demonstra a atualização dos dados do DHT22 em uma página web usando WebSocket.

## Como testar

1. Testar o DHT22 usando o código serial.
2. Gravar o código transmissor em um ou mais ESP32.
3. Alterar o `DEVICE_ID` de cada placa.
4. Gravar o código da central em outro ESP32.
5. Verificar os pacotes no Monitor Serial.
6. Conectar ao Wi-Fi da central e abrir a dashboard no navegador.
7. Repetir o teste em diferentes distâncias e posições.

Cada pasta representa uma etapa diferente. Os sketches são gravados separadamente pela Arduino IDE.

## Configurações importantes

- Os testes LoRa utilizam a faixa de 915 MHz.
- Todas as placas do mesmo teste precisam usar parâmetros de rádio compatíveis.
- O código serial do DHT22 utiliza GPIO 26.
- O código dos transmissores utiliza GPIO 4.
- A ligação física deve ser conferida de acordo com o sketch gravado.
- Alguns arquivos históricos não possuem a extensão `.ino`, embora contenham código Arduino.

## Próximas etapas

- descoberta automática de novos dispositivos;
- escolha automática do melhor próximo salto;
- confirmação da entrega da mensagem até a central;
- controle de pacotes duplicados e loops;
- inclusão do infiltrômetro e de outros sensores;
- envio dos dados da dashboard para um servidor.

## Status

O projeto está em fase de prototipagem. As etapas de leitura do sensor, transmissão LoRa, central e dashboard estão sendo usadas como base para a implementação da mesh dinâmica.

## Autoria

Projeto de desenvolvimento de uma rede de telemetria LoRa com ESP32.
