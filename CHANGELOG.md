# Changelog - MoodLink MFCC Test

## 2025-11-16 - Resolución de problemas de Display y refinamientos de UI

### Resumen
Sesión de debugging y mejoras para hacer funcionar el display GC9D01N en el T-Circle S3 V1.0 y refinar la interfaz de usuario.

---

### Historial de Versiones

| Versión | Intención | Resultado | Cambios Realizados |
|---------|-----------|-----------|-------------------|
| **Inicial** | Código funcionaba bien en serial pero display no mostraba nada | ❌ Display negro/sin imagen | - Código compilaba correctamente<br>- Puerto serie funcionaba<br>- Display no mostraba contenido |
| **v1.0-test1** | Activar backlight y agregar debugging detallado | ❌ Display seguía sin mostrar nada | - Agregado `pinMode(LCD_BL, OUTPUT)` y `digitalWrite(LCD_BL, HIGH)`<br>- Agregado sistema de versionado<br>- Agregado debug detallado de pines LCD<br>- Agregada prueba visual (rectángulo rojo)<br>- Corregida versión en `pin_config.h` (V1.0 en lugar de V1.1) |
| **v1.0-test2** | Usar configuración exacta de ejemplos oficiales que funcionan | ✅ Display funcionando correctamente | - Cambiado `Arduino_ESP32SPI` → `Arduino_ESP32SPIDMA` (con DMA)<br>- Cambiado parámetro IPS: `true` → `false`<br>- Agregados parámetros width, height y offsets al constructor GC9D01N<br>- Implementado control PWM del backlight (`ledcAttachPin`, `ledcSetup`, `ledcWrite`)<br>- Agregada secuencia de prueba visual (BLANCO→ROJO→VERDE→AZUL) |
| **v1.0-test3** | Centrar elementos de UI para pantalla circular | ✅ UI centrada correctamente | - Centrados todos los títulos de pantallas<br>- Ajustadas posiciones X de todos los elementos<br>- "MoodLink": 30→32<br>- "GRABANDO": 15→32<br>- "Procesando": 10→20<br>- "Completo": 25→32<br>- Métricas y otros elementos ajustados |
| **v1.0-test4** | Reducir brillo del display al 50% | ✅ Brillo reducido correctamente | - Descubierto que PWM está invertido (0=max, 255=off)<br>- Cambiado `ledcWrite(1, 0)` → `ledcWrite(1, 127)` para 50% brillo<br>- Agregado comentario explicativo sobre PWM invertido<br>- Usuario ajustó delays de secuencia de colores (500ms→200ms) |

---

### Problemas Resueltos

#### 1. Display no mostraba contenido
**Causa raíz:** Configuración incorrecta del bus SPI y parámetros del driver GC9D01N

**Solución:**
- Usar `Arduino_ESP32SPIDMA` en lugar de `Arduino_ESP32SPI`
- Configurar parámetro IPS como `false` (no `true`)
- Agregar parámetros completos al constructor: width, height, offsets
- Implementar control PWM del backlight correctamente

#### 2. Textos cortados en pantalla circular
**Causa raíz:** Elementos alineados a la izquierda se salían del área visible

**Solución:**
- Centrar todos los títulos y elementos de texto
- Ajustar coordenadas X de todos los elementos de UI

#### 3. Brillo del display muy alto
**Causa raíz:** PWM del backlight invertido (0=máximo brillo)

**Solución:**
- Documentado que el hardware usa lógica invertida
- Fórmula: `valor_PWM = 255 - (brillo_deseado * 255 / 100)`
- Configurado a 127 para 50% de brillo

---

### Configuración Actual del Display (v1.0-test4)

```cpp
// Bus SPI con DMA
Arduino_DataBus *bus = new Arduino_ESP32SPIDMA(
    LCD_DC,      // 16
    LCD_CS,      // 13
    LCD_SCLK,    // 15
    LCD_MOSI,    // 17
    GFX_NOT_DEFINED);

// Driver GC9D01N
Arduino_GFX *gfx = new Arduino_GC9D01N(
    bus,
    LCD_RST,     // -1 (no usado)
    0,           // rotation
    false,       // IPS
    LCD_WIDTH,   // 160
    LCD_HEIGHT,  // 160
    0, 0, 0, 0); // offsets

// Backlight PWM (invertido)
ledcAttachPin(LCD_BL, 1);  // Pin 18
ledcSetup(1, 2000, 8);
ledcWrite(1, 127);  // 50% brillo
```

---

### Hardware Verificado

- **Modelo:** T-Circle S3 V1.0
- **Display:** GC9D01N 160x160 circular
- **Micrófono:** MSM261 (3 pines: BCLK=7, WS=9, DATA=8)
- **Backlight:** Control PWM invertido en pin 18

---

### Notas Importantes

1. **PWM del Backlight Invertido:**
   - `ledcWrite(1, 0)` = 100% brillo
   - `ledcWrite(1, 127)` = 50% brillo
   - `ledcWrite(1, 255)` = 0% brillo (apagado)

2. **Configuración de Versión:**
   - `pin_config.h` debe tener `T_Circle_S3_V1_0` definido
   - `platformio.ini` también define `-DT_Circle_S3_V1_0`
   - Warnings de redefinición son normales y no afectan

3. **Display Circular:**
   - Siempre centrar elementos importantes
   - Los bordes pueden cortarse en pantalla circular

---

### Próximos Pasos Sugeridos

- [ ] Probar funcionalidad completa de grabación de audio
- [ ] Verificar extracción de MFCCs
- [ ] Optimizar actualización de UI durante procesamiento
- [ ] Considerar agregar función para ajustar brillo dinámicamente si es necesario

---

### Referencias

- Ejemplos oficiales consultados:
  - `extras/T-Circle-S3/examples/GFX/GFX.ino`
  - `extras/T-Circle-S3/examples/GFX_CST816D_Image/GFX_CST816D_Image.ino`
- Tests anteriores funcionales:
  - `test/mic test.old`
  - `test/mic+pc transfer.old`
