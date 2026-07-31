// ================= LIBRERIAS =================
#include <LedControl.h>
#include <LiquidCrystal.h>
#include <Keypad.h>

// ================= PINES MATRIZ (AGUA - MAX7219) =================
#define CS  53
#define SCK 52
#define DIN 51

// ================= PINES MOTOR =================
#define D1 50
#define D0 49

// ================= PULSADORES =================
#define BTN_ON    48
#define BTN_START 47
#define BTN_CONFIG 46

// ================= LCD =================
#define LCD_D7 45
#define LCD_D6 44
#define LCD_D5 43
#define LCD_D4 42
#define LCD_EN 41
#define LCD_RS 40

// ================= LEDS =================
#define T1 38
#define T2 37
#define T3 36
#define N1 35
#define N2 34
#define N3 33

// ================= KEYPAD TEMP =================
#define TC1 32
#define TF1 31
#define TF2 30

// ================= KEYPAD AGUA =================
#define NAC1 29
#define NAF1 28
#define NAF2 27

// ================= EXTRAS =================
#define BUZZER 26 

// ================= PINES DIODOS TEMPERATURA ==============
#define ST1 25
#define ST2 24
#define ST3 23

// ================= PINES KEYPAD DE CONFIGURACION ===========
#define CC1 21
#define CF1 20
#define CF2 19

// >>>>>>>>>>> PINES MATRIZ ICONOS (595) <<<<<<<<<<<
#define M_DATA  12 
#define M_CLOCK 11 
#define M_LATCH 10 

// >>>>>>>>>>> DIBUJOS ICONOS (PIXEL PERFECT) <<<<<<<<<<<
// 1. GOTA (Punta fina, base ancha) - MODO NORMAL/MANUAL
byte I_MANUAL[8] = {0x10, 0x38, 0x28, 0x44, 0x44, 0x44, 0x38, 0x00};
// 2. CRISTALES (Copa centrada)
byte I_GLASS[8]  = {0x00, 0x7F, 0x3E, 0x1C, 0x08, 0x08, 0x1C, 0x00};
// 3. ECOLOGICO (Hoja diagonal achatada)
byte I_ECO[8]    = {0x06, 0x09, 0x12, 0x24, 0x44, 0x30, 0x40, 0x80};
// 4. PRESION (Chorro punteado)
byte I_PRES[8]   = {0x05, 0x0A, 0x14, 0xA8, 0x14, 0x0A, 0x05, 0x00}; 

// ================= OBJETOS =================
LedControl lc(DIN, SCK, CS, 8);
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ================= KEYPADS =================
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

// ================= ESTADO =================
bool sistemaEncendido = false;
bool lavadoActivo = false;
bool pausa = false;
bool mostrarMarca = true;
bool configuracionActivado =true;
bool modoConfig = false;

byte nivelTemp = 0;   // 0=BAJO, 1=MEDIO, 2=ALTO
byte nivelAgua = 0;   // 0=BAJO, 1=MEDIO, 2=ALTO

// 4 MODOS: 0=NORMAL(MANUAL), 1=CRISTAL, 2=ECO, 3=PRESION
byte nivelConfig = 0; 

unsigned long tiempoInicioLavado = 0;
unsigned long tiempoPausaInicio = 0;
unsigned long duracionLavado = 60000; // Base inicial
byte etapa = 0;

// ================= SETUP =================
void setup() {
  inicializarLedsTemperatura();
  inicializarLedControl();
  inicializarMotores();
  inicializarLCD();
  inicializarEntradas();
  
  pinMode(M_DATA, OUTPUT); pinMode(M_CLOCK, OUTPUT); pinMode(M_LATCH, OUTPUT);
  
  apagarTodo();
}

// >>>>>>>>>>> NUEVO: CALCULO DE TIEMPO MATEMATICO SEGURO <<<<<<<<<<<
void calcularTiempo() {
  // IMPORTANTE: Usamos 'UL' para forzar calculo en 32 bits y evitar que baje el tiempo
  // Base 60s + (30s * nivel temp) + (30s * nivel agua)
  duracionLavado = 60000UL + ((unsigned long)nivelTemp * 30000UL) + ((unsigned long)nivelAgua * 30000UL);
}

// >>>>>>>>>>> DIBUJO ICONOS <<<<<<<<<<<
void apagarMatrizIconos() {
    digitalWrite(M_LATCH, LOW); 
    shiftOut(M_DATA, M_CLOCK, LSBFIRST, 0); 
    shiftOut(M_DATA, M_CLOCK, LSBFIRST, 255); 
    digitalWrite(M_LATCH, HIGH);
}

void refescarMatrizIconos() {
    byte* dibujo;
    bool mostrarIconoDeModo = false;

    // LOGICA VISUAL CORREGIDA
    // 1. Si estamos seleccionando modo (Menu con I/J) -> Muestra icono del modo
    if (modoConfig) {
       mostrarIconoDeModo = true;
    }
    // 2. Si estamos LAVANDO (Play/Pausa) -> Muestra icono del modo
    else if (lavadoActivo || pausa) {
       mostrarIconoDeModo = true;
    }
    // 3. En cualquier otro caso (Listo/Standby/Config Manual fuera de lavado) -> GOTA
    else {
       mostrarIconoDeModo = false; 
    }

    if (mostrarIconoDeModo) {
       if (nivelConfig == 0) dibujo = I_MANUAL;
       else if (nivelConfig == 1) dibujo = I_GLASS;
       else if (nivelConfig == 2) dibujo = I_ECO;
       else dibujo = I_PRES;
    } else {
       dibujo = I_MANUAL; // SIEMPRE Gota en reposo/manual
    }

    // PARPADEO SOLO EN PLAY (NO EN PAUSA)
    bool encender = true;
    if (lavadoActivo && !pausa) {
        if ((millis() / 500) % 2 == 0) {
            encender = false;
        }
    }

    if (!encender) {
        apagarMatrizIconos();
        return;
    }

    // DIBUJAR (Bucle invertido para enderezar)
    for (int i=7; i>=0; i--) {
        digitalWrite(M_LATCH, LOW);
        shiftOut(M_DATA, M_CLOCK, LSBFIRST, (1 << i)); 
        shiftOut(M_DATA, M_CLOCK, LSBFIRST, ~dibujo[i]); 
        digitalWrite(M_LATCH, HIGH);
        delay(2); 
    }
}

// ================= LOOP =================
void loop() {
  leerPulsadoresGenerales();

  if (sistemaEncendido) {
    if (!lavadoActivo || pausa) {
      leerConfiguracionAvanzada();
    }
    actualizarLeds();
    
    // Refrescar siempre
    refescarMatrizIconos(); 
    
    ejecutarLavado();
  } else {
     apagarMatrizIconos();
  }
}

// ================= KEYPADS =================
void leerConfiguracionAvanzada() {

  if (modoConfig) {
    char c = tecladoConfig.getKey();
    if (c) {
      if (c == 'i') {
        nivelConfig = (nivelConfig + 1) % 4; 
      } 
      else if (c == 'j') {
        nivelConfig = (nivelConfig + 3) % 4; 
      }

      aplicarModoLavado();
      // Recalcular tiempo al cambiar modo
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
    }
    else if (t == 'k') { 
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
    }
    else if (a == 'l') { 
      if (nivelAgua > 0) nivelAgua--; else nivelAgua = 0;
      calcularTiempo();
      sonar(2); actualizarLeds(); actualizarPantallaConfig(); delay(200);
    }
  }
}

// ================= SONIDOS =================
void sonar(int tipo) {
  switch (tipo) {
    case 1: tone(BUZZER,1000,100); delay(150); tone(BUZZER,1500,300); break;
    case 2: tone(BUZZER,2000,50); break;
    case 3: tone(BUZZER,800,100); delay(100); tone(BUZZER,800,100); break;
    case 4: tone(BUZZER,500,300); delay(300); tone(BUZZER,300,400); break;
  }
}

// ================= BOTONES =================
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
        lcd.print("LAVAVAJILLAS");
        lcd.setCursor(5,1);
        lcd.print("ULTRA");
        delay(1500);
        mostrarMarca = false;
        lcd.clear();
        lcd.print("LISTO P/ INICIAR");
        
        // Resetear a valores iniciales
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
      while(digitalRead(BTN_ON));
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
          lcd.setCursor(0,0);
          lcd.print("PAUSA...        ");
          actualizarPantallaConfig(); // Muestra el tiempo restante
        } else {
          tiempoInicioLavado += millis() - tiempoPausaInicio;
          lcd.clear();
        }
      }
      while(digitalRead(BTN_START));
    }
  }
}

// ================= LAVADO =================
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

// ================= LCD =================
void mostrarLCD(unsigned long t) {
  lcd.setCursor(0,0);
  const char* txt[] = {"PRELAVADO","LAVADO","ENJUAGUE","SECADO"};
  lcd.print(txt[etapa]);
  lcd.print("        ");

  // TIEMPO RESTANTE (Calculo dinamico)
  lcd.setCursor(12,0);
  long restante = (long)duracionLavado - (long)t;
  if (restante < 0) restante = 0;
  
  int minutos = restante / 60000;
  int segundos = (restante % 60000) / 1000;

  lcd.print(minutos);
  lcd.print(":");
  if (segundos < 10) lcd.print("0");
  lcd.print(segundos);

  lcd.setCursor(0,1);
  int barras = map(t, 0, duracionLavado, 0, 16);
  for (int i=0;i<16;i++) lcd.print(i<barras?char(255):'_');
}

void actualizarPantallaConfig() {

    if (modoConfig) {
    lcd.setCursor(0,1);
    lcd.print("MODO: ");
    // TEXTOS ORIGINALES
    if (nivelConfig == 0) lcd.print("NORMAL     ");
    else if (nivelConfig == 1) lcd.print("CRISTALES  ");
    else if (nivelConfig == 2) lcd.print("ECOLOGICO  ");
    else lcd.print("PRESION    ");
    return;
  }

  if (!mostrarMarca && (!lavadoActivo || pausa)) {
    lcd.setCursor(0,1);
    lcd.print("T:");
    if (nivelTemp == 0) lcd.print("BAJ ");
    else if (nivelTemp == 1) lcd.print("MED ");
    else lcd.print("ALT ");
    
    lcd.print("A:");
    if (nivelAgua == 0) lcd.print("BAJ ");
    else if (nivelAgua == 1) lcd.print("MED ");
    else lcd.print("ALT ");
    
    // TIEMPO ELIMINADO DE AQUI
    
    if (!lavadoActivo && !pausa) {
      lcd.setCursor(0,0);
      lcd.print("LISTO P/ INICIAR");
    }
  }
}

// ================= LEDS =================
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

// ================= DELAY NO BLOQUEANTE =================
bool delayNoBloqueante(unsigned long ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < ms) {
    leerPulsadoresGenerales();
    
    if (sistemaEncendido) refescarMatrizIconos(); 

    if (pausa || !sistemaEncendido) return false;
  }
  return true;
}

// ================= ANIMACIONES COMPLETAS =================
void limpiarMatriz() {
  for (int i=0; i<8; i++) {
    lc.clearDisplay(i);
  }
}

void aguaBajo(bool activo) {
  motorBajo(activo);
  for (int f=0; f<8; f++) {
    if(pausa||!sistemaEncendido) {
      apagarMotores();
      limpiarMatriz();
      return;
    }
    for (int c=0; c<8; c++) {
      if(pausa||!sistemaEncendido) {
        apagarMotores();
        limpiarMatriz();
        return;
      }
      
      lc.setLed(7,f,c,true);
      lc.setLed(7,f-2,c-3,true);
      lc.setLed(7,f-4,c-6,true);
      lc.setLed(7,f-6,c-9,true);   

      lc.setLed(6,f,c,true);
      lc.setLed(6,f-2,c-3,true);
      lc.setLed(6,f-4,c-6,true);
      lc.setLed(6,f-6,c-9,true);

      lc.setLed(0,f,7-c,true);
      lc.setLed(0,f-2,4-c,true);
      lc.setLed(0,f-4,1-c,true);
      lc.setLed(0,f-6,-2-c,true);
        
      lc.setLed(1,f,7-c,true);
      lc.setLed(1,f-2,4-c,true);
      lc.setLed(1,f-4,1-c,true);
      lc.setLed(1,f-6,-2-c,true);
      
      if (!delayNoBloqueante(80)) {
        apagarMotores();
        limpiarMatriz();
        return;
      }
      
      lc.setLed(7,f,c,false);
      lc.setLed(7,f-2,c-3,false);
      lc.setLed(7,f-4,c-6,false);
      lc.setLed(7,f-6,c-9,false);   

      lc.setLed(6,f,c,false);
      lc.setLed(6,f-2,c-3,false);
      lc.setLed(6,f-4,c-6,false);
      lc.setLed(6,f-6,c-9,false);

      lc.setLed(0,f,7-c,false);
      lc.setLed(0,f-2,4-c,false);
      lc.setLed(0,f-4,1-c,false);
      lc.setLed(0,f-6,-2-c,false);
        
      lc.setLed(1,f,7-c,false);
      lc.setLed(1,f-2,4-c,false);
      lc.setLed(1,f-4,1-c,false);
      lc.setLed(1,f-6,-2-c,false);

    }
  }
}



void aguaMedio(bool activo) {
  motorMedio(activo);
  for (int f=0; f<8; f++) {
    if(pausa||!sistemaEncendido) {
      apagarMotores();
      limpiarMatriz();
      return;
    }
    for (int c=0; c<8; c++) {
      if(pausa||!sistemaEncendido) {
        apagarMotores();
        limpiarMatriz();
        return;
      }
      
      lc.setLed(7,f,c,true);
      lc.setLed(7,f-2,c-3,true);
      lc.setLed(7,f-4,c-6,true);
      lc.setLed(7,f-6,c-9,true);

      lc.setLed(6,f,c,true);
      lc.setLed(6,f-2,c-3,true);
      lc.setLed(6,f-4,c-6,true);
      lc.setLed(6,f-6,c-9,true);

      lc.setLed(5,f,c,true);
      lc.setLed(5,f-2,c-3,true);
      lc.setLed(5,f-4,c-6,true);
      lc.setLed(5,f-6,c-9,true);


      lc.setLed(0,f,7-c,true);
      lc.setLed(0,f-2,4-c,true);
      lc.setLed(0,f-4,1-c,true);
      lc.setLed(0,f-6,-2-c,true);

      lc.setLed(1,f,7-c,true);
      lc.setLed(1,f-2,4-c,true);
      lc.setLed(1,f-4,1-c,true);
      lc.setLed(1,f-6,-2-c,true);

      lc.setLed(2,f,7-c,true);
      lc.setLed(2,f-2,4-c,true);
      lc.setLed(2,f-4,1-c,true);
      lc.setLed(2,f-6,-2-c,true);

      
      if (!delayNoBloqueante(50)) {
        apagarMotores();
        limpiarMatriz();
        return;
      }
      
      lc.setLed(7,f,c,false);
      lc.setLed(7,f-2,c-3,false);
      lc.setLed(7,f-4,c-6,false);
      lc.setLed(7,f-6,c-9,false);

      lc.setLed(6,f,c,false);
      lc.setLed(6,f-2,c-3,false);
      lc.setLed(6,f-4,c-6,false);
      lc.setLed(6,f-6,c-9,false);

      lc.setLed(5,f,c,false);
      lc.setLed(5,f-2,c-3,false);
      lc.setLed(5,f-4,c-6,false);
      lc.setLed(5,f-6,c-9,false);


      lc.setLed(0,f,7-c,false);
      lc.setLed(0,f-2,4-c,false);
      lc.setLed(0,f-4,1-c,false);
      lc.setLed(0,f-6,-2-c,false);

      lc.setLed(1,f,7-c,false);
      lc.setLed(1,f-2,4-c,false);
      lc.setLed(1,f-4,1-c,false);
      lc.setLed(1,f-6,-2-c,false);

      lc.setLed(2,f,7-c,false);
      lc.setLed(2,f-2,4-c,false);
      lc.setLed(2,f-4,1-c,false);
      lc.setLed(2,f-6,-2-c,false);

    }
  }
}

void aguaAlto(bool activo) {
  motorAlto(activo);

  for (int f = 0; f < 8; f++) {
    if (pausa || !sistemaEncendido) {
      apagarMotores();
      limpiarMatriz();
      return;
    }

    for (int c = 0; c < 8; c++) {
      if (pausa || !sistemaEncendido) {
        apagarMotores();
        limpiarMatriz();
        return;
      }

      for (int m = 0; m <= 3; m++) {
        lc.setLed(7-m, f, c, true); lc.setLed(7-m, f-2, c-3, true);
        lc.setLed(7-m, f-4, c-6, true); lc.setLed(7-m, f-6, c-9, true);
        lc.setLed(m, f, 7-c, true); lc.setLed(m, f-2, 4-c, true);
        lc.setLed(m, f-4, 1-c, true); lc.setLed(m, f-6, -2-c, true);
      }

      if (!delayNoBloqueante(30)) {
        apagarMotores();
        limpiarMatriz();
        return;
      }

      for (int m = 0; m <= 3; m++) {
        lc.setLed(7-m, f, c, false); lc.setLed(7-m, f-2, c-3, false);
        lc.setLed(7-m, f-4, c-6, false); lc.setLed(7-m, f-6, c-9, false);
        lc.setLed(m, f, 7-c, false); lc.setLed(m, f-2, 4-c, false);
        lc.setLed(m, f-4, 1-c, false); lc.setLed(m, f-6, -2-c, false);
      }
    }
  }
}


// ================= HARDWARE =================
void inicializarLedControl() {
  for (int i=0;i<8;i++) {lc.shutdown(i,false); lc.setIntensity(i,10); lc.clearDisplay(i);}
}
void inicializarLCD(){ lcd.begin(16,2); }
void inicializarEntradas(){
  pinMode(BTN_ON,INPUT); pinMode(BTN_START,INPUT); pinMode(BUZZER,OUTPUT);
  pinMode(T1,OUTPUT); pinMode(T2,OUTPUT); pinMode(T3,OUTPUT);
  pinMode(N1,OUTPUT); pinMode(N2,OUTPUT); pinMode(N3,OUTPUT);
}
void inicializarMotores(){ pinMode(D0,OUTPUT); pinMode(D1,OUTPUT); }
void motorBajo(bool a){ if(a){digitalWrite(D1,LOW); digitalWrite(D0,HIGH);} else apagarMotores(); }
void motorMedio(bool a){ if(a){digitalWrite(D1,HIGH); digitalWrite(D0,LOW);} else apagarMotores(); }
void motorAlto(bool a){ if(a){digitalWrite(D1,HIGH); digitalWrite(D0,HIGH);} else apagarMotores(); }
void apagarMotores(){ digitalWrite(D0,LOW); digitalWrite(D1,LOW); }
void inicializarLedsTemperatura(){ pinMode(ST1, OUTPUT); pinMode(ST2, OUTPUT); pinMode(ST3, OUTPUT); }

void simularTemperatura() {
  digitalWrite(ST1, LOW); digitalWrite(ST2, LOW); digitalWrite(ST3, LOW);
  if (!lavadoActivo || pausa) return;
  if (nivelTemp == 0) digitalWrite(ST1, HIGH); 
  else if (nivelTemp == 1) digitalWrite(ST2, HIGH); 
  else if (nivelTemp == 2) digitalWrite(ST3, HIGH);
}

void aplicarModoLavado() {
  switch (nivelConfig) {
    case 0: // NORMAL (MANUAL)
      // Ajuste Medio/Medio por defecto para normal
      nivelAgua = 1; nivelTemp = 1; 
      break;
    case 1: // CRISTALES
      nivelAgua = 1; nivelTemp = 0; 
      break;
    case 2: // ECOLOGICO
      nivelAgua = 0; nivelTemp = 0; 
      break;
    case 3: // PRESION
      nivelAgua = 2; nivelTemp = 2; 
      break;
  }
  actualizarLeds();
  // Importante: recalcular tiempo cada vez que se aplica un modo
  calcularTiempo();
}

void apagarTodo() {
  lcd.clear(); lcd.noDisplay(); apagarMotores(); limpiarMatriz();
  digitalWrite(T1,LOW); digitalWrite(T2,LOW); digitalWrite(T3,LOW);
  digitalWrite(N1,LOW); digitalWrite(N2,LOW); digitalWrite(N3,LOW);
  lavadoActivo=false; pausa=false; mostrarMarca=true; nivelTemp = 0; nivelAgua = 0;
  
  // Reiniciar a NORMAL (0) para que aparezca la Gota y la config estandar
  nivelConfig = 0;
  // Forzamos calculo inicial
  calcularTiempo();
  apagarMatrizIconos();
}