// Projeto CP2 - Vinheria Agnello
// O sistema monitora a temperatura, a umidade e a luminosidade da adega.
// A cada 10 segundos, calcula a média das leituras e verifica se os valores estão ideais.
// Os LEDs indicam o estado do ambiente e o buzzer avisa quando há uma situação crítica.
// Os valores aparecem no LCD, e as situações críticas são gravadas na EEPROM.

#include <Wire.h>               // Comunicação I2C, usada pelo LCD e pelo relógio.
#include <DHT.h>                // Biblioteca do sensor de temperatura e umidade.
#include <LiquidCrystal_I2C.h>  // Biblioteca para controlar o LCD com módulo I2C.
#include <RTClib.h>             // Biblioteca do relógio de tempo real DS1307.
#include <EEPROM.h>             // Biblioteca da memória que não se apaga ao desligar.

// Pinos ligados aos componentes.
#define PINO_DHT     2          // Pino digital do sensor DHT22.
#define PINO_LDR     A0         // Entrada analógica do sensor de luminosidade.
#define LED_VERDE    8          // Pino do LED verde (ambiente ideal).
#define LED_AMARELO  9          // Pino do LED amarelo (atenção).
#define LED_VERMELHO 10         // Pino do LED vermelho (situação crítica).
#define BUZZER       11         // Pino do buzzer.

// Limites de temperatura, em graus Celsius.
#define TEMP_MIN 10             // Abaixo disso: crítico (13 °C menos 3 °C de variação).
#define TEMP_IDEAL_MIN 12       // De 12 °C a 14 °C: ideal (perto dos 13 °C).
#define TEMP_IDEAL_MAX 14       // Entre o ideal e o crítico: atenção.
#define TEMP_MAX 16             // Acima disso: crítico (13 °C mais 3 °C de variação).

// Limites de umidade relativa, em porcentagem.
#define UMID_MIN 60             // Abaixo disso: crítico.
#define UMID_IDEAL_MIN 65       // De 65% a 75%: ideal (perto dos 70%).
#define UMID_IDEAL_MAX 75       // Entre o ideal e o crítico: atenção.
#define UMID_MAX 80             // Acima disso: crítico.

// Limites de luminosidade, em porcentagem.
#define LUZ_OK   30             // Até 30%: penumbra, o ideal para o vinho.
#define LUZ_MAX  60             // De 31% a 60%: atenção. Acima de 60%: crítico.

// Valores do LDR medidos no teste de calibração.
#define LDR_ESCURO 1016          // Leitura do LDR com o mínimo de luz.
#define LDR_CLARO  8           // Leitura do LDR com o máximo de luz.

// Cria o objeto para comunicar com o sensor DHT22.
DHT dht(PINO_DHT, DHT22);

// Configura o LCD no endereço I2C 0x27, com 16 colunas e 2 linhas.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Cria o objeto para comunicar com o relógio DS1307.
RTC_DS1307 rtc;

// Configurações (setup) que ficam guardadas na EEPROM.
int fusoUTC = -3;               // Fuso horário de Brasília: UTC-3.
char unidade = 'C';             // Unidade de temperatura: 'C' (Celsius).

// Acumuladores usados para calcular as médias.
float somaTemp = 0;             // Soma das leituras de temperatura.
float somaUmid = 0;             // Soma das leituras de umidade.
float somaLuz = 0;              // Soma das leituras de luminosidade.
int leituras = 0;               // Quantidade de leituras já somadas.

// Médias calculadas a cada 10 segundos.
float mediaTemp = 0;            // Média da temperatura.
float mediaUmid = 0;            // Média da umidade.
float mediaLuz = 0;             // Média da luminosidade.

int segundos = 0;               // Conta os segundos para trocar a tela do LCD.
int enderecoLog = 10;           // Endereço da EEPROM onde será gravado o próximo log.

// Desenhos da taça para o logo. Cada desenho tem 8 linhas de 5 pixels (1 = aceso).
byte tacaVazia[8] = {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}; // Taça vazia.
byte tacaMeia[8]  = {0b10001, 0b10001, 0b10001, 0b10001, 0b11111, 0b11111, 0b01110, 0b00100}; // Taça pela metade.
byte tacaCheia[8] = {0b10001, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b01110, 0b00100}; // Taça cheia.
byte haste[8]     = {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110, 0b11111, 0b00000}; // Haste e base.

// Executa uma vez quando o Arduino é ligado ou reiniciado.
void setup() {
  Serial.begin(9600);                   // Inicia a comunicação Serial a 9600 bits por segundo.

  pinMode(LED_VERDE, OUTPUT);           // Configura o LED verde como saída.
  pinMode(LED_AMARELO, OUTPUT);         // Configura o LED amarelo como saída.
  pinMode(LED_VERMELHO, OUTPUT);        // Configura o LED vermelho como saída.
  pinMode(BUZZER, OUTPUT);              // Configura o buzzer como saída.

  dht.begin();                          // Inicializa o sensor DHT22.
  lcd.init();                           // Inicializa o LCD.
  lcd.backlight();                      // Liga a iluminação do LCD.
  rtc.begin();                          // Inicializa o relógio.

  EEPROM.put(0, fusoUTC);               // Grava o fuso horário nos endereços 0 e 1 da EEPROM.
  EEPROM.put(2, unidade);               // Grava a unidade de temperatura no endereço 2.
  EEPROM.get(0, fusoUTC);               // Lê o fuso horário de volta da EEPROM.
  EEPROM.get(2, unidade);               // Lê a unidade de temperatura de volta da EEPROM.

  mostrarLogo();                        // Mostra o logo animado no LCD.

  mediaTemp = dht.readTemperature();    // Primeira leitura, para o LCD não começar mostrando zero.
  mediaUmid = dht.readHumidity();       // Primeira leitura da umidade.
  mediaLuz = lerLuz();                  // Primeira leitura da luminosidade.
}

// Esta função se repete continuamente, uma vez por segundo.
void loop() {
  float temp = dht.readTemperature();   // Lê a temperatura em graus Celsius.
  float umid = dht.readHumidity();      // Lê a umidade relativa em porcentagem.

  // Só soma as leituras quando o sensor responde corretamente.
  if (!isnan(temp) && !isnan(umid)) {
    somaTemp += temp;                   // Adiciona a temperatura ao acumulador.
    somaUmid += umid;                   // Adiciona a umidade ao acumulador.
    somaLuz += lerLuz();                // Adiciona a luminosidade ao acumulador.
    leituras++;                         // Conta mais uma leitura.
  }

  // Depois de 10 leituras (10 segundos), calcula as médias.
  if (leituras == 10) {
    mediaTemp = somaTemp / 10;          // Calcula a média da temperatura.
    mediaUmid = somaUmid / 10;          // Calcula a média da umidade.
    mediaLuz = somaLuz / 10;            // Calcula a média da luminosidade.

    verificarAlertas();                 // Atualiza os LEDs, o buzzer e os logs.

    somaTemp = 0;                       // Zera o acumulador de temperatura.
    somaUmid = 0;                       // Zera o acumulador de umidade.
    somaLuz = 0;                        // Zera o acumulador de luminosidade.
    leituras = 0;                       // Recomeça a contagem para os próximos 10 segundos.
  }

  int tela = (segundos / 3) % 3;        // Escolhe a tela (0, 1 ou 2), trocando a cada 3 segundos.
  mostrarTela(tela);                    // Mostra a tela escolhida no LCD.

  segundos++;                           // Conta mais um segundo.
  delay(1000);                          // Aguarda 1 segundo até a próxima leitura.
}

// Lê o LDR e converte o valor para uma escala de 0% a 100%.
int lerLuz() {
  int leitura = analogRead(PINO_LDR);                       // Lê o valor bruto do sensor (0 a 1023).
  int luz = map(leitura, LDR_ESCURO, LDR_CLARO, 0, 100);    // Calibra: 1016 vira 0% e 8 vira 100%.
  return constrain(luz, 0, 100);                            // Limita o resultado entre 0% e 100%.
}

// Verifica as médias e indica o estado do ambiente com os LEDs e o buzzer.
void verificarAlertas() {
  bool tempRuim = mediaTemp < TEMP_MIN || mediaTemp > TEMP_MAX;   // Temperatura fora dos limites.
  bool umidRuim = mediaUmid < UMID_MIN || mediaUmid > UMID_MAX;   // Umidade fora dos limites.
  bool luzAlta = mediaLuz > LUZ_MAX;                              // Luz acima do limite crítico.

  bool tempAtencao = mediaTemp < TEMP_IDEAL_MIN || mediaTemp > TEMP_IDEAL_MAX;   // Temperatura fora do ideal.
  bool umidAtencao = mediaUmid < UMID_IDEAL_MIN || mediaUmid > UMID_IDEAL_MAX;   // Umidade fora do ideal.
  bool luzAtencao = mediaLuz > LUZ_OK;                                           // Luz acima da penumbra.

  digitalWrite(LED_VERDE, LOW);         // Apaga o LED verde.
  digitalWrite(LED_AMARELO, LOW);       // Apaga o LED amarelo.
  digitalWrite(LED_VERMELHO, LOW);      // Apaga o LED vermelho.

  // Situação crítica: temperatura, umidade ou luz além dos limites críticos.
  if (tempRuim || umidRuim || luzAlta) {
    digitalWrite(LED_VERMELHO, HIGH);   // Acende o LED vermelho.
    tone(BUZZER, 1000, 1000);           // Emite um som de 1000 Hz durante 1 segundo.
    gravarLog();                        // Registra a ocorrência na EEPROM.
  }
  // Situação de atenção: algum valor saiu do ideal, mas ainda não é crítico.
  else if (tempAtencao || umidAtencao || luzAtencao) {
    digitalWrite(LED_AMARELO, HIGH);    // Acende o LED amarelo.
  }
  // Todas as condições estão dentro dos limites.
  else {
    digitalWrite(LED_VERDE, HIGH);      // Acende o LED verde.
  }
}

// Grava um log na EEPROM com a hora (em UTC), o minuto e as médias.
void gravarLog() {
  DateTime agora = rtc.now();                         // Obtém a data e a hora atuais do relógio.
  int horaUTC = (agora.hour() - fusoUTC + 24) % 24;   // Converte a hora local para UTC.

  // Cada log ocupa 5 endereços (5 bytes) da EEPROM.
  EEPROM.write(enderecoLog, horaUTC);                 // Byte 1: hora em UTC.
  EEPROM.write(enderecoLog + 1, agora.minute());      // Byte 2: minuto.
  EEPROM.write(enderecoLog + 2, (int)mediaTemp);      // Byte 3: temperatura média.
  EEPROM.write(enderecoLog + 3, (int)mediaUmid);      // Byte 4: umidade média.
  EEPROM.write(enderecoLog + 4, (int)mediaLuz);       // Byte 5: luminosidade média.

  enderecoLog += 5;                                   // Avança para a posição do próximo log.
  if (enderecoLog > 1015) enderecoLog = 10;           // Se a memória encher, volta ao início.

  // Mostra o log no Serial Monitor.
  Serial.print("LOG ");                               // Identifica a linha como um log.
  Serial.print(agora.hour());                         // Envia a hora local.
  Serial.print(":");                                  // Separador entre hora e minuto.
  Serial.print(agora.minute());                       // Envia o minuto.
  Serial.print(" (UTC ");                             // Identifica a hora em UTC.
  Serial.print(horaUTC);                              // Envia a hora em UTC.
  Serial.print("h) | Temp: ");                        // Identifica o campo da temperatura.
  Serial.print(mediaTemp);                            // Envia a temperatura média.
  Serial.print(" | Umid: ");                          // Identifica o campo da umidade.
  Serial.print(mediaUmid);                            // Envia a umidade média.
  Serial.print(" | Luz: ");                           // Identifica o campo da luminosidade.
  Serial.println(mediaLuz);                           // Envia a luminosidade e termina a linha.
}

// Mostra uma das três telas no LCD: 0 = temperatura, 1 = umidade, 2 = luminosidade.
void mostrarTela(int tela) {
  lcd.clear();                                        // Limpa o LCD.

  DateTime agora = rtc.now();                         // Obtém a hora atual do relógio.
  lcd.setCursor(11, 0);                               // Posiciona o cursor no canto superior direito.
  if (agora.hour() < 10) lcd.print("0");              // Acrescenta o zero à esquerda (9 vira 09).
  lcd.print(agora.hour());                            // Mostra a hora.
  lcd.print(":");                                     // Separador entre hora e minuto.
  if (agora.minute() < 10) lcd.print("0");            // Acrescenta o zero à esquerda no minuto.
  lcd.print(agora.minute());                          // Mostra o minuto.

  lcd.setCursor(0, 0);                                // Volta o cursor para o início da primeira linha.

  // Tela 0: temperatura.
  if (tela == 0) {
    lcd.print("Temp.");                               // Mostra o título da tela.
    lcd.setCursor(0, 1);                              // Passa para a segunda linha.
    if (unidade == 'F') lcd.print(mediaTemp * 1.8 + 32, 1);   // Converte para Fahrenheit, se configurado.
    else lcd.print(mediaTemp, 1);                     // Mostra em Celsius, com uma casa decimal.
    lcd.print((char)223);                             // Mostra o símbolo de grau.
    lcd.print(unidade);                               // Mostra a unidade (C ou F).
    if (mediaTemp < TEMP_MIN) lcd.print(" BAIXA");    // Temperatura abaixo do limite crítico.
    else if (mediaTemp > TEMP_MAX) lcd.print(" ALTA"); // Temperatura acima do limite crítico.
    else if (mediaTemp < TEMP_IDEAL_MIN || mediaTemp > TEMP_IDEAL_MAX) lcd.print(" ATENCAO"); // Fora do ideal.
    else lcd.print(" OK");                            // Temperatura ideal.
  }
  // Tela 1: umidade.
  else if (tela == 1) {
    lcd.print("Umidade");                             // Mostra o título da tela.
    lcd.setCursor(0, 1);                              // Passa para a segunda linha.
    lcd.print(mediaUmid, 0);                          // Mostra a umidade sem casas decimais.
    lcd.print("%");                                   // Mostra o símbolo de porcentagem.
    if (mediaUmid < UMID_MIN) lcd.print(" BAIXA");    // Umidade abaixo do limite crítico.
    else if (mediaUmid > UMID_MAX) lcd.print(" ALTA"); // Umidade acima do limite crítico.
    else if (mediaUmid < UMID_IDEAL_MIN || mediaUmid > UMID_IDEAL_MAX) lcd.print(" ATENCAO"); // Fora do ideal.
    else lcd.print(" OK");                            // Umidade ideal.
  }
  // Tela 2: luminosidade.
  else {
    lcd.print("Luz");                                 // Mostra o título da tela.
    lcd.setCursor(0, 1);                              // Passa para a segunda linha.
    lcd.print(mediaLuz, 0);                           // Mostra a luminosidade sem casas decimais.
    lcd.print("%");                                   // Mostra o símbolo de porcentagem.
    if (mediaLuz <= LUZ_OK) lcd.print(" OK");         // Penumbra: ideal.
    else if (mediaLuz <= LUZ_MAX) lcd.print(" MEIA LUZ"); // Faixa de atenção.
    else lcd.print(" ALTA");                          // Luz acima do limite.
  }
}

// Mostra o logo animado: a taça enche de vinho e o nome da vinheria aparece.
void mostrarLogo() {
  lcd.createChar(0, tacaVazia);         // Registra o desenho da taça vazia na posição 0.
  lcd.createChar(1, tacaMeia);          // Registra o desenho da taça pela metade na posição 1.
  lcd.createChar(2, tacaCheia);         // Registra o desenho da taça cheia na posição 2.
  lcd.createChar(3, haste);             // Registra o desenho da haste na posição 3.

  lcd.setCursor(0, 1);                  // Posiciona o cursor no início da segunda linha.
  lcd.write(3);                         // Desenha a haste da taça.

  lcd.setCursor(0, 0);                  // Posiciona o cursor no início da primeira linha.
  lcd.write(byte(0));                   // Desenha a taça vazia (byte() evita ambiguidade com o zero).
  delay(500);                           // Aguarda meio segundo.
  lcd.setCursor(0, 0);                  // Volta para a mesma posição.
  lcd.write(1);                         // Desenha a taça pela metade.
  delay(500);                           // Aguarda meio segundo.
  lcd.setCursor(0, 0);                  // Volta para a mesma posição.
  lcd.write(2);                         // Desenha a taça cheia.
  delay(500);                           // Aguarda meio segundo.

  lcd.setCursor(3, 0);                  // Posiciona o cursor ao lado da taça, na primeira linha.
  lcd.print("Vinheria");                // Mostra o nome da vinheria.
  lcd.setCursor(3, 1);                  // Posiciona o cursor na segunda linha.
  lcd.print("Agnello");                 // Mostra o sobrenome da vinheria.
  delay(2000);                          // Mantém o logo na tela durante 2 segundos.
}