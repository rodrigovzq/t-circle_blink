# MoodLink - Plan de Pruebas 4 a 7

## Estado Actual
El sistema ya reconoce emociones a partir de audio usando un modelo de inteligencia artificial embebido. La versión actual (v3.0) logró reducir el tiempo de procesamiento de 6 minutos a 5.6 segundos.

## Objetivo
Desarrollar el producto final: un dispositivo que capture voz, analice la emoción y muestre el resultado de forma continua.

---

## Plan de Pruebas

### Prueba 4 - Análisis de Recursos
Medir cuánta memoria utiliza cada componente del sistema para asegurar que todo cabe en el dispositivo.

### Prueba 5 - Integración del Pipeline
Conectar todas las partes: captura de audio → procesamiento → reconocimiento de emoción. Verificar que funcionen juntas correctamente.

### Prueba 6 - Optimización
Reducir el uso de memoria y tiempo de procesamiento donde sea posible.

### Prueba 7 - Producto Final
Sistema completo funcionando en ciclo continuo con indicadores visuales del estado y la emoción detectada.

---

## Métricas Objetivo

| Métrica | Actual | Objetivo |
|---------|--------|----------|
| Tiempo de análisis | 5.6s | < 8s |
| Uso de memoria | ~1.6 MB | < 1.5 MB |
| Ciclo completo | ~10s | < 12s |

---

## Entregable Final
Dispositivo que:
1. Graba 4 segundos de voz
2. Procesa y clasifica la emoción (7 categorías)
3. Muestra el resultado visualmente
4. Repite el ciclo de forma automática
