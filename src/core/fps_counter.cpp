#include "core/fps_counter.hpp"
using clk = std::chrono::steady_clock; // alias para el reloj estable de alta precisión

// -----------------------------------------------------
// FPSCounter::tick
// Descripción:
//   - Incrementa el número de frames renderizados.
//   - Cada 500 ms calcula la media de FPS.
//   - Reinicia el contador de frames para el siguiente período.
// Notas:
//   - Se usa un promedio en 0.5s porque da un balance entre
//     respuesta rápida y estabilidad en el valor mostrado.
// -----------------------------------------------------
void FPSCounter::tick() {
  frames_++; // contar un frame más

  auto now = clk::now(); // obtener tiempo actual
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count();

  // Si ha pasado al menos medio segundo, recalculamos FPS
  if (ms >= 500) {
    fps_ = frames_ * 1000.0 / ms; // frames renderizados por segundo
    frames_ = 0;                  // reiniciar contador
    last_ = now;                  // actualizar última medición
  }
}