/*
  Sistema de Control de Acceso y Seguridad - Variante A
  Arduino UNO R4 WiFi | Proyecto Arquitectura del Computador 2S2026

  RFID: RC522, SS D10, MOSI D11, MISO D12, SCK D13,
        RST conectado fisicamente a 3.3 V (sin pin de reset digital).
  74HC595: SER D11, SRCLK D13, RCLK D6. Conectarlo ANTES
           de los divisores de nivel hacia el RFID.
  Teclado: filas D2,D3,D4,D5; columnas D8,A0,A1,A2.
  LCD I2C: SDA A4, SCL A5, direccion 0x27.
  Buzzer activo: D7. Servo SG90 senal: D9, alimentacion externa 5 V.
  Los GND deben ser comunes. Nunca aplicar 5 V al RC522.

  Tecla * borra PIN; # confirma; D cierra cerrojo; A silencia alarma.
  Usuarios de demostracion: UID 84 C6 BD 06 PIN 1234;
                            UID 43 EE D8 06 PIN 5678.
*/
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>
#include <string.h>

const byte PIN_RFID_SS = 10;
const byte PIN_RFID_RST = MFRC522::UNUSED_PIN;
const byte PIN_595_LATCH = 6;
const byte PIN_BUZZER = 7;
const byte PIN_SERVO = 9;

MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo cerrojo;

const int ANGULO_CERRADO = 90;
const int ANGULO_ABIERTO = 0;
const byte RETARDO_POR_GRADO_MS = 5;

// Mapa invertido segun el teclado fisico que se verifico.
const byte PINES_FILA[4] = {2, 3, 4, 5};
const byte PINES_COL[4] = {8, A0, A1, A2};
const char MAPA[4][4] = {
  {'D', '#', '0', '*'},
  {'C', '9', '8', '7'},
  {'B', '6', '5', '4'},
  {'A', '3', '2', '1'}
};
char teclaAnterior = 0;
char teclaConfirmada = 0;
unsigned long inicioEstabilidad = 0;

// EEPROM: 0-3 encabezado, 4-12 usuario 1, 13-21 usuario 2.
// UID de 4 bytes + cuatro digitos + terminador NUL.
const byte EEPROM_FIRMA_A = 0xA7;
const byte EEPROM_FIRMA_B = 0x4C;
const byte EEPROM_VERSION = 1;
const byte NUM_USUARIOS = 2;
const int EEPROM_BASE = 4;
const byte TAM_USUARIO = 9;

struct Usuario {
  byte uid[4];
  char pin[5];
};
const byte UID_DEMO[2][4] = {
  {0x84, 0xC6, 0xBD, 0x06},
  {0x43, 0xEE, 0xD8, 0x06}
};
const char PIN_DEMO[2][5] = {"1234", "5678"};
Usuario usuarioAutenticando;

enum Estado {ESPERAR_RFID, ESPERAR_PIN, ABIERTO, ALARMA};
Estado estado = ESPERAR_RFID;
byte intentos = 0;
byte cantidadPIN = 0;
char pinDigitado[5] = "";
unsigned long ultimoCambioAlarma = 0;
bool buzzerEncendido = false;
unsigned long ultimoDiagnostico = 0;

void actualizar595(byte valor) {
  // Compartimos SCK y MOSI; el 595 solo actualiza LED al pulsar LATCH.
  digitalWrite(PIN_RFID_SS, HIGH);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_595_LATCH, LOW);
  SPI.transfer(valor);
  digitalWrite(PIN_595_LATCH, HIGH);
  digitalWrite(PIN_595_LATCH, LOW);
  SPI.endTransaction();
}

void led(byte n) {
  if (n >= 1 && n <= 8) actualizar595((byte)(1U << (n - 1)));
}

void pantalla(const char* l1, const char* l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

void bip(byte veces, unsigned int duracion) {
  for (byte i = 0; i < veces; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(duracion);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
}

void configurarTeclado() {
  for (byte i = 0; i < 4; i++) {
    digitalWrite(PINES_FILA[i], HIGH);
    pinMode(PINES_FILA[i], OUTPUT);
    pinMode(PINES_COL[i], INPUT_PULLUP);
  }
}

char escanearTeclado() {
  char detectada = 0;
  for (byte f = 0; f < 4; f++) {
    digitalWrite(PINES_FILA[f], LOW);
    delayMicroseconds(50);
    for (byte c = 0; c < 4; c++) {
      if (digitalRead(PINES_COL[c]) == LOW) {
        detectada = MAPA[f][c];
        break;
      }
    }
    digitalWrite(PINES_FILA[f], HIGH);
    if (detectada) break;
  }
  return detectada;
}

char teclaNueva() {
  char leida = escanearTeclado();
  if (leida != teclaAnterior) {
    teclaAnterior = leida;
    inicioEstabilidad = millis();
  }
  if (millis() - inicioEstabilidad >= 40 && leida != teclaConfirmada) {
    teclaConfirmada = leida;
    if (leida) return leida;
  }
  return 0;
}

int dirUsuario(byte i) {return EEPROM_BASE + TAM_USUARIO * i;}

void escribirUsuario(byte i, const byte uid[4], const char pin[5]) {
  int dir = dirUsuario(i);
  for (byte j = 0; j < 4; j++) EEPROM.update(dir + j, uid[j]);
  for (byte j = 0; j < 5; j++) EEPROM.update(dir + 4 + j, (byte)pin[j]);
}

bool leerUsuario(byte i, Usuario& u) {
  if (i >= NUM_USUARIOS) return false;
  int dir = dirUsuario(i);
  for (byte j = 0; j < 4; j++) u.uid[j] = EEPROM.read(dir + j);
  for (byte j = 0; j < 5; j++) u.pin[j] = (char)EEPROM.read(dir + 4 + j);
  for (byte j = 0; j < 4; j++) {
    if (u.pin[j] < '0' || u.pin[j] > '9') return false;
  }
  return u.pin[4] == '\0';
}

void inicializarUsuarios() {
  bool existe = EEPROM.read(0) == EEPROM_FIRMA_A &&
                EEPROM.read(1) == EEPROM_FIRMA_B &&
                EEPROM.read(2) == EEPROM_VERSION &&
                EEPROM.read(3) == NUM_USUARIOS;
  Usuario u;
  if (existe) {
    for (byte i = 0; i < NUM_USUARIOS; i++) {
      if (!leerUsuario(i, u)) {existe = false; break;}
    }
  }
  if (!existe) {
    Serial.println(F("EEPROM: registrando usuarios iniciales"));
    EEPROM.update(0, 0); // Encabezado invalidado hasta terminar.
    for (byte i = 0; i < NUM_USUARIOS; i++) {
      escribirUsuario(i, UID_DEMO[i], PIN_DEMO[i]);
    }
    EEPROM.update(3, NUM_USUARIOS);
    EEPROM.update(2, EEPROM_VERSION);
    EEPROM.update(1, EEPROM_FIRMA_B);
    EEPROM.update(0, EEPROM_FIRMA_A);
  } else {
    Serial.println(F("EEPROM: usuarios existentes"));
  }
  for (byte i = 0; i < NUM_USUARIOS; i++) {
    if (leerUsuario(i, u)) {
      Serial.print(F("Usuario ")); Serial.print(i + 1);
      Serial.print(F(" UID: "));
      for (byte j = 0; j < 4; j++) {
        if (u.uid[j] < 16) Serial.print('0');
        Serial.print(u.uid[j], HEX); Serial.print(' ');
      }
      Serial.println();
    }
  }
}

bool autenticarRFID() {
  if (rfid.uid.size != 4) return false;
  Usuario u;
  for (byte i = 0; i < NUM_USUARIOS; i++) {
    if (leerUsuario(i, u) && memcmp(rfid.uid.uidByte, u.uid, 4) == 0) {
      usuarioAutenticando = u;
      Serial.print(F("RFID valido: usuario "));
      Serial.println(i + 1);
      return true;
    }
  }
  return false;
}

void volverAEspera() {
  estado = ESPERAR_RFID;
  intentos = 0;
  cantidadPIN = 0;
  pinDigitado[0] = '\0';
  buzzerEncendido = false;
  digitalWrite(PIN_BUZZER, LOW);
  led(2);
  pantalla("CONTROL ACCESO", "PRESENTE TARJETA");
  Serial.println(F("ESPERANDO RFID"));
}

void mostrarPIN() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("INGRESE SU PIN");
  lcd.setCursor(0, 1);
  for (byte i = 0; i < 4; i++) lcd.print(i < cantidadPIN ? '*' : '_');
}

void moverCerrojo(int desde, int hasta) {
  int paso = (hasta >= desde) ? 1 : -1;
  for (int a = desde; a != hasta; a += paso) {
    cerrojo.write(a);
    delay(RETARDO_POR_GRADO_MS);
  }
  cerrojo.write(hasta);
  delay(RETARDO_POR_GRADO_MS);
}

void abrirCerrojo() {
  pantalla("ACCESO AUTORIZ.", "ABRIENDO...");
  led(6);
  moverCerrojo(ANGULO_CERRADO, ANGULO_ABIERTO);
  estado = ABIERTO;
  pantalla("CERROJO ABIERTO", "D PARA CERRAR");
  Serial.println(F("CERROJO ABIERTO - esperando D"));
}

void cerrarCerrojo() {
  pantalla("CERRANDO", "CERROJO...");
  moverCerrojo(ANGULO_ABIERTO, ANGULO_CERRADO);
  bip(1, 120);
  pantalla("CERROJO CERRADO", "ACCESO SEGURO");
  Serial.println(F("CERROJO CERRADO"));
  delay(500);
  volverAEspera();
}

void iniciarAlarma() {
  estado = ALARMA;
  led(8);
  pantalla("ACCESO BLOQUEADO", "ALARMA ACTIVADA");
  buzzerEncendido = true;
  digitalWrite(PIN_BUZZER, HIGH);
  ultimoCambioAlarma = millis();
  Serial.println(F("ALARMA INDEFINIDA - presione A"));
}

void procesarAlarma() {
  if (millis() - ultimoCambioAlarma >= 250) {
    ultimoCambioAlarma = millis();
    buzzerEncendido = !buzzerEncendido;
    digitalWrite(PIN_BUZZER, buzzerEncendido ? HIGH : LOW);
  }
  if (teclaNueva() == 'A') {
    digitalWrite(PIN_BUZZER, LOW);
    pantalla("ALARMA", "DESACTIVADA");
    delay(500);
    volverAEspera();
  }
}

void verificarPIN() {
  led(5);
  pantalla("VALIDANDO PIN", "ESPERE...");
  delay(250);
  if (strcmp(pinDigitado, usuarioAutenticando.pin) == 0) {
    Serial.println(F("PIN correcto"));
    led(6);
    bip(2, 150);
    abrirCerrojo();
    return;
  }
  intentos++;
  Serial.print(F("PIN incorrecto, intento "));
  Serial.println(intentos);
  led(7);
  if (intentos >= 3) {
    pantalla("ACCESO DENEGADO", "3 INTENTOS");
    bip(3, 120);
    delay(400);
    iniciarAlarma();
    return;
  }
  pantalla("PIN INCORRECTO", "REINTENTE");
  bip(2, 100);
  delay(650);
  cantidadPIN = 0;
  pinDigitado[0] = '\0';
  led(4);
  mostrarPIN();
}

void procesarPIN() {
  char tecla = teclaNueva();
  if (!tecla) return;
  if (tecla == '*') {
    cantidadPIN = 0;
    pinDigitado[0] = '\0';
    mostrarPIN();
    return;
  }
  if (tecla == '#') {
    if (cantidadPIN == 4) verificarPIN();
    else {
      pantalla("PIN INCOMPLETO", "USE 4 DIGITOS");
      delay(650);
      mostrarPIN();
    }
    return;
  }
  if (tecla >= '0' && tecla <= '9' && cantidadPIN < 4) {
    pinDigitado[cantidadPIN++] = tecla;
    pinDigitado[cantidadPIN] = '\0';
    mostrarPIN();
  }
}

void procesarRFID() {
  if (millis() - ultimoDiagnostico >= 3000) {
    ultimoDiagnostico = millis();
    Serial.println(F("BUSCANDO RFID..."));
  }
  if (!rfid.PICC_IsNewCardPresent()) return;
  Serial.println(F("TARJETA DETECTADA"));
  if (!rfid.PICC_ReadCardSerial()) {
    Serial.println(F("FALLO AL LEER UID"));
    return;
  }
  led(3);
  pantalla("VALIDANDO RFID", "ESPERE...");
  Serial.print(F("UID: "));
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 16) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
  bip(1, 100);
  bool existe = autenticarRFID();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  if (!existe) {
    led(7);
    pantalla("TARJETA INVALIDA", "ACCESO DENEGADO");
    bip(3, 100);
    delay(900);
    volverAEspera();
    return;
  }
  estado = ESPERAR_PIN;
  intentos = 0;
  cantidadPIN = 0;
  pinDigitado[0] = '\0';
  led(4);
  mostrarPIN();
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_RFID_SS, HIGH);
  pinMode(PIN_RFID_SS, OUTPUT);
  digitalWrite(PIN_595_LATCH, LOW);
  pinMode(PIN_595_LATCH, OUTPUT);
  configurarTeclado();
  SPI.begin();
  Wire.begin();
  lcd.init();
  lcd.backlight();
  cerrojo.attach(PIN_SERVO);
  cerrojo.write(ANGULO_CERRADO);
  led(1);
  pantalla("SISTEMA ACCESO", "INICIANDO...");
  Serial.println(F("=== CONTROL ACCESO UNO R4 WIFI ==="));
  delay(900);
  inicializarUsuarios();
  rfid.PCD_Init();
  delay(150);
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("VERSION RFID: 0x"));
  if (version < 16) Serial.print('0');
  Serial.println(version, HEX);
  if (version == 0x00 || version == 0xFF) {
    led(7);
    pantalla("ERROR RFID", "REVISAR CABLES");
    Serial.println(F("ERROR COMUNICACION RFID - verificar SPI, 3.3V y GND"));
    while (true) delay(1000);
  }
  // Secuencia visual de los 8 LEDs.
  for (byte i = 1; i <= 8; i++) {
    led(i);
    delay(110);
  }
  volverAEspera();
}

void loop() {
  switch (estado) {
    case ESPERAR_RFID: procesarRFID(); break;
    case ESPERAR_PIN: procesarPIN(); break;
    case ABIERTO:
      if (teclaNueva() == 'D') cerrarCerrojo();
      break;
    case ALARMA: procesarAlarma(); break;
  }
}
