#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

// Bibliotecas para audio
#include "Audio.h"
#include "SD.h"
#include "FS.h"

// SERVOS
Servo BrazoDerecho;
Servo BrazoIzquierdo;
int PosBrazoDerecho = 0;
int PosBrazoIzquierdo = 0;
#define tiempobrazos 300

// NEOPIXEL
#define PIN_WS2812B 21
#define NUM_PIXELS 26
Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);

// Definir las diferentes zonas del robot
#define BASE_START 0
#define BASE_END 20
#define DIENTES_START 21
#define DIENTES_END 23
#define OJOS_START 24
#define OJOS_END 25

// Variables para efectos de luces
unsigned long ultimoTiempoEfecto = 0;
int velocidadAnimacion = 50;
int pasoEfecto = 0;

// Configuración de la tarjeta SD para audio
#define SD_CS 5
#define SPI_MOSI 13
#define SPI_MISO 15
#define SPI_SCK 14

// Configuración I2S para audio
#define I2S_DOUT 4
#define I2S_BCLK 26
#define I2S_LRC 25

// Objeto de audio
Audio audio;

// Variables para control de audio
bool reproduciendoAudio = false;
unsigned long ultimoAudio = 0;
const int intervaloAudioAleatorio = 20000; // 20 segundos entre audios aleatorios
String ultimoArchivoReproducido = "";
const int numArchivosAudio = 9;
String archivosAudio[numArchivosAudio] = {"1.wav", "2.wav", "3.wav", "4.wav", "5.wav", "6.wav", "7.wav", "8.wav", "berserk_2.wav"};

// MOTORES
#define BrazoDerechoPin 32
#define BrazoIzquierdoPin 33
#define MotorENAPin 17 //Cambiado del 23 al 17 (La etiqueta no se ha cambiado)
#define MotorIN1Pin 27
#define MotorIN2Pin 12
#define MotorENBPin 22
#define MotorIN3Pin 18
#define MotorIN4Pin 19

// CONTROL
int Conexionperdida = 0;
int ConexionperdidaAnterior = 0;
int angulo = 0;
int modulo = 0;
int Boton = 0;
int velmin = 80;
int Potencia = 0;
int PotenciaMR = 0;
int PotenciaML = 0;

// Variable para controlar los brazos
bool empujando = false;

// PAQUETE DE DATOS
struct PacketData {
  long lastConnection;
  int AnguloValue;
  int ModuloValue;
  int switchPressed;
};
PacketData receiverData;
unsigned long lastRecvTime = 0;
#define SIGNAL_TIMEOUT 1000

// CALLBACK DATOS
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == 0) return;
  memcpy(&receiverData, incomingData, sizeof(receiverData));
  lastRecvTime = millis();
}

void MapeoJoystick() {
  angulo = receiverData.AnguloValue;
  modulo = receiverData.ModuloValue;
  if (modulo > 255) modulo = 255;
  Boton = receiverData.switchPressed;
}

// NEOPIXEL
int PixelEstado = 0;
int PixelTimer = 0;

void PixelInicializar() {
  PixelEstado = 0;
  PixelTimer = 0;
  ultimoTiempoEfecto = 0;
  pasoEfecto = 0;
}

// Colores predefinidos
uint32_t colorVerde = ws2812b.Color(0, 255, 0);
uint32_t colorRojo = ws2812b.Color(255, 0, 0);
uint32_t colorAzul = ws2812b.Color(0, 0, 255);
uint32_t colorBlanco = ws2812b.Color(255, 255, 255);
uint32_t colorAmarillo = ws2812b.Color(255, 255, 0);
uint32_t colorMorado = ws2812b.Color(128, 0, 128);
uint32_t colorCian = ws2812b.Color(0, 255, 255);

// Función para verificar si audio está sonando
bool audioEstaReproduciendo() {
  return audio.isRunning();
}

// Función para reproducir un archivo de audio
void reproducirAudio(String nombreArchivo) {
  // Solo reproducir si no hay otro audio sonando
  if (!audioEstaReproduciendo()) {
    if (SD.exists("/" + nombreArchivo)) {
      Serial.println("Reproduciendo archivo " + nombreArchivo);
      audio.stopSong();
      delay(50); // Pausa corta para asegurar que se detuvo correctamente
      String path = "/" + nombreArchivo;
      audio.connecttoFS(SD, path.c_str()); // Corregido con c_str()
      delay(50); // Pequeña pausa para iniciar la reproducción
      ultimoArchivoReproducido = nombreArchivo;
      reproduciendoAudio = true;
      ultimoAudio = millis(); // Actualizar timer después de cualquier audio
    } else {
      Serial.println("ERROR: No se encuentra el archivo " + nombreArchivo);
    }
  } else {
    Serial.println("No se puede reproducir " + nombreArchivo + ", ya hay un audio en curso");
  }
}

// Función para reproducir un archivo aleatorio
void reproducirAudioAleatorio() {
  if (!audioEstaReproduciendo() && millis() - ultimoAudio > intervaloAudioAleatorio) {
    // Elegir un archivo aleatorio que no sea el último reproducido
    String archivoElegido;
    do {
      int indice = random(numArchivosAudio);
      archivoElegido = archivosAudio[indice];
    } while (archivoElegido == ultimoArchivoReproducido);
    
    reproducirAudio(archivoElegido);
    ultimoAudio = millis();
  }
}

// Mantener ojos con color específico
void mantenerOjos(uint32_t color) {
  for (int i = OJOS_START; i <= OJOS_END; i++) {
    ws2812b.setPixelColor(i, color);
  }
}

// Controlar las luces de los dientes
void efectoDientesChattering() {
  static unsigned long lastUpdate = 0;
  static bool encendido = false;
  
  if (audioEstaReproduciendo()) {
    // Cuando hay audio, parpadear rápidamente con color cian
    if (millis() - lastUpdate > 30) { // Parpadeo rápido (30ms)
      if (encendido) {
        for (int i = DIENTES_START; i <= DIENTES_END; i++) {
          ws2812b.setPixelColor(i, 0); // Apagar
        }
      } else {
        for (int i = DIENTES_START; i <= DIENTES_END; i++) {
          ws2812b.setPixelColor(i, colorBlanco); // Encender con color cian
        }
      }
      encendido = !encendido;
      lastUpdate = millis();
    }
  } else {
    // Cuando no hay audio, mantener los dientes encendidos con luz blanca
    for (int i = DIENTES_START; i <= DIENTES_END; i++) {
      ws2812b.setPixelColor(i, colorBlanco);
    }
  }
}

// Efecto arcoíris para la base
void efectoArcoiris() {
  if (millis() - ultimoTiempoEfecto > 10) {
    // Solo aplicar arcoiris a la base
    for (int i = BASE_START; i <= BASE_END; i++) {
      int pos = (i * 256 / (BASE_END - BASE_START)) + pasoEfecto;
      ws2812b.setPixelColor(i, ws2812b.ColorHSV(pos * 256, 255, 255));
    }
    
    pasoEfecto = (pasoEfecto + 1) % 256;
    ultimoTiempoEfecto = millis();
  }
}

// Expresión para robot enojado
void robotEnojado() {
  // Base: rojo
  for (int i = BASE_START; i <= BASE_END; i++) {
    ws2812b.setPixelColor(i, colorRojo);
  }
  
  // Dientes: amarillo pero parpadeando si está hablando
  if (!audioEstaReproduciendo()) {
    // Si no hay audio, dientes fijos en amarillo
    for (int i = DIENTES_START; i <= DIENTES_END; i++) {
      ws2812b.setPixelColor(i, colorAmarillo);
    }
  } // Si hay audio, los dientes se controlarán en efectoDientesChattering()
  
  // Ojos: rojo intenso
  for (int i = OJOS_START; i <= OJOS_END; i++) {
    ws2812b.setPixelColor(i, colorRojo);
  }
  
  ws2812b.show();
}

// Luces de policía para la base
void luzPoliciaClasica() {
  static unsigned long lastUpdate = 0;
  static bool esAzul = true;
  
  if (millis() - lastUpdate > velocidadAnimacion) {
    // Aplicar solo a la base
    for (int i = BASE_START; i <= BASE_END; i++) {
      ws2812b.setPixelColor(i, esAzul ? colorAzul : colorRojo);
    }
    esAzul = !esAzul;
    lastUpdate = millis();
  }
}

// Actualización de efectos según la condición del robot
void actualizarEfectos() {
  // Dientes siempre están controlados por efectoDientesChattering()
  efectoDientesChattering();
  
  // Si los brazos están empujando (pala arriba), modo policía
  if (empujando) {
    luzPoliciaClasica();
  } else {
    // Base siempre arcoíris cuando no está en modo policía
    efectoArcoiris();
  }
  
  // Actualiza los colores
  ws2812b.show();
}

void BucleNeopixel() {
  switch (PixelEstado) {
    case 0: // Inicialización
      ws2812b.clear();
      ws2812b.show();
      PixelTimer = millis();
      PixelEstado = 1;
      break;
      
    case 1: // Estado de decisión
      if (Conexionperdida == 1) {
        // Robot enojado cuando pierde conexión
        if (!audioEstaReproduciendo()) {
          robotEnojado();
        }
        PixelEstado = 2;
      } else {
        // Efectos de luces cuando hay conexión
        PixelEstado = 3;
      }
      break;
      
    case 2: // Estado de desconexión
      if (Conexionperdida == 0) {
        PixelEstado = 1;
        pasoEfecto = 0;
        PixelTimer = millis();
      } else {
        // Base y ojos rojos (ya configurados en robotEnojado)
        // Pero procesar los dientes por separado para permitir parpadeo al hablar
        if (audioEstaReproduciendo()) {
          // Si está reproduciendo audio, permitir que los dientes parpadeen
          efectoDientesChattering();
          ws2812b.show();
        } else {
          // Si no hay audio, los dientes ya están configurados por robotEnojado()
        }
      }
      break;
      
    case 3: // Efectos de luces
      if (Conexionperdida == 1) {
        PixelEstado = 1;
      } else {
        actualizarEfectos();
      }
      break;
  }
}

// BRAZOS
int EstadoEmpujar = 0;
int TimerEmpujar = 0;

void InicializarEmpujar() {
  EstadoEmpujar = 0;
  TimerEmpujar = 0;
  empujando = false;
  PosBrazoDerecho = 70;
  PosBrazoIzquierdo = 180 - PosBrazoDerecho;
}

void MoverBrazos() {
  BrazoDerecho.write(PosBrazoDerecho);
  BrazoIzquierdo.write(PosBrazoIzquierdo);
}

void CambiarPosicionBrazos() {
  if (!empujando) {
    empujando = true;
    PosBrazoDerecho = 170;
    PosBrazoIzquierdo = 180 - PosBrazoDerecho;
  } else {
    empujando = false;
    PosBrazoDerecho = 70;
    PosBrazoIzquierdo = 180 - PosBrazoDerecho;
  }
}

void Empujar() {
  switch (EstadoEmpujar) {
    case 0:
      MoverBrazos();
      if (receiverData.switchPressed) EstadoEmpujar = 1;
      break;
    case 1:
      CambiarPosicionBrazos();
      MoverBrazos();
      TimerEmpujar = millis();
      EstadoEmpujar = 2;
      break;
    case 2:
      MoverBrazos();
      if (millis() >= TimerEmpujar + tiempobrazos) {
        TimerEmpujar = 0;
        EstadoEmpujar = 0;
      }
      break;
  }
}

// MOVIMIENTO
int EstadoMovimiento = 0;
int TimerMovimiento = 0;

void IniMovimiento() {
  EstadoMovimiento = 0;
  TimerMovimiento = 0;
}

void Movimiento() {
  switch (EstadoMovimiento) {
    case 0:
      EstadoMovimiento = 1;
      break;
    case 1:
      if (modulo > 0) {
        Potencia = map(modulo, 0, 255, velmin, 255);
        
        // Dirección hacia adelante (ojos verdes)
        if (angulo >= 330 || angulo <= 30) {
          mantenerOjos(colorVerde);
          PotenciaML = Potencia;
          PotenciaMR = Potencia;
          adelante();
        } else if (angulo >= 300 && angulo < 330) {
          mantenerOjos(colorVerde);
          PotenciaML = Potencia;
          PotenciaMR = velmin;
          adelante();
        } else if (angulo > 240 && angulo <= 300) {
          mantenerOjos(colorVerde);
          PotenciaML = 200;
          PotenciaMR = 0;
          adelante();
        } else if (angulo > 30 && angulo <= 60) {
          mantenerOjos(colorVerde);
          PotenciaML = velmin;
          PotenciaMR = Potencia;
          adelante();
        } else if (angulo > 60 && angulo <= 120) {
          mantenerOjos(colorVerde);
          PotenciaML = 0;
          PotenciaMR = 200;
          adelante();
        } 
        // Dirección hacia atrás (ojos rojos)
        else if (angulo > 120 && angulo <= 150) {
          mantenerOjos(colorRojo);
          PotenciaML = velmin;
          PotenciaMR = Potencia;
          atras();
        } else if (angulo >= 150 && angulo <= 210) {
          mantenerOjos(colorRojo);
          PotenciaML = Potencia;
          PotenciaMR = Potencia;
          atras();
        } else if (angulo > 210 && angulo < 240) {
          mantenerOjos(colorRojo);
          PotenciaML = Potencia;
          PotenciaMR = velmin;
          atras();
        }
      } else {
        // Parado - ojos blancos
        mantenerOjos(colorBlanco);
        parar();
      }
      break;
  }
}

// MOTORES
void MRparado() {
  analogWrite(MotorENAPin, 0);
  digitalWrite(MotorIN1Pin, LOW);
  digitalWrite(MotorIN2Pin, LOW);
}
void MLparado() {
  analogWrite(MotorENBPin, 0);
  digitalWrite(MotorIN3Pin, LOW);
  digitalWrite(MotorIN4Pin, LOW);
}
void MRadelante() {
  analogWrite(MotorENAPin, PotenciaMR);
  digitalWrite(MotorIN1Pin, HIGH);
  digitalWrite(MotorIN2Pin, LOW);
}
void MLadelante() {
  analogWrite(MotorENBPin, PotenciaML);
  digitalWrite(MotorIN3Pin, LOW);
  digitalWrite(MotorIN4Pin, HIGH);
}
void MRatras() {
  analogWrite(MotorENAPin, PotenciaMR);
  digitalWrite(MotorIN1Pin, LOW);
  digitalWrite(MotorIN2Pin, HIGH);
}
void MLatras() {
  analogWrite(MotorENBPin, PotenciaML);
  digitalWrite(MotorIN3Pin, HIGH);
  digitalWrite(MotorIN4Pin, LOW);
}
void parar() {
  MRparado();
  MLparado();
}
void adelante() {
  MRadelante();
  MLadelante();
}
void atras() {
  MRatras();
  MLatras();
}

// Función para listar directorios de la SD
void listDir(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void setup() {
  // Inicializar semilla aleatoria
  randomSeed(analogRead(0));
  
  // Motores
  pinMode(MotorENAPin, OUTPUT);
  pinMode(MotorIN1Pin, OUTPUT);
  pinMode(MotorIN2Pin, OUTPUT);
  pinMode(MotorENBPin, OUTPUT);
  pinMode(MotorIN3Pin, OUTPUT);
  pinMode(MotorIN4Pin, OUTPUT);

  Serial.begin(115200);
  Serial.println("\n\n--- Iniciando robot Share Horizons ---");

  // Inicializar SD Card para audio
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar la tarjeta SD");
    // Intenta nuevamente
    delay(500);
    if (!SD.begin(SD_CS)) {
      Serial.println("Fallo en la segunda inicialización de SD");
    } else {
      Serial.println("SD inicializada correctamente en segundo intento");
    }
  } else {
    Serial.println("SD inicializada correctamente");
    
    // Listar archivos en la raíz para verificar
    File root = SD.open("/");
    Serial.println("Archivos en tarjeta SD:");
    listDir(root, 0);
  }

  // Configuración extra para estabilidad
  Serial.println("Configurando audio I2S con parámetros extendidos...");
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21);
  audio.setBufsize(1024*8, 1024*8); // Aumenta el tamaño del buffer

  // Un pequeño delay para permitir que I2S se inicialice correctamente
  delay(100);
  
  // Reproducir archivo de inicio
  if (SD.exists("/inicio.wav")) {
    Serial.println("Reproduciendo archivo inicio.wav");
    delay(100); // Pequeña pausa antes de iniciar el audio
    audio.connecttoFS(SD, "/inicio.wav");
    ultimoArchivoReproducido = "inicio.wav";
    reproduciendoAudio = true;
    ultimoAudio = millis();
    
    // Esperar a que se inicie la reproducción
    delay(50);
  } else {
    Serial.println("ERROR: No se encuentra el archivo inicio.wav");
  }

  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.print("Error initializing ESP-NOW\n");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // Servos
  BrazoDerecho.setPeriodHertz(50);
  BrazoIzquierdo.setPeriodHertz(50);
  BrazoDerecho.attach(BrazoDerechoPin, 500, 2500);
  BrazoIzquierdo.attach(BrazoIzquierdoPin, 500, 2500);

  IniMovimiento();
  InicializarEmpujar();
  
  // Inicializar NeoPixel
  ws2812b.begin();
  PixelInicializar();
  
  // Test inicial de luces
  for (int i = 0; i < NUM_PIXELS; i++) {
    ws2812b.setPixelColor(i, colorVerde);
    ws2812b.show();
    delay(50);
  }
  ws2812b.clear();
  ws2812b.show();
  
  // Inicializar el estado anterior de conexión
  ConexionperdidaAnterior = 1; // Asumimos que empieza desconectado
}

void loop() {
  // Gestionar reproducción de audio (no bloqueante)
  audio.loop();
  
  // Actualizar estado de reproducción
  reproduciendoAudio = audioEstaReproduciendo();
  
  // Guardar el estado anterior de conexión
  ConexionperdidaAnterior = Conexionperdida;
  
  // Verificar conexión
  if ((millis() - lastRecvTime) > SIGNAL_TIMEOUT) {
    if (Conexionperdida == 0) {
      Serial.println("MANDO OUT - Conexión perdida");
      // Reproducir audio de desconexión
      reproducirAudio("dormido.wav");
    }
    Conexionperdida = 1;
    parar();
    IniMovimiento();
  } else {
    if (Conexionperdida == 1) {
      Serial.println("MANDO IN - Conexión recuperada");
      // Reproducir audio de conexión
      reproducirAudio("cambio_berserk.wav");
    }
    Conexionperdida = 0;
    MapeoJoystick();
    
    // Reproducir audio aleatorio si estamos conectados y ha pasado el tiempo de intervalo
    reproducirAudioAleatorio();
    
    // Imprime valores del joystick cada segundo
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      Serial.print("Joystick - Angulo: ");
      Serial.print(angulo);
      Serial.print(", Modulo: ");
      Serial.print(modulo);
      Serial.print(", Botón: ");
      Serial.println(Boton);
      lastPrint = millis();
    }
    Movimiento();
    Empujar();
  }
  BucleNeopixel();
}
