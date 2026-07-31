#include <LedControl.h>
#include <LiquidCrystal.h>
#include <Keypad.h>

// Matriz de agua (4 modulos MAX7219 encadenados)
#define CS  53
#define SCK 52
#define DIN 51

// Puente/relay del motor de lavado
#define D1 50
#define D0 49

// Botonera principal
#define BTN_ON     48
#define BTN_START  47
#define BTN_CONFIG 46

// LCD 16x2 (modo 4 bits)
#define LCD_D7 45
#define LCD_D6 44
#define LCD_D5 43
#define LCD_D4 42
#define LCD_EN 41
#define LCD_RS 40

// LEDs de temperatura (T) y nivel de agua (N)
#define T1 38
#define T2 37
#define T3 36
#define N1 35
#define N2 34
#define N3 33

// Teclado de temperatura
#define TC1 32
#define TF1 31
#define TF2 30

// Teclado de nivel de agua
#define NAC1 29
#define NAF1 28
#define NAF2 27

#define BUZZER 26

// LEDs que simulan temperatura durante el lavado
#define ST1 25
#define ST2 24
#define ST3 23

// Teclado de programas
#define CC1 21
#define CF1 20
#define CF2 19

LedControl lc(DIN, SCK, CS, 8);
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

char teclasTemp[2][1] = { {'o'}, {'k'} };
byte filasTemp[2] = {TF1, TF2};
byte colsTemp[1]  = {TC1};
Keypad tecladoTemp(makeKeymap(teclasTemp), filasTemp, colsTemp, 2, 1);

char teclasAgua[2][1] = { {'p'}, {'l'} };
byte filasAgua[2] = {NAF1, NAF2};
byte colsAgua[1]  = {NAC1};
Keypad tecladoAgua(makeKeymap(teclasAgua), filasAgua, colsAgua, 2, 1);

char teclasConfig[2][1] = { {'i'}, {'j'} };
byte filasConfig[2] = {CF1, CF2};
byte colsConfig[1]  = {CC1};
Keypad tecladoConfig(makeKeymap(teclasConfig), filasConfig, colsConfig, 2, 1);

bool sistemaEncendido = false;
bool lavadoActivo     = false;
bool pausa            = false;
bool mostrarMarca     = true;
bool configuracionActivado = true;
bool modoConfig       = false; // true = seleccion de programa, false = ajuste manual

byte nivelTemp   = 0; // 0=BAJO, 1=MEDIO, 2=ALTO
byte nivelAgua   = 0; // 0=BAJO, 1=MEDIO, 2=ALTO
byte nivelConfig = 0; // 0=NORMAL, 1=CRISTALES, 2=ECOLOGICO, 3=PRESION

unsigned long tiempoInicioLavado = 0;
unsigned long tiempoPausaInicio  = 0;
unsigned long duracionLavado     = 60000;
byte etapa = 0; // 0=Prelavado 1=Lavado 2=Enjuague 3=Secado

void setup() {
  inicializarLedsTemperatura();
  inicializarLedControl();
  inicializarMotores();
  inicializarLCD();
  inicializarEntradas();

  apagarTodo();
}

void loop() {
  leerPulsadoresGenerales();

  if (sistemaEncendido) {
    if (!lavadoActivo || pausa) {
      leerConfiguracionAvanzada();
    }
    actualizarLeds();
    ejecutarLavado();
  }
}

void leerPulsadoresGenerales() {

  if (digitalRead(BTN_CONFIG) && sistemaEncendido && (!lavadoActivo || pausa)) {
    delay(50);
    if (digitalRead(BTN_CONFIG)) {
      modoConfig = !modoConfig;
      sonar(2);

      if (modoConfig) {
        lcd.clear();
        lcd.print("MODO LAVADO");
        aplicarModoLavado();
      } else {
        lcd.clear();
        lcd.print("CONFIG MANUAL");
      }
      actualizarPantallaConfig();
      while (digitalRead(BTN_CONFIG));
    }
  }

  if (digitalRead(BTN_ON)) {
    delay(50);
    if (digitalRead(BTN_ON)) {
      if (!sistemaEncendido) {
        sistemaEncendido = true;
        sonar(1);
        lcd.display();
        lcd.clear();
        lcd.print("LavaPlatos");
        lcd.setCursor(3, 1);
        lcd.print("Grupo 6");
        delay(1500);
        mostrarMarca = false;
        lcd.clear();
        lcd.print("LISTO P/ INICIAR");

        nivelConfig = 0;
        nivelAgua = 0;
        nivelTemp = 0;
        calcularTiempo();

        actualizarPantallaConfig();
        actualizarLeds();
      } else if (!lavadoActivo || pausa) {
        sonar(4);
        sistemaEncendido = false;
        apagarTodo();
      }
      while (digitalRead(BTN_ON));
    }
  }

  if (digitalRead(BTN_START) && sistemaEncendido) {
    delay(50);
    if (digitalRead(BTN_START)) {
      sonar(3);
      if (!lavadoActivo) {
        lavadoActivo = true;
        pausa = false;
        tiempoInicioLavado = millis();
        lcd.clear();
      } else {
        pausa = !pausa;
        if (pausa) {
          apagarMotores();
          limpiarMatriz();
          tiempoPausaInicio = millis();
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("PAUSA...        ");
          actualizarPantallaConfig();
        } else {
          tiempoInicioLavado += millis() - tiempoPausaInicio;
          lcd.clear();
        }
      }
      while (digitalRead(BTN_START));
    }
  }
}

void leerConfiguracionAvanzada() {

  if (modoConfig) {
    char c = tecladoConfig.getKey();
    if (c) {
      if (c == 'i') {
        nivelConfig = (nivelConfig + 1) % 4;
      } else if (c == 'j') {
        nivelConfig = (nivelConfig + 3) % 4;
      }

      aplicarModoLavado();
      calcularTiempo();
      sonar(2);
      actualizarPantallaConfig();
      delay(100);
    }
    return;
  }

  char t = tecladoTemp.getKey();
  if (t) {
    if (t == 'o') {
      if (nivelTemp < 2) nivelTemp++; else nivelTemp = 2;
      calcularTiempo();
      sonar(2); actualizarLeds(); actualizarPantallaConfig(); delay(200);
    } else if (t == 'k') {
      if (nivelTemp > 0) nivelTemp--; else nivelTemp = 0;
      calcularTiempo();
      sonar(2); actualizarLeds(); actualizarPantallaConfig(); delay(200);
    }
  }

  char a = tecladoAgua.getKey();
  if (a) {
    if (a == 'p') {
      if (nivelAgua < 2) nivelAgua++; else nivelAgua = 2;
      calcularTiempo();
      sonar(2); actualizarLeds(); actualizarPantallaConfig(); delay(200);
    } else if (a == 'l') {
      if (nivelAgua > 0) nivelAgua--; else nivelAgua = 0;
      calcularTiempo();
      sonar(2); actualizarLeds(); actualizarPantallaConfig(); delay(200);
    }
  }
}

void calcularTiempo() {
  duracionLavado = 60000UL + ((unsigned long)nivelTemp * 30000UL) + ((unsigned long)nivelAgua * 30000UL);
}

void aplicarModoLavado() {
  switch (nivelConfig) {
    case 0: nivelAgua = 1; nivelTemp = 1; break; // NORMAL
    case 1: nivelAgua = 1; nivelTemp = 0; break; // CRISTALES
    case 2: nivelAgua = 0; nivelTemp = 0; break; // ECOLOGICO
    case 3: nivelAgua = 2; nivelTemp = 2; break; // PRESION
  }
  actualizarLeds();
  calcularTiempo();
}

void ejecutarLavado() {
  if (!lavadoActivo || pausa) return;

  unsigned long t = millis() - tiempoInicioLavado;

  if (t >= duracionLavado) {
    lavadoActivo = false;
    apagarMotores();
    limpiarMatriz();
    sonar(4);
    lcd.clear();
    lcd.print("LAVADO TERMINADO");
    delay(2000);
    apagarTodo();
    sistemaEncendido = true;
    lcd.display();
    lcd.print("LISTO P/ INICIAR");
    actualizarPantallaConfig();
    actualizarLeds();
    return;
  }

  if (t < duracionLavado * 0.25) etapa = 0;
  else if (t < duracionLavado * 0.5) etapa = 1;
  else if (t < duracionLavado * 0.75) etapa = 2;
  else etapa = 3;

  mostrarLCD(t);

  if (nivelAgua == 0) aguaBajo(true);
  if (nivelAgua == 1) aguaMedio(true);
  if (nivelAgua == 2) aguaAlto(true);
}

void mostrarLCD(unsigned long t) {
  lcd.setCursor(0, 0);
  const char* txt[] = {"PRELAVADO", "LAVADO", "ENJUAGUE", "SECADO"};
  lcd.print(txt[etapa]);
  lcd.print("        ");

  lcd.setCursor(12, 0);
  long restante = (long)duracionLavado - (long)t;
  if (restante < 0) restante = 0;

  int minutos = restante / 60000;
  int segundos = (restante % 60000) / 1000;

  lcd.print(minutos);
  lcd.print(":");
  if (segundos < 10) lcd.print("0");
  lcd.print(segundos);

  lcd.setCursor(0, 1);
  int barras = map(t, 0, duracionLavado, 0, 16);
  for (int i = 0; i < 16; i++) lcd.print(i < barras ? char(255) : '_');
}

void actualizarPantallaConfig() {

  if (modoConfig) {
    lcd.setCursor(0, 1);
    lcd.print("MODO: ");
    if (nivelConfig == 0) lcd.print("NORMAL     ");
    else if (nivelConfig == 1) lcd.print("CRISTALES  ");
    else if (nivelConfig == 2) lcd.print("ECOLOGICO  ");
    else lcd.print("PRESION    ");
    return;
  }

  if (!mostrarMarca && (!lavadoActivo || pausa)) {
    lcd.setCursor(0, 1);
    lcd.print("T:");
    if (nivelTemp == 0) lcd.print("BAJ ");
    else if (nivelTemp == 1) lcd.print("MED ");
    else lcd.print("ALT ");

    lcd.print("A:");
    if (nivelAgua == 0) lcd.print("BAJ ");
    else if (nivelAgua == 1) lcd.print("MED ");
    else lcd.print("ALT ");

    if (!lavadoActivo && !pausa) {
      lcd.setCursor(0, 0);
      lcd.print("LISTO P/ INICIAR");
    }
  }
}

void actualizarLeds() {
  digitalWrite(T1, LOW);
  digitalWrite(T2, LOW);
  digitalWrite(T3, LOW);
  if (nivelTemp == 0) digitalWrite(T1, HIGH);
  else if (nivelTemp == 1) digitalWrite(T2, HIGH);
  else if (nivelTemp == 2) digitalWrite(T3, HIGH);

  digitalWrite(N1, LOW);
  digitalWrite(N2, LOW);
  digitalWrite(N3, LOW);
  if (nivelAgua == 0) digitalWrite(N1, HIGH);
  else if (nivelAgua == 1) digitalWrite(N2, HIGH);
  else if (nivelAgua == 2) digitalWrite(N3, HIGH);

  simularTemperatura();
}

void simularTemperatura() {
  digitalWrite(ST1, LOW); digitalWrite(ST2, LOW); digitalWrite(ST3, LOW);
  if (!lavadoActivo || pausa) return;
  if (nivelTemp == 0) digitalWrite(ST1, HIGH);
  else if (nivelTemp == 1) digitalWrite(ST2, HIGH);
  else if (nivelTemp == 2) digitalWrite(ST3, HIGH);
}

bool delayNoBloqueante(unsigned long ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < ms) {
    leerPulsadoresGenerales();
    if (pausa || !sistemaEncendido) return false;
  }
  return true;
}

void limpiarMatriz() {
  for (int i = 0; i < 8; i++) {
    lc.clearDisplay(i);
  }
}

void aguaBajo(bool activo) {
  motorBajo(activo);
  for (int f = 0; f < 8; f++) {
    if (pausa || !sistemaEncendido) { apagarMotores(); limpiarMatriz(); return; }
    for (int c = 0; c < 8; c++) {
      if (pausa || !sistemaEncendido) { apagarMotores(); limpiarMatriz(); return; }

      lc.setLed(7, f, c, true);       lc.setLed(7, f - 2, c - 3, true);
      lc.setLed(7, f - 4, c - 6, true); lc.setLed(7, f - 6, c - 9, true);

      lc.setLed(6, f, c, true);       lc.setLed(6, f - 2, c - 3, true);
      lc.setLed(6, f - 4, c - 6, true); lc.setLed(6, f - 6, c - 9, true);

      lc.setLed(0, f, 7 - c, true);   lc.setLed(0, f - 2, 4 - c, true);
      lc.setLed(0, f - 4, 1 - c, true); lc.setLed(0, f - 6, -2 - c, true);

      lc.setLed(1, f, 7 - c, true);   lc.setLed(1, f - 2, 4 - c, true);
      lc.setLed(1, f - 4, 1 - c, true); lc.setLed(1, f - 6, -2 - c, true);

      if (!delayNoBloqueante(80)) { apagarMotores(); limpiarMatriz(); return; }

      lc.setLed(7, f, c, false);       lc.setLed(7, f - 2, c - 3, false);
      lc.setLed(7, f - 4, c - 6, false); lc.setLed(7, f - 6, c - 9, false);

      lc.setLed(6, f, c, false);       lc.setLed(6, f - 2, c - 3, false);
      lc.setLed(6, f - 4, c - 6, false); lc.setLed(6, f - 6, c - 9, false);

      lc.setLed(0, f, 7 - c, false);   lc.setLed(0, f - 2, 4 - c, false);
      lc.setLed(0, f - 4, 1 - c, false); lc.setLed(0, f - 6, -2 - c, false);

      lc.setLed(1, f, 7 - c, false);   lc.setLed(1, f - 2, 4 - c, false);
      lc.setLed(1, f - 4, 1 - c, false); lc.setLed(1, f - 6, -2 - c, false);
    }
  }
}

void aguaMedio(bool activo) {
  motorMedio(activo);
  for (int f = 0; f < 8; f++) {
    if (pausa || !sistemaEncendido) { apagarMotores(); limpiarMatriz(); return; }
    for (int c = 0; c < 8; c++) {
      if (pausa || !sistemaEncendido) { apagarMotores(); limpiarMatriz(); return; }

      lc.setLed(7, f, c, true);       lc.setLed(7, f - 2, c - 3, true);
      lc.setLed(7, f - 4, c - 6, true); lc.setLed(7, f - 6, c - 9, true);

      lc.setLed(6, f, c, true);       lc.setLed(6, f - 2, c - 3, true);
      lc.setLed(6, f - 4, c - 6, true); lc.setLed(6, f - 6, c - 9, true);

      lc.setLed(5, f, c, true);       lc.setLed(5, f - 2, c - 3, true);
      lc.setLed(5, f - 4, c - 6, true); lc.setLed(5, f - 6, c - 9, true);

      lc.setLed(0, f, 7 - c, true);   lc.setLed(0, f - 2, 4 - c, true);
      lc.setLed(0, f - 4, 1 - c, true); lc.setLed(0, f - 6, -2 - c, true);

      lc.setLed(1, f, 7 - c, true);   lc.setLed(1, f - 2, 4 - c, true);
      lc.setLed(1, f - 4, 1 - c, true); lc.setLed(1, f - 6, -2 - c, true);

      lc.setLed(2, f, 7 - c, true);   lc.setLed(2, f - 2, 4 - c, true);
      lc.setLed(2, f - 4, 1 - c, true); lc.setLed(2, f - 6, -2 - c, true);

      if (!delayNoBloqueante(50)) { apagarMotores(); limpiarMatriz(); return; }

      lc.setLed(7, f, c, false);       lc.setLed(7, f - 2, c - 3, false);
      lc.setLed(7, f - 4, c - 6, false); lc.setLed(7, f - 6, c - 9, false);

      lc.setLed(6, f, c, false);       lc.setLed(6, f - 2, c - 3, false);
      lc.setLed(6, f - 4, c - 6, false); lc.setLed(6, f - 6, c - 9, false);

      lc.setLed(5, f, c, false);       lc.setLed(5, f - 2, c - 3, false);
      lc.setLed(5, f - 4, c - 6, false); lc.setLed(5, f - 6, c - 9, false);

      lc.setLed(0, f, 7 - c, false);   lc.setLed(0, f - 2, 4 - c, false);
      lc.setLed(0, f - 4, 1 - c, false); lc.setLed(0, f - 6, -2 - c, false);

      lc.setLed(1, f, 7 - c, false);   lc.setLed(1, f - 2, 4 - c, false);
      lc.setLed(1, f - 4, 1 - c, false); lc.setLed(1, f - 6, -2 - c, false);

      lc.setLed(2, f, 7 - c, false);   lc.setLed(2, f - 2, 4 - c, false);
      lc.setLed(2, f - 4, 1 - c, false); lc.setLed(2, f - 6, -2 - c, false);
    }
  }
}

void aguaAlto(bool activo) {
  motorAlto(activo);
  for (int f = 0; f < 8; f++) {
    if (pausa || !sistemaEncendido) { apagarMotores(); limpiarMatriz(); return; }
    for (int c = 0; c < 8; c++) {
      if (pausa || !sistemaEncendido) { apagarMotores(); limpiarMatriz(); return; }

      for (int m = 0; m <= 3; m++) {
        lc.setLed(7 - m, f, c, true);     lc.setLed(7 - m, f - 2, c - 3, true);
        lc.setLed(7 - m, f - 4, c - 6, true); lc.setLed(7 - m, f - 6, c - 9, true);
        lc.setLed(m, f, 7 - c, true);     lc.setLed(m, f - 2, 4 - c, true);
        lc.setLed(m, f - 4, 1 - c, true); lc.setLed(m, f - 6, -2 - c, true);
      }

      if (!delayNoBloqueante(30)) { apagarMotores(); limpiarMatriz(); return; }

      for (int m = 0; m <= 3; m++) {
        lc.setLed(7 - m, f, c, false);     lc.setLed(7 - m, f - 2, c - 3, false);
        lc.setLed(7 - m, f - 4, c - 6, false); lc.setLed(7 - m, f - 6, c - 9, false);
        lc.setLed(m, f, 7 - c, false);     lc.setLed(m, f - 2, 4 - c, false);
        lc.setLed(m, f - 4, 1 - c, false); lc.setLed(m, f - 6, -2 - c, false);
      }
    }
  }
}

void inicializarLedControl() {
  for (int i = 0; i < 8; i++) { lc.shutdown(i, false); lc.setIntensity(i, 10); lc.clearDisplay(i); }
}

void inicializarLCD() {
  lcd.begin(16, 2);
}

void inicializarEntradas() {
  pinMode(BTN_ON, INPUT); pinMode(BTN_START, INPUT); pinMode(BUZZER, OUTPUT);
  pinMode(T1, OUTPUT); pinMode(T2, OUTPUT); pinMode(T3, OUTPUT);
  pinMode(N1, OUTPUT); pinMode(N2, OUTPUT); pinMode(N3, OUTPUT);
}

void inicializarMotores() {
  pinMode(D0, OUTPUT); pinMode(D1, OUTPUT);
}

void inicializarLedsTemperatura() {
  pinMode(ST1, OUTPUT); pinMode(ST2, OUTPUT); pinMode(ST3, OUTPUT);
}

void motorBajo(bool a)  { if (a) { digitalWrite(D1, LOW);  digitalWrite(D0, HIGH); } else apagarMotores(); }
void motorMedio(bool a) { if (a) { digitalWrite(D1, HIGH); digitalWrite(D0, LOW);  } else apagarMotores(); }
void motorAlto(bool a)  { if (a) { digitalWrite(D1, HIGH); digitalWrite(D0, HIGH); } else apagarMotores(); }
void apagarMotores()    { digitalWrite(D0, LOW); digitalWrite(D1, LOW); }

void sonar(int tipo) {
  switch (tipo) {
    case 1: tone(BUZZER, 1000, 100); delay(150); tone(BUZZER, 1500, 300); break;
    case 2: tone(BUZZER, 2000, 50); break;
    case 3: tone(BUZZER, 800, 100); delay(100); tone(BUZZER, 800, 100); break;
    case 4: tone(BUZZER, 500, 300); delay(300); tone(BUZZER, 300, 400); break;
  }
}

void apagarTodo() {
  lcd.clear(); lcd.noDisplay(); apagarMotores(); limpiarMatriz();
  digitalWrite(T1, LOW); digitalWrite(T2, LOW); digitalWrite(T3, LOW);
  digitalWrite(N1, LOW); digitalWrite(N2, LOW); digitalWrite(N3, LOW);
  lavadoActivo = false; pausa = false; mostrarMarca = true; nivelTemp = 0; nivelAgua = 0;

  nivelConfig = 0;
  calcularTiempo();
}
