# PCRM — Infraestrutura de telecomunicações baseada em rede LoRa

Projeto técnico autoral de **Anthony**, estudante de **Análise e Desenvolvimento de Sistemas (ADS) no IFPE**, voltado ao estudo prático de comunicação sem fio, programação embarcada em C++ e redes de dispositivos baseadas em LoRa.

O repositório registra a evolução de um protocolo próprio para ESP32/Heltec com rádio SX1262: primeiro um enlace ponto a ponto, depois uma central em estrela e, por fim, uma rede mesh capaz de descobrir caminhos, encaminhar pacotes e confirmar a entrega de dados.

![Infraestrutura técnica com nós ESP32, rádio LoRa, gateway e dashboard](assets/pcrm-infraestrutura-lora-hero.png)

> O projeto não é uma implementação de LoRaWAN. É uma experiência acadêmica e prática com LoRa em nível de rádio, mais um protocolo de aplicação e roteamento desenvolvido para o PCRM.

## Sumário

- [Visão geral](#visão-geral)
- [O que é LoRa](#o-que-é-lora)
- [LoRa, LoRaWAN e rede LoRa](#lora-lorawan-e-rede-lora)
- [Frequência e parâmetros de rádio](#frequência-e-parâmetros-de-rádio)
- [Comparação com outras comunicações](#comparação-com-outras-comunicações)
- [Evolução: ponto a ponto, estrela e mesh](#evolução-ponto-a-ponto-estrela-e-mesh)
- [Arquitetura do PCRM](#arquitetura-do-pcrm)
- [Formato dos pacotes](#formato-dos-pacotes)
- [Confirmação de entrega e ACK](#confirmação-de-entrega-e-ack)
- [Descoberta e seleção de rotas](#descoberta-e-seleção-de-rotas)
- [Saltos, camadas, tamanho do pacote e interferência](#saltos-camadas-tamanho-do-pacote-e-interferência)
- [Lógica do código](#lógica-do-código)
- [Dashboard e gateway](#dashboard-e-gateway)
- [Hardware e pinagem](#hardware-e-pinagem)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Como executar](#como-executar)
- [Metodologia de testes](#metodologia-de-testes)
- [Estado atual e limitações](#estado-atual-e-limitações)
- [Próximas etapas](#próximas-etapas)

---

## Visão geral

O PCRM — **Protocolo de Comunicação e Roteamento Mesh** — nasceu de uma pergunta simples: como fazer vários dispositivos LoRa trocarem dados sem deixar o caminho entre eles fixo no código?

Em vez de definir manualmente que `ESP_01` sempre envia para `ESP_02`, que `ESP_02` sempre envia para `ESP_03` e assim por diante, a proposta é permitir que qualquer nó:

- produza um dado;
- descubra quais vizinhos estão disponíveis;
- participe da descoberta de uma rota;
- encaminhe mensagens de outros nós;
- descarte pacotes repetidos;
- evite loops usando caminho e TTL;
- selecione uma rota com base na qualidade dos enlaces;
- confirme que o destino final recebeu o dado.

O objetivo principal é estudar a infraestrutura de comunicação: rádio, enlace, pacote, protocolo, roteamento, confiabilidade, temporização, filas, dashboard e integração entre hardware e software.

![Caminho da informação desde um nó até a dashboard](assets/pcrm-dashboard.svg)

### O que este projeto demonstra

1. Leitura de um sensor conectado ao ESP32.
2. Transmissão direta por LoRa.
3. Medição de RSSI, SNR, latência e perda de pacotes.
4. Encaminhamento por múltiplos saltos.
5. Descoberta automática de nós e rotas.
6. Confirmação de entrega com identificador de pacote.
7. Gateway LoRa com servidor HTTP e dashboard local.
8. Organização de um sistema embarcado em C++ mesmo quando a versão final é mantida em um único arquivo `.ino`.

---

## O que é LoRa?

**LoRa** é uma tecnologia de modulação de rádio baseada em **Chirp Spread Spectrum (CSS)**. A modulação é a forma como os bits são transformados em um sinal que pode viajar pelo ar e ser recuperado pelo receptor.

Um chirp é um sinal cuja frequência varia continuamente durante um símbolo. Na prática, o rádio espalha a informação em uma faixa de frequência e o receptor usa a característica desse sinal para demodular a mensagem mesmo quando existe ruído ou o sinal chega muito fraco.

A vantagem de LoRa não é simplesmente “ter mais potência”. O ganho vem de uma combinação de:

- modulação CSS;
- alta sensibilidade do receptor;
- largura de banda configurável;
- espalhamento configurável pelo `Spreading Factor`;
- correção de erros pelo `Coding Rate`;
- operação em frequências sub-GHz adequadas ao cenário e à regulamentação local.

Essa combinação favorece mensagens pequenas, baixo consumo e enlaces de maior alcance relativo. Em troca, a taxa de dados é baixa e a mensagem pode permanecer mais tempo ocupando o canal.

![Representação conceitual da modulação LoRa e de seus parâmetros](assets/pcrm-lora-fisico.svg)

### LoRa não é Wi-Fi mais lento

LoRa e Wi-Fi resolvem problemas diferentes. Wi-Fi foi projetado para alta taxa de dados e acesso a redes IP. LoRa foi projetada para enviar pequenas mensagens a uma distância maior, com pouca informação por transmissão.

Uma leitura como:

```text
temperatura=25.4;umidade=61.2
```

é adequada para LoRa. Um vídeo, uma imagem ou um arquivo grande não é o objetivo desse rádio.

### O que LoRa não define sozinha

LoRa define principalmente a camada física do enlace. Ela não determina, por si só:

- como um dispositivo recebe um endereço;
- como uma rota é descoberta;
- como um pacote é confirmado;
- como mensagens duplicadas são descartadas;
- como um gateway conversa com uma dashboard;
- como a aplicação interpreta os dados.

Essas responsabilidades precisam ser resolvidas por um protocolo de rede e/ou de aplicação. No PCRM, elas são implementadas pelo software C++ executado nos ESP32.

---

## LoRa, LoRaWAN e rede LoRa

Os nomes são parecidos, mas representam camadas diferentes.

| Termo | Significado | Papel no PCRM |
|---|---|---|
| **LoRa** | Modulação de rádio baseada em CSS. | É o transporte físico usado pelos SX1262. |
| **Rede LoRa** | Conjunto de dispositivos que trocam dados usando LoRa. | É a rede mesh que o projeto está construindo. |
| **LoRaWAN** | Protocolo padronizado de rede com dispositivos, gateways, servidores e regras próprias. | Não é o protocolo implementado neste repositório. |
| **PCRM** | Protocolo autoral de aplicação, descoberta, roteamento e confirmação. | É a lógica criada para este projeto. |

Portanto, quando este README diz “rede LoRa”, significa uma rede de nós que usa a camada física LoRa e executa o protocolo próprio do PCRM. Não significa automaticamente LoRaWAN.

No LoRaWAN tradicional, o gateway normalmente encaminha mensagens para um servidor de rede por IP, formando uma arquitetura chamada *star-of-stars*. No PCRM, os próprios ESP32 participam do encaminhamento LoRa e podem formar uma mesh de múltiplos saltos.

![Separação entre a camada física LoRa, o protocolo PCRM e a aplicação](assets/pcrm-camadas.svg)

---

## Frequência e parâmetros de rádio

LoRa não é uma frequência única. A frequência depende do módulo, da região, do plano de canais e da configuração usada no projeto.

Nesta base, o rádio está configurado para **915 MHz**:

```cpp
constexpr float FREQUENCIA = 915.0;
```

Esse valor é uma configuração de referência do protótipo. A frequência, a potência, os canais e a antena devem ser compatíveis com o módulo utilizado e com as regras aplicáveis no local de operação.

### Parâmetros atuais do protótipo monolítico

| Parâmetro | Valor | Efeito principal |
|---|---:|---|
| Frequência | `915.0 MHz` | Define a portadora usada pelo rádio. |
| Largura de banda | `125 kHz` | Afeta taxa, robustez e ocupação espectral. |
| Spreading Factor | `SF9` | Aumenta robustez e tempo no ar quando elevado. |
| Coding Rate | `4/5` | Adiciona redundância para correção de erros. |
| Preâmbulo | `12 símbolos` | Ajuda o receptor a sincronizar. |
| Potência de transmissão | `17 dBm` | Limita a potência de saída configurada. |
| Tamanho máximo no protocolo | `240 bytes` | Protege a fila e o rádio contra mensagens grandes. |

### RSSI e SNR

O **RSSI** indica a potência recebida, normalmente em dBm. Como a escala é negativa, `-60 dBm` é mais forte que `-80 dBm`.

O **SNR** indica a relação entre sinal e ruído. Quanto melhor a margem do sinal em relação ao ruído, maior a probabilidade de o receptor decodificar o pacote.

Esses indicadores ajudam a escolher uma rota, mas não são um medidor perfeito de distância. Um nó próximo pode apresentar RSSI ruim por causa de obstáculos, orientação da antena, reflexão, ruído ou alimentação inadequada. Por isso, o PCRM combina qualidade do sinal com quantidade de saltos, estabilidade e perdas observadas.

---

## Comparação com outras comunicações

Não existe uma tecnologia de rádio “melhor” em todos os casos. A escolha depende de alcance, taxa de dados, consumo, infraestrutura e tipo de mensagem.

| Tecnologia | Alcance relativo | Taxa de dados | Consumo relativo | Cenário típico |
|---|---|---|---|---|
| **Wi-Fi** | baixo a médio | alto | alto | Internet, arquivos, vídeo e dashboard local |
| **Bluetooth/BLE** | baixo | baixo a médio | baixo | Comunicação próxima e acessórios |
| **Zigbee/802.15.4** | baixo a médio | médio | baixo | Automação e redes locais de baixa potência |
| **FSK/OOK** | variável | baixo a médio | baixo a médio | Enlaces simples e telemetria dedicada |
| **LoRa** | médio a muito alto, conforme instalação | baixo | baixo a médio | Mensagens pequenas em distâncias maiores |
| **LoRaWAN** | depende dos gateways | baixo | baixo no dispositivo | IoT padronizada com infraestrutura de servidor |

LoRa foi escolhida para este estudo porque permite trabalhar diretamente com os parâmetros do rádio e com a construção do protocolo: enlace, vizinhança, rota, encaminhamento, confirmação e falha.

---

## Evolução: ponto a ponto, estrela e mesh

O projeto não começou com a mesh pronta. Cada arquitetura foi uma etapa de validação de um problema específico.

### 1. Comunicação ponto a ponto

Na comunicação **ponto a ponto**, um dispositivo transmite diretamente para outro. Não existe um relay entre eles.

Esta foi a primeira etapa real de testes porque permite separar o rádio do restante do sistema. Antes de discutir rotas, é necessário saber se duas placas conseguem trocar mensagens com estabilidade.

Foram observados ou devem ser observados:

- distância;
- antena;
- frequência;
- potência;
- RSSI;
- SNR;
- tempo de resposta;
- perda de pacotes;
- comportamento com obstáculos;
- diferença entre o pacote enviado e o pacote recebido.

O alcance divulgado pelo projeto deve ser o alcance **medido** no teste, com a antena, potência e configuração registradas. O número teórico do fabricante não substitui a medição do enlace real; por isso, o valor final deve ser preenchido depois da campanha de testes, sem inventar uma distância.

![Teste de um enlace LoRa ponto a ponto entre dois ESP32](assets/pcrm-ponto-a-ponto.svg)

### 2. Rede estrela

Na rede estrela, vários nós enviam diretamente para uma central.

```text
Nó A ─┐
Nó B ─┼──> Central LoRa ──> Dashboard
Nó C ─┘
```

A estrela simplifica o protocolo, mas cria uma dependência: todos os nós precisam alcançar a central diretamente. Se um nó ficar fora da cobertura, ele não possui um caminho alternativo.

### 3. Rede mesh

Na mesh, um nó pode encaminhar pacotes de outro nó. O caminho pode ter vários saltos e ser diferente para cada mensagem ou descoberta de rota.

```text
Nó A → Nó C → Nó E → Gateway
Nó B → Nó D ─────────> Gateway
Nó E ─────────────────> Gateway
```

O exemplo acima não é uma sequência fixa. O objetivo do PCRM é aprender o caminho em tempo de execução, conforme os nós disponíveis e a qualidade dos enlaces.

![Comparação visual entre ponto a ponto, estrela e mesh](assets/pcrm-topologias.svg)

---

## Arquitetura do PCRM

O PCRM foi pensado para que os nós sejam participantes equivalentes da rede. Qualquer nó pode iniciar um evento, retransmitir um pacote ou participar de uma resposta.

### O que não fica definido manualmente

O firmware não deve depender de uma tabela escrita assim:

```text
ESP_01 sempre envia para ESP_02
ESP_02 sempre envia para ESP_04
ESP_04 sempre envia para ESP_05
```

Esse modelo seria uma cadeia fixa. Se um nó desligasse, o caminho quebraria mesmo que existisse outra placa disponível.

No PCRM, estas informações são aprendidas:

- próximo salto;
- salto anterior para confirmação;
- caminho completo;
- custo da rota;
- validade da rota;
- quantidade de saltos;
- vizinhos observados;
- presença da dashboard.

### Identidade automática

Cada placa gera uma identidade interna a partir do identificador de hardware do ESP32:

```text
N_XXXXXXXX
```

Esse identificador não precisa ser digitado manualmente em cada placa. O nome visual mostrado na dashboard pode ser associado à identidade real, mas a rede usa a identidade própria do hardware para distinguir os nós.

Na base monolítica, a única decisão de papel feita no código é:

```cpp
constexpr bool EH_DASHBOARD = false;
```

- `false`: placa participa como nó da mesh;
- `true`: placa também funciona como gateway e dashboard.

Mesmo assim, o próximo salto e a rota não são definidos manualmente.

### Entrada de novos nós

Um nó novo deve anunciar a própria presença. Os demais dispositivos podem observar esse anúncio e a dashboard pode criar um registro para a identidade ainda desconhecida.

O cadastro lógico pode guardar:

- identidade do nó;
- último contato;
- RSSI e SNR;
- quantidade de saltos;
- estado online/offline;
- sensores informados;
- última telemetria recebida.

Assim, adicionar uma placa à rede não precisa significar editar todos os firmwares existentes.

![Descoberta de rota e escolha automática do caminho](assets/pcrm-roteamento.svg)

---

## Formato dos pacotes

O PCRM usa mensagens textuais com campos separados por `;`. Esse formato facilita a inspeção pelo Monitor Serial durante a fase de desenvolvimento.

Exemplo conceitual:

```text
TIPO=DADO;ORIGEM=N_A1B2C3D4;DESTINO=N_F5E6D7C8;ID_DADO=DADO-N_A1B2C3D4-4;CAMINHO=N_A1B2C3D4>N_11223344>N_F5E6D7C8;SALTO_ALVO=N_11223344;TTL=8;TEMP=25.40;UMID_AR=61.20;EVENTO=NORMAL
```

### Campos principais

| Campo | Função |
|---|---|
| `TIPO` | Define a operação: dado, anúncio, busca de rota, resposta ou confirmação. |
| `ORIGEM` | Identifica o nó que criou a mensagem. |
| `DESTINO` | Identifica o nó final esperado. |
| `ID_DADO` | Identifica aquela mensagem específica e permite confirmar o pacote correto. |
| `CAMINHO` | Registra a sequência de nós que a mensagem percorreu ou deverá percorrer. |
| `SALTO_ALVO` | Diz qual vizinho deve processar a mensagem naquele momento. |
| `TTL` | Limita o número de encaminhamentos e evita loops indefinidos. |
| `TEMP` | Valor de temperatura da aplicação. |
| `UMID_AR` | Valor de umidade do ar da aplicação. |
| `EVENTO` | Estado ou evento produzido pelo nó. |

### Estados em português

O protocolo pode expor estados legíveis durante os testes:

```text
ROTA_PROCURANDO
ROTA_ENCONTRADA
SINAL_FRACO
SINAL_PERDIDO
CONFIRMACAO_RECEBIDA
ENTREGUE
TEMPO_ESGOTADO
ROTA_NAO_ENCONTRADA
PACOTE_DUPLICADO
TTL_ESGOTADO
PACOTE_INVALIDO
```

Os nomes `BUSCA_ROTA` e `ROTA_ENCONTRADA` são a nomenclatura em português usada pelo PCRM para o conceito que em outros estudos pode aparecer como RREQ/RREP. A referência conceitual é útil, mas a implementação do projeto é própria.

---

## Confirmação de entrega e ACK

**ACK** é a abreviação de *Acknowledgement*, que significa confirmação de recebimento.

No desenho atual, o PCRM separa duas ideias:

1. **Confirmação de presença:** o nó confirma que continua alcançável.
2. **Confirmação final:** o destino confirma que recebeu o dado completo.

### Como o dado percorre a rede

1. O nó de origem cria um `ID_DADO` único.
2. A origem encontra ou inicia uma busca de rota.
3. O pacote é enviado ao próximo salto.
4. Cada relay verifica `SALTO_ALVO`, `CAMINHO`, `TTL` e duplicidade.
5. O relay encaminha o pacote sem criar um novo dado.
6. A dashboard recebe o pacote quando é o destino final.
7. A dashboard envia `CONFIRMACAO_FINAL` pelo caminho inverso.
8. A origem libera o pacote pendente quando encontra o mesmo `ID_DADO` no ACK.

O relay não precisa devolver um ACK completo à origem a cada salto. Essa decisão reduz o número de transmissões e evita o comportamento lento de enviar, esperar, reenviar ao mesmo nó e só então continuar.

Se a confirmação final não chegar dentro do tempo configurado, a origem invalida a rota, tenta uma nova descoberta e pode retransmitir o dado dentro do limite de tentativas.

![Fluxo do pacote de dados e da confirmação final](assets/pcrm-pacote-ack.svg)

### Timeout e retransmissão

Na versão monolítica:

```cpp
constexpr unsigned long TIMEOUT_ACK_DADO_MS = 15000UL;
constexpr uint8_t MAX_TENTATIVAS_DADO = 2;
```

Esses valores são parâmetros experimentais. Eles não representam uma garantia universal para qualquer instalação. O tempo adequado depende do spreading factor, do número de saltos, do tráfego, do tamanho do pacote e da estabilidade do enlace.

---

## Descoberta e seleção de rotas

Quando um nó não possui uma rota válida até a dashboard, ele inicia uma mensagem de busca. A mensagem pode ser retransmitida pelos nós que ainda não aparecem no caminho.

Um nó intermediário deve:

1. verificar se já viu aquele identificador de busca;
2. verificar se ele próprio já aparece no caminho;
3. medir RSSI e SNR do pacote recebido;
4. somar o custo do enlace ao custo acumulado;
5. reduzir o TTL;
6. encaminhar a busca ou responder quando for o destino.

### RSSI não é distância

Se uma placa recebe:

```text
Nó 2: RSSI = -60 dBm
Nó 3: RSSI = -80 dBm
```

o enlace com o nó 2 está mais forte naquele instante. Isso não prova, sozinho, que o nó 2 está fisicamente mais perto. Obstáculos, reflexões, posição da antena, ruído e interferência podem mudar o RSSI.

Por isso, a decisão de rota não deve usar apenas “o maior RSSI”. Uma rota deve considerar o conjunto:

- qualidade do RSSI;
- SNR;
- perda de pacotes;
- quantidade de saltos;
- estabilidade observada;
- validade temporal;
- possibilidade de o nó encaminhar a mensagem.

### Custo experimental do enlace

O código monolítico normaliza RSSI e SNR e cria uma probabilidade heurística de qualidade:

```text
qualidade_rssi = limitar((RSSI + 120) / 65, 0, 1)
qualidade_snr  = limitar((SNR  + 12)  / 24, 0, 1)
qualidade      = 0,65 × qualidade_rssi + 0,35 × qualidade_snr
custo_enlace   = 1 / qualidade
```

O custo da rota é acumulado a cada salto. Em geral, uma rota com um enlace muito ruim pode perder para uma rota um pouco mais longa, mas estável.

Essa fórmula é uma heurística de protótipo, não uma medição precisa de distância. O próximo refinamento é incluir taxa de entrega real e histórico de falhas no custo.

![Origem, vizinhos, métricas de enlace e rota escolhida](assets/pcrm-roteamento.svg)

---

## Saltos, camadas, tamanho do pacote e interferência

Esta é uma distinção importante: **mais camadas de software** não significam automaticamente mais tempo de rádio.

Uma camada pode apenas organizar o código. Separar `Protocolo`, `Roteamento` e `Dashboard` em funções diferentes não obriga o rádio a transmitir mais bytes.

O que normalmente aumenta o tempo total é:

- quantidade de saltos físicos;
- tamanho do pacote;
- quantidade de confirmações;
- tempo de backoff;
- tentativas de retransmissão;
- valor do spreading factor;
- coding rate;
- colisões com outros nós;
- processamento bloqueante.

Uma aproximação útil é:

```text
tempo_total ≈ soma(tempo_no_ar de cada salto)
              + espera da fila
              + backoff
              + retransmissões
```

### Por que mais saltos demoram mais?

Se um pacote vai diretamente da origem para o gateway, ele ocupa o canal uma vez. Se percorre quatro saltos, o mesmo conteúdo precisa ser transmitido pelo menos quatro vezes, além das esperas entre os nós.

Isso não significa que a mesh é sempre pior. Um enlace direto pode ser impossível ou muito instável; três enlaces curtos e bons podem entregar mais dados do que um enlace longo com muitas perdas. O roteamento precisa equilibrar alcance, estabilidade, saltos e tempo.

### Por que pacotes grandes prejudicam?

Quanto maior o pacote, maior tende a ser o tempo no ar. Com o canal ocupado por mais tempo:

- outros nós precisam esperar;
- aumenta a chance de duas transmissões se sobreporem;
- uma colisão desperdiça mais tempo;
- o retry custa mais;
- a fila cresce;
- a confirmação chega mais tarde.

Por isso, o PCRM usa campos diretos e limita o tamanho máximo do pacote. Em uma futura versão binária, os campos podem ocupar menos bytes, mas a legibilidade durante os testes diminui.

### Half-duplex

O transceptor trabalha de forma half-duplex: o mesmo rádio não transmite e recebe simultaneamente. O firmware precisa alternar entre recepção, transmissão e espera. A fila, o backoff aleatório e o `SALTO_ALVO` ajudam a reduzir transmissões indevidas entre vizinhos.

![Camadas de software e hardware que participam do caminho da mensagem](assets/pcrm-camadas.svg)

---

## Lógica do código

Mesmo quando a versão usada pela Arduino IDE está em um arquivo grande, ela possui responsabilidades distintas.

### Inicialização

No `setup()`, o firmware:

1. inicia o Monitor Serial;
2. calcula a identidade automática do nó;
3. inicializa a OLED;
4. inicializa o DHT22 apenas quando a placa não é dashboard;
5. configura o barramento SPI e o SX1262;
6. inicia o Wi-Fi e o servidor HTTP se `EH_DASHBOARD=true`;
7. começa a anunciar a dashboard ou a presença do nó.

Se o rádio falhar, a dashboard ainda pode iniciar o servidor web e o firmware tenta recuperar o rádio periodicamente. O código não deve ficar preso para sempre em uma tela de inicialização.

### Laço principal

O `loop()` executa tarefas curtas:

```text
atender requisições HTTP
processar pacotes recebidos
responder buscas de rota
ler o sensor quando o intervalo permitir
anunciar presença
criar telemetria
verificar timeouts
processar fila de transmissão
```

O uso de intervalos com `millis()` evita bloquear o programa durante o funcionamento normal. O DHT22 possui intervalo próprio de leitura; a dashboard pode consultar a API a cada segundo sem obrigar o sensor a ser lido a cada segundo.

### Fila de transmissão

As mensagens entram em uma fila com prioridade. Confirmações possuem prioridade maior que telemetria comum, porque liberar um pacote pendente é mais urgente do que iniciar outro envio.

O agendamento também utiliza um pequeno atraso aleatório. Isso reduz a probabilidade de todos os nós transmitirem exatamente no mesmo instante depois de um anúncio ou evento.

### Encaminhamento

Ao receber uma mensagem, o nó não deve simplesmente retransmitir tudo. Ele verifica:

- se o pacote é válido;
- se o `SALTO_ALVO` é ele;
- se o caminho contém o nó;
- se o identificador já foi processado;
- se o `TTL` ainda permite encaminhamento;
- qual é o próximo nó do caminho.

Essa validação evita loops, duplicatas e retransmissões de mensagens destinadas a outro vizinho.

---

## Dashboard e gateway

A dashboard possui duas funções diferentes:

1. **Gateway LoRa:** recebe pacotes pelo SX1262 e confirma a entrega.
2. **Servidor web:** apresenta os dados por HTTP para computador ou celular.

O caminho é:

```text
Sensor → ESP32 → rede mesh LoRa → gateway → Wi-Fi/HTTP → navegador
```

O Wi-Fi não substitui o LoRa entre os nós. Ele serve para o acesso local à interface da placa configurada como dashboard.

### Modos de acesso

Na configuração atual, há duas possibilidades:

- **Ponto de acesso próprio:** deixar `WIFI_LOCAL_SSID` vazio. A dashboard cria a rede `PCRM_DASHBOARD`.
- **Rede local existente:** preencher `WIFI_LOCAL_SSID` e `WIFI_LOCAL_SENHA`. A dashboard tenta entrar nessa rede e exibe o IP obtido.

O servidor usa a porta HTTP 80. A página consulta:

```text
/api/status
/api/nos
```

O navegador atualiza a tabela a cada segundo. Isso atualiza a visualização, mas a frequência real de chegada depende do sensor, do intervalo de telemetria, do rádio e da rota.

![Fluxo entre sensor, mesh LoRa, gateway, Wi-Fi e navegador](assets/pcrm-dashboard.svg)

---

## Hardware e pinagem

Base de referência: **Heltec WiFi LoRa 32 V3**, com ESP32-S3 e rádio SX1262.

### SX1262 / SPI

| Função | GPIO |
|---|---:|
| SCK | 9 |
| MISO | 11 |
| MOSI | 10 |
| NSS/CS | 8 |
| DIO1 | 14 |
| RESET do rádio | 12 |
| BUSY | 13 |

### OLED

| Função | GPIO |
|---|---:|
| SDA | 17 |
| SCL | 18 |
| RESET | 21 |
| VEXT | 36 |

### Sensor de teste

Na versão monolítica, o DHT22 está configurado no **GPIO 4**:

```cpp
constexpr int PINO_DHT = 4;
```

O número `4` representa o GPIO4; não é necessário escrever `04`. A montagem física precisa seguir exatamente o sketch gravado.

O código de teste serial histórico pode usar outro pino, como GPIO26. Por isso, a pinagem deve ser conferida na versão específica antes de concluir que o sensor está com defeito.

### OLED e processamento

Ativar o VEXT e aumentar o contraste da OLED não altera a frequência, o alcance ou o protocolo LoRa. O efeito relevante é elétrico: o display pode consumir mais corrente, especialmente com muitos pixels acesos. Em alimentação USB estável isso não costuma afetar o processamento; em bateria, reduz a autonomia.

---

## Estrutura do repositório

| Caminho | Responsabilidade |
|---|---|
| `PCRM_v0_1_ACK.ino` | Primeiro estudo de confirmação entre nós. |
| `PCRM_v0_2_RETRANSMISSAO.ino` | Encaminhamento experimental por múltiplos saltos. |
| `PCRM_v0_4_MESH_GERAL.ino` | Base de mesh com descoberta de rotas e qualquer nó como origem. |
| `PCRM_v0_5_DESCoberta_NOS.ino` | Descoberta automática de presença e registro de nós. |
| `PCRM_v0_6_DASHBOARD_WEB.ino` | Integração da central com servidor web. |
| `PCRM_v0_6_Dashboard_Mesh/` | Tentativa modular com classes e responsabilidades separadas. |
| `PCRM_v0_8_Monolitico/` | Versão consolidada em um único `.ino` para facilitar o upload na Arduino IDE. |
| `PCRM_contexto_autoral.md` | Registro das decisões, autoria, objetivos e evolução do protocolo. |
| `assets/` | Ilustrações usadas neste README. |

### Por que existe uma versão monolítica?

Durante os testes, a separação em muitos arquivos gerou dificuldades de caminho e de compilação na Arduino IDE. A versão `PCRM_v0_8_Monolitico` concentra o firmware em um único arquivo, mas mantém comentários e blocos lógicos por responsabilidade.

Isso é uma decisão de organização do protótipo, não uma exigência de que todo sistema profissional precise ficar em um único arquivo. Em uma etapa posterior, o código pode voltar a ser dividido em módulos depois que o protocolo estiver estável.

---

## Como executar

### Dependências

- Arduino IDE;
- pacote de placas ESP32;
- biblioteca RadioLib;
- biblioteca DHT sensor library;
- Adafruit GFX;
- Adafruit SSD1306;
- placa Heltec WiFi LoRa 32 V3;
- antena compatível com o módulo de rádio.

### Gravar a dashboard

1. Abrir `PCRM_v0_8_Monolitico/PCRM_v0_8_Monolitico.ino`.
2. Selecionar a placa ESP32-S3/Heltec correspondente.
3. Conferir a porta serial.
4. Alterar somente:

   ```cpp
   constexpr bool EH_DASHBOARD = true;
   ```

5. Compilar e gravar.
6. Abrir o Monitor Serial em `115200`.
7. Observar o IP exibido na OLED ou no Monitor Serial.
8. Conectar ao Wi-Fi configurado e abrir esse IP no navegador.

### Gravar um nó da mesh

1. Usar o mesmo sketch monolítico.
2. Deixar:

   ```cpp
   constexpr bool EH_DASHBOARD = false;
   ```

3. Conferir DHT22 no GPIO4.
4. Gravar em cada placa.
5. Não definir `SALTO_ALVO` ou caminho manualmente.
6. Aguardar o anúncio da dashboard e a descoberta da rota.

Todas as placas que participam do mesmo teste precisam usar parâmetros de rádio compatíveis: frequência, largura de banda, spreading factor, coding rate, preâmbulo e potência.

---

## Metodologia de testes

O teste deve aumentar a complexidade gradualmente.

### Teste 1 — Sensor isolado

Confirmar no Monitor Serial se o DHT22 fornece valores válidos. Se falhar aqui, ainda não existe um problema de LoRa.

### Teste 2 — Ponto a ponto

Usar duas placas, uma transmitindo e outra recebendo. Registrar:

| Medida | Registro |
|---|---|
| Distância | metros entre as placas |
| RSSI | dBm recebido |
| SNR | margem sinal/ruído |
| Latência | tempo entre envio e resposta |
| Perda | pacotes enviados x recebidos |
| Configuração | frequência, BW, SF, CR e potência |

### Teste 3 — Central direta

Adicionar a placa dashboard e verificar se um nó consegue descobrir o gateway e enviar dados sem relay.

### Teste 4 — Dois caminhos possíveis

Adicionar outro nó e observar se a origem consegue escolher entre caminhos diferentes. Não basta olhar apenas o RSSI; é necessário registrar perdas, saltos e estabilidade.

### Teste 5 — Falha de rota

Desligar um nó que fazia parte do caminho. O esperado é que a origem expire o timeout, invalide a rota e tente descobrir outra possibilidade.

### Teste 6 — Nó novo

Ligar uma placa que ainda não estava na rede. A dashboard deve reconhecer a identidade automática e registrar o novo nó sem editar a rota dos demais.

### Teste 7 — Confirmação final

Verificar no Serial:

```text
[DADO] criado ...
[ROTA] ...
[TX] TIPO=DADO ...
[DADO] destino final ...
[ACK] dado entregue ...
```

O dado somente deve ser marcado como entregue quando o ACK correspondente ao mesmo `ID_DADO` retornar à origem.

---

## Estado atual e limitações

O projeto está em fase de prototipagem acadêmica e de testes físicos. A base atual já possui estrutura para:

- comunicação LoRa direta;
- múltiplos saltos;
- descoberta de rota;
- anúncio de presença;
- identidade automática por hardware;
- fila de transmissão;
- TTL;
- prevenção de duplicatas;
- confirmação final;
- timeout e tentativa de recuperação;
- gateway com dashboard HTTP local.

Ainda existem pontos que precisam ser validados antes de qualquer uso crítico:

- a fórmula de custo de rota é heurística;
- RSSI e SNR não substituem um teste estatístico de entrega;
- a implementação ainda precisa de ensaios com colisões e muitos nós;
- a confirmação indica recebimento pelo gateway, não persistência em um servidor externo;
- a versão textual consome mais bytes que um protocolo binário;
- não há, nesta base, uma camada completa de criptografia e gerenciamento de chaves;
- parâmetros regulatórios e de antena precisam ser conferidos para a instalação real;
- os sketches históricos podem ter pinagens e comportamentos diferentes da v0.8.

Essas limitações fazem parte da documentação técnica: registrar o que funciona e o que ainda precisa ser medido é tão importante quanto mostrar o caminho feliz.

---

## Próximas etapas

1. Finalizar a medição estatística do enlace ponto a ponto.
2. Registrar perda de pacotes por configuração de SF, BW e CR.
3. Melhorar o cálculo de rota com taxa de entrega e histórico de falhas.
4. Testar colisões com vários nós transmitindo em janelas próximas.
5. Evoluir o controle de fila e prioridade.
6. Definir versão binária opcional do pacote.
7. Adicionar integridade, autenticação e criptografia.
8. Persistir métricas e eventos em banco ou servidor externo.
9. Validar a entrada e a remoção automática de nós.
10. Separar novamente as classes quando o protocolo estiver estabilizado.

---

## Referências técnicas

- [Semtech — What is LoRa?](https://www.semtech.com/lora/what-is-lora)
- [Semtech — LoRa and LoRaWAN](https://www.semtech.com/uploads/technology/LoRa/lora-and-lorawan.pdf)
- [LoRa Alliance — diferença entre LoRa e LoRaWAN](https://resources.lora-alliance.org/infographic/lora-and-lorawan)
- [RadioLib — exemplos e suporte ao SX126x](https://github.com/jgromes/RadioLib)
- [RadioLib — exemplo de configuração do SX126x](https://github.com/jgromes/RadioLib/blob/master/examples/SX126x/SX126x_Settings/SX126x_Settings.ino)

---

## Autoria

**Anthony** — estudante de Análise e Desenvolvimento de Sistemas no IFPE e técnico em Informática.

As decisões de arquitetura, a escolha do hardware, a evolução do protocolo, a definição da mesh, a estratégia de confirmação e os testes pertencem ao desenvolvimento autoral do projeto PCRM.
