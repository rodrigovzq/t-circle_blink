#include <Arduino.h>
#include <LittleFS.h>
#include "FastLED.h"
#include "pin_config.h"
#include "Arduino_DriveBus_Library.h"
#include "arduinoFFT.h"
#include "Arduino_GFX_Library.h"

// VERSION DE PRUEBA
#define VERSION "v1.0-test4"

#define DATA_PIN APA102_DATA
#define CLOCK_PIN APA102_CLOCK
#define BUTTON_PIN 0

// Parámetros del modelo
#define SAMPLE_RATE 44100
#define DURATION_SEC 4
#define N_SAMPLES (SAMPLE_RATE * DURATION_SEC)
#define N_MFCC 128
#define N_FFT 2048
#define HOP_LENGTH 512
#define N_FRAMES 345
#define N_MELS 128

// Normalización
#define MFCC_MEAN -3.419721f
#define MFCC_STD 51.893776f

// Colores para la pantalla (RGB565)
#define COLOR_BG 0x0000      // Negro
#define COLOR_PRIMARY 0x07FF // Cyan
#define COLOR_SUCCESS 0x07E0 // Verde
#define COLOR_WARNING 0xFFE0 // Amarillo
#define COLOR_ERROR 0xF800   // Rojo
#define COLOR_TEXT 0xFFFF    // Blanco
#define COLOR_GRAY 0x8410    // Gris

CRGB leds[1];

std::shared_ptr<Arduino_IIS_DriveBus> IIS_Bus =
    std::make_shared<Arduino_HWIIS>(I2S_NUM_0, MSM261_BCLK, MSM261_WS, MSM261_DATA);
std::unique_ptr<Arduino_IIS> Microphone(new Arduino_MEMS(IIS_Bus));

// Display
Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

// Buffers
int16_t *audioBuffer = nullptr;
float *mfccOutput = nullptr;
size_t writeIndex = 0;
bool recording = false;

// FFT
ArduinoFFT<float> FFT;
float *vReal = nullptr;
float *vImag = nullptr;

// Mel filterbank y DCT
float *melFilterbank = nullptr;
float *dctMatrix = nullptr;
float *hammingWindow = nullptr;

// Métricas
unsigned long timeCapture = 0;
unsigned long timeMFCC = 0;
unsigned long timeTotal = 0;

// ============================================
// UI: Pantalla de inicio
// ============================================
void showStartScreen()
{
  gfx->fillScreen(COLOR_BG);

  // Título (centrado)
  gfx->setTextColor(COLOR_PRIMARY);
  gfx->setTextSize(2);
  gfx->setCursor(32, 40);
  gfx->println("MoodLink");

  // Versión (centrado)
  gfx->setTextColor(COLOR_GRAY);
  gfx->setTextSize(1);
  gfx->setCursor(45, 65);
  gfx->println("Test MFCC");

  // Instrucción (centrado)
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(1);
  gfx->setCursor(28, 100);
  gfx->println("Presiona BTN");
  gfx->setCursor(28, 115);
  gfx->println("para grabar");

  // Indicador de estado (centrado)
  // gfx->fillCircle(80, 140, 5, COLOR_SUCCESS);
  gfx->setTextColor(COLOR_SUCCESS);
  gfx->setCursor(65, 136);
  gfx->println("Listo");
}

// ============================================
// UI: Pantalla de grabación
// ============================================
void showRecordingScreen(float progress)
{
  gfx->fillScreen(COLOR_BG);

  // Título (centrado)
  gfx->setTextColor(COLOR_ERROR);
  gfx->setTextSize(2);
  gfx->setCursor(32, 30);
  gfx->println("GRABANDO");

  // Tiempo transcurrido (centrado)
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(1);
  int elapsed = (int)(progress * DURATION_SEC);
  gfx->setCursor(34, 60);
  gfx->printf("%d.0s / 4.0s", elapsed);

  // Barra de progreso circular
  int centerX = 80;
  int centerY = 100;
  int radius = 35;

  // Fondo del círculo
  gfx->drawCircle(centerX, centerY, radius, COLOR_GRAY);

  // Arco de progreso
  int angle = (int)(progress * 360);
  for (int i = 0; i < angle; i++)
  {
    float rad = (i - 90) * PI / 180.0;
    int x1 = centerX + (radius - 3) * cos(rad);
    int y1 = centerY + (radius - 3) * sin(rad);
    int x2 = centerX + radius * cos(rad);
    int y2 = centerY + radius * sin(rad);
    gfx->drawLine(x1, y1, x2, y2, COLOR_ERROR);
  }

  // Porcentaje en el centro
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_TEXT);
  gfx->setCursor(centerX - 20, centerY - 10);
  gfx->printf("%d%%", (int)(progress * 100));
}

// ============================================
// UI: Pantalla de procesamiento
// ============================================
void showProcessingScreen(int currentFrame, int totalFrames)
{
  static unsigned long lastUpdate = 0;

  // Actualizar solo cada 100ms para no saturar
  if (millis() - lastUpdate < 100)
    return;
  lastUpdate = millis();

  gfx->fillScreen(COLOR_BG);

  // Título (centrado)
  gfx->setTextColor(COLOR_WARNING);
  gfx->setTextSize(2);
  gfx->setCursor(20, 30);
  gfx->println("Procesando");

  // Progreso (centrado)
  float progress = (float)currentFrame / totalFrames;
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(1);
  gfx->setCursor(22, 60);
  gfx->printf("Frame %d/%d", currentFrame, totalFrames);

  // Barra horizontal (centrada)
  int barX = 20;
  int barY = 85;
  int barW = 120;
  int barH = 15;

  gfx->drawRect(barX, barY, barW, barH, COLOR_GRAY);
  gfx->fillRect(barX + 2, barY + 2, (int)((barW - 4) * progress), barH - 4, COLOR_WARNING);

  // Porcentaje (centrado)
  gfx->setTextSize(2);
  gfx->setCursor(60, 110);
  gfx->printf("%d%%", (int)(progress * 100));

  // Spinner animado (centrado)
  static int spinnerAngle = 0;
  spinnerAngle = (spinnerAngle + 30) % 360;
  int cx = 80, cy = 140, r = 8;
  float rad = spinnerAngle * PI / 180.0;
  gfx->drawLine(cx, cy, cx + r * cos(rad), cy + r * sin(rad), COLOR_WARNING);
}

// ============================================
// UI: Pantalla de resultados
// ============================================
void showResultsScreen()
{
  gfx->fillScreen(COLOR_BG);

  // Título (centrado)
  gfx->setTextColor(COLOR_SUCCESS);
  gfx->setTextSize(2);
  gfx->setCursor(32, 20);
  gfx->println("Completo");

  // Métricas (centradas)
  gfx->setTextColor(COLOR_TEXT);
  gfx->setTextSize(1);

  gfx->setCursor(20, 50);
  gfx->printf("Captura: %lums", timeCapture);

  gfx->setCursor(20, 65);
  gfx->printf("MFCCs:   %lums", timeMFCC);

  gfx->setCursor(20, 80);
  gfx->printf("Total:   %lums", timeTotal);

  // Línea separadora (centrada)
  gfx->drawLine(20, 95, 140, 95, COLOR_GRAY);

  // Primer MFCC (centrado)
  gfx->setCursor(28, 105);
  gfx->println("MFCC[0][0]:");
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_PRIMARY);
  gfx->setCursor(40, 120);
  gfx->printf("%.2f", mfccOutput[0]);

  // Indicador de éxito (centrado)
  gfx->fillCircle(80, 148, 4, COLOR_SUCCESS);
}

// ============================================
// Inicialización de Hamming Window
// ============================================
void initHammingWindow()
{
  for (int i = 0; i < N_FFT; i++)
  {
    hammingWindow[i] = 0.54f - 0.46f * cos(2.0f * PI * i / (N_FFT - 1));
  }
  Serial.println("✅ Hamming window inicializada");
}

// ============================================
// Inicialización de Mel Filterbank
// ============================================
void initMelFilterbank()
{
  auto hzToMel = [](float hz)
  { return 2595.0f * log10(1.0f + hz / 700.0f); };
  auto melToHz = [](float mel)
  { return 700.0f * (pow(10.0f, mel / 2595.0f) - 1.0f); };

  float melMin = hzToMel(0);
  float melMax = hzToMel(SAMPLE_RATE / 2.0f);

  float melPoints[N_MELS + 2];
  for (int i = 0; i < N_MELS + 2; i++)
  {
    melPoints[i] = melMin + (melMax - melMin) * i / (N_MELS + 1);
  }

  int bins[N_MELS + 2];
  for (int i = 0; i < N_MELS + 2; i++)
  {
    float hz = melToHz(melPoints[i]);
    bins[i] = (int)floor((N_FFT + 1) * hz / SAMPLE_RATE);
  }

  memset(melFilterbank, 0, N_MELS * (N_FFT / 2 + 1) * sizeof(float));

  for (int m = 0; m < N_MELS; m++)
  {
    int leftBin = bins[m];
    int centerBin = bins[m + 1];
    int rightBin = bins[m + 2];

    for (int k = leftBin; k < centerBin; k++)
    {
      melFilterbank[m * (N_FFT / 2 + 1) + k] =
          (float)(k - leftBin) / (centerBin - leftBin);
    }

    for (int k = centerBin; k < rightBin; k++)
    {
      melFilterbank[m * (N_FFT / 2 + 1) + k] =
          (float)(rightBin - k) / (rightBin - centerBin);
    }
  }

  Serial.println("✅ Mel filterbank inicializada");
}

// ============================================
// Inicialización de DCT Matrix
// ============================================
void initDCTMatrix()
{
  for (int i = 0; i < N_MFCC; i++)
  {
    for (int j = 0; j < N_MELS; j++)
    {
      dctMatrix[i * N_MELS + j] =
          cos(PI * i * (j + 0.5f) / N_MELS) * sqrt(2.0f / N_MELS);
    }
  }
  Serial.println("✅ DCT matrix inicializada");
}

// ============================================
// Extraer MFCCs de un frame
// ============================================
void extractFrameMFCC(int frameIndex, float *mfccs)
{
  int offset = frameIndex * HOP_LENGTH;

  for (int i = 0; i < N_FFT; i++)
  {
    if (offset + i < N_SAMPLES)
    {
      vReal[i] = audioBuffer[offset + i] * hammingWindow[i];
    }
    else
    {
      vReal[i] = 0;
    }
    vImag[i] = 0;
  }

  FFT.compute(vReal, vImag, N_FFT, FFTDirection::Forward);

  float magnitude[N_FFT / 2 + 1];
  for (int i = 0; i < N_FFT / 2 + 1; i++)
  {
    magnitude[i] = sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
  }

  float melEnergies[N_MELS];
  for (int m = 0; m < N_MELS; m++)
  {
    float energy = 0.0f;
    for (int k = 0; k < N_FFT / 2 + 1; k++)
    {
      energy += magnitude[k] * melFilterbank[m * (N_FFT / 2 + 1) + k];
    }
    melEnergies[m] = log(energy + 1e-10f);
  }

  for (int i = 0; i < N_MFCC; i++)
  {
    float sum = 0.0f;
    for (int j = 0; j < N_MELS; j++)
    {
      sum += melEnergies[j] * dctMatrix[i * N_MELS + j];
    }
    mfccs[i] = sum;
  }
}

// ============================================
// Procesar todo el audio a MFCCs
// ============================================
void processAudioToMFCC()
{
  Serial.println("\n🔄 Procesando audio a MFCCs...");

  unsigned long startMFCC = millis();

  float frameMFCCs[N_MFCC];

  for (int frame = 0; frame < N_FRAMES; frame++)
  {
    extractFrameMFCC(frame, frameMFCCs);

    for (int i = 0; i < N_MFCC; i++)
    {
      mfccOutput[i * N_FRAMES + frame] = (frameMFCCs[i] - MFCC_MEAN) / MFCC_STD;
    }

    // Actualizar UI cada 10 frames
    if (frame % 10 == 0)
    {
      showProcessingScreen(frame, N_FRAMES);
    }

    if (frame % 50 == 0)
    {
      Serial.printf("   Frame %d/%d (%.1f%%)\n", frame, N_FRAMES, frame * 100.0f / N_FRAMES);
    }
  }

  timeMFCC = millis() - startMFCC;
  Serial.printf("✅ MFCCs extraídos en %lu ms\n", timeMFCC);
}

// ============================================
// Mostrar métricas en Serial
// ============================================
void printMetrics()
{
  Serial.println("\n📊 ═══════════════════════════════════════");
  Serial.println("   MÉTRICAS DE PROCESAMIENTO");
  Serial.println("   ═══════════════════════════════════════");
  Serial.printf("   🎤 Captura de audio:      %5lu ms\n", timeCapture);
  Serial.printf("   🔊 Extracción MFCCs:      %5lu ms\n", timeMFCC);
  Serial.println("   ───────────────────────────────────────");
  Serial.printf("   ⏱️  TOTAL:                 %5lu ms\n", timeTotal);
  Serial.printf("   📈 Desglose porcentual:\n");
  Serial.printf("   Captura:   %.1f%%\n", (timeCapture * 100.0f) / timeTotal);
  Serial.printf("   MFCCs:     %.1f%%\n", (timeMFCC * 100.0f) / timeTotal);
  Serial.printf("   💾 PSRAM libre:     %d KB\n",
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
  Serial.printf("   📐 Primeros 5 MFCCs del frame 0:\n");
  for (int i = 0; i < 5; i++)
  {
    Serial.printf("      [%d][0] = %.4f\n", i, mfccOutput[i * N_FRAMES]);
  }
  Serial.println("   ═══════════════════════════════════════\n");
}

// ============================================
// Setup
// ============================================
void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("\n🎙️  MoodLink - Test MFCC + UI");
  Serial.println("═══════════════════════════════════════");
  Serial.print("📌 VERSION: ");
  Serial.println(VERSION);
  Serial.println("═══════════════════════════════════════");

  if (!LittleFS.begin(true))
  {
    Serial.println("❌ No se pudo montar LittleFS");
    while (1)
      ;
  }

  // Inicializar LED
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, 1);
  FastLED.setBrightness(50);
  leds[0] = CRGB::Blue;
  FastLED.show();

  // Inicializar pantalla
  Serial.println("🖥️  Inicializando pantalla...");
  Serial.printf("   LCD_DC: %d, LCD_CS: %d, LCD_SCLK: %d, LCD_MOSI: %d\n",
                LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI);
  Serial.printf("   LCD_RST: %d, LCD_BL: %d\n", LCD_RST, LCD_BL);

  // Configurar backlight con PWM al 50% de brillo
  // NOTA: El PWM está invertido (0=máximo, 255=apagado)
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  ledcAttachPin(LCD_BL, 1);
  ledcSetup(1, 2000, 8);
  ledcWrite(1, 127); // Invertido: 127 = 50% brillo (255-128=127)
  Serial.println("   ✓ Backlight configurado con PWM (50%)");

  // Usar Arduino_ESP32SPIDMA (con DMA) igual que ejemplos oficiales
  bus = new Arduino_ESP32SPIDMA(
      LCD_DC,
      LCD_CS,
      LCD_SCLK,
      LCD_MOSI,
      GFX_NOT_DEFINED);
  Serial.println("   ✓ Bus SPI DMA creado");

  // Configurar GC9D01N igual que ejemplos oficiales
  gfx = new Arduino_GC9D01N(
      bus,
      LCD_RST,
      0,     // rotation
      false, // IPS (false como en ejemplos oficiales)
      LCD_WIDTH,
      LCD_HEIGHT,
      0, 0, 0, 0); // offsets
  Serial.println("   ✓ Driver GC9D01N creado");

  gfx->begin();
  Serial.println("   ✓ Display inicializado");

  gfx->fillScreen(WHITE);
  Serial.println("   ✓ Pantalla rellenada con BLANCO");
  delay(200);

  gfx->fillScreen(RED);
  Serial.println("   ✓ Pantalla rellenada con ROJO");
  delay(200);

  gfx->fillScreen(GREEN);
  Serial.println("   ✓ Pantalla rellenada con VERDE");
  delay(200);

  gfx->fillScreen(BLUE);
  Serial.println("   ✓ Pantalla rellenada con AZUL");
  delay(200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("✅ Pantalla lista");

  // Reservar memoria en PSRAM
  Serial.println("\n💾 Reservando memoria PSRAM...");

  audioBuffer = (int16_t *)ps_malloc(N_SAMPLES * sizeof(int16_t));
  mfccOutput = (float *)ps_malloc(N_MFCC * N_FRAMES * sizeof(float));
  vReal = (float *)ps_malloc(N_FFT * sizeof(float));
  vImag = (float *)ps_malloc(N_FFT * sizeof(float));
  melFilterbank = (float *)ps_malloc(N_MELS * (N_FFT / 2 + 1) * sizeof(float));
  dctMatrix = (float *)ps_malloc(N_MFCC * N_MELS * sizeof(float));
  hammingWindow = (float *)ps_malloc(N_FFT * sizeof(float));

  if (!audioBuffer || !mfccOutput || !vReal || !vImag ||
      !melFilterbank || !dctMatrix || !hammingWindow)
  {
    Serial.println("❌ Error: No se pudo reservar PSRAM");
    while (1)
      ;
  }

  Serial.println("✅ PSRAM reservada");

  // Inicializar micrófono
  Serial.println("\n🎤 Inicializando micrófono I2S...");
  while (!Microphone->begin(I2S_MODE_MASTER, AD_IIS_DATA_IN,
                            I2S_CHANNEL_FMT_ONLY_LEFT, 16, SAMPLE_RATE))
  {
    Serial.println("❌ Micrófono no inicializado, reintentando...");
    delay(2000);
  }
  Serial.println("✅ Micrófono listo");

  // Inicializar matrices de preprocesamiento
  Serial.println("\n🔧 Inicializando matrices MFCC...");
  initHammingWindow();
  initMelFilterbank();
  initDCTMatrix();

  leds[0] = CRGB::Green;
  FastLED.show();

  Serial.println("\n✅ Sistema listo");
  Serial.println("═══════════════════════════════════════\n");

  // Mostrar pantalla de inicio
  showStartScreen();
}

// ============================================
// Loop
// ============================================
void loop()
{
  if (digitalRead(BUTTON_PIN) == LOW && !recording)
  {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW)
    {
      recording = true;
      writeIndex = 0;

      Serial.println("\n🔴 GRABANDO...");
      leds[0] = CRGB::Red;
      FastLED.show();

      unsigned long startTotal = millis();
      unsigned long startCapture = millis();
      unsigned long captureStart = millis();

      // Capturar audio con actualización de UI
      while (millis() - captureStart < DURATION_SEC * 1000)
      {
        int16_t sample;
        if (Microphone->IIS_Read_Data((char *)&sample, sizeof(int16_t)))
        {
          if (writeIndex < N_SAMPLES)
          {
            audioBuffer[writeIndex++] = sample;
          }
        }

        // Actualizar UI cada 100ms
        static unsigned long lastUIUpdate = 0;
        if (millis() - lastUIUpdate > 100)
        {
          float progress = (float)(millis() - captureStart) / (DURATION_SEC * 1000);
          showRecordingScreen(progress);
          lastUIUpdate = millis();
        }
      }

      timeCapture = millis() - startCapture;
      Serial.printf("✅ Captura completada: %d samples en %lu ms\n", writeIndex, timeCapture);

      leds[0] = CRGB::Yellow;
      FastLED.show();

      // Procesar a MFCCs
      processAudioToMFCC();

      timeTotal = millis() - startTotal;

      // Mostrar resultados
      showResultsScreen();
      printMetrics();

      recording = false;
      leds[0] = CRGB::Green;
      FastLED.show();

      Serial.println("✅ Listo para otra grabación");
      Serial.println("Presiona BTN para volver al inicio...");

      // Esperar 5 segundos antes de volver
      delay(5000);
      showStartScreen();
    }
  }
}
