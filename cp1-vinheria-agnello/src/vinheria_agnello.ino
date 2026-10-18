#include <Adafruit_LiquidCrystal.h>

Adafruit_LiquidCrystal lcd(0); // 0 é endereço do i2c, uma conectividade que facilita as conexões para mais aprimoramentos

const int LED_VERMELHO = 8;     
const int LED_AMARELO  = 9;     
const int LED_VERDE    = 10;    
const int BUZZER       = 11;    
const int LDR          = A0;


void setup() { // "liga" o arduino
  // Configuração dos pinos de saída
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  // Logo: uma uva desenhada em 5x8 pixels
byte uva[8] = {
  B00100,
  B01000,
  B01110,
  B11111,
  B11111,
  B01110,
  B00100,
  B00000
};

  
  // Inicialização da Serial e do LCD
  Serial.begin(9600); // canal de comunicação com o pc e 9660 diz respeito a velocidade
  lcd.begin(16, 2); // 16 colunas por 2 linhas
  lcd.createChar(0, uva);  // registra a "pixelart" da uva; 0 é a gaveta, no lcd as gavetas vão de 0 a 7
  
 // Boas-vindas com logo
  lcd.setCursor(0, 0);
  lcd.write((uint8_t)0);             // desenha a uva
  lcd.print("Agnello Vinhos");
  lcd.write((uint8_t)0);
  lcd.setCursor(0, 1);
  lcd.print("   Bem-vindo  ");
  delay(3000);
  lcd.clear();
}

void loop() { // "loop" verificador
  int leituraBruta = analogRead(LDR); // essa linha e a de baixo efetivamente monitoram
  int luminosidade = map(leituraBruta, 0, 1023, 0, 100); // porque arduino lê de 0 a 1023, e isso precisa ser convertido em percentual

  lcd.setCursor(0, 0);
  lcd.print("Luz: ");
  lcd.print(luminosidade);
  lcd.print("%   ");

  if (luminosidade <= 30) {
    controlLights(luminosidade);
    noTone(BUZZER);
    display_status("OK   ");
  } 
  else if (luminosidade > 30 && luminosidade <= 60) {
    controlLights(luminosidade);
    display_status("ALERTA   ");
    play_buzzer();

  } 
  else {
    controlLights(luminosidade);
    display_status("CRITICO   ");
    play_buzzer();
  }
  delay(1000);
}

void display_status(String status) {
      lcd.setCursor(0, 1);
      lcd.print("Status: ");
      lcd.print(status);
      lcd.print("                ");
}

void controlLights(int luminosidade) {
  digitalWrite(LED_VERDE, (luminosidade <= 30));
  digitalWrite(LED_AMARELO, (luminosidade > 30) && (luminosidade <= 60));
  digitalWrite(LED_VERMELHO, luminosidade > 60);
}

void play_buzzer() {
    tone(BUZZER, 500); 
    delay(3000);       
    noTone(BUZZER);     
}