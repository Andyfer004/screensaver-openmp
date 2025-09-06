#pragma once
#include <string>

struct AppConfig {
  // Parámetros de ventana / escena
  int   width  = 640;
  int   height = 480;
  int   n      = 8;           // octavas
  float lacunarity  = 2.0f;
  float persistence = 0.5f;
  float zspeed      = 0.15f;
  unsigned int seed = 0;
  bool  vsync   = false;

  // UI / título / paleta
  bool  show_fps = true;      // mostrar FPS en pantalla/console
  std::string palette = "nebula";
  std::string window_title = "Nebulae — OpenMP Screensaver (UVG)";

  // OpenMP
  std::string omp_schedule = "static"; // static|dynamic|guided|auto
  int   omp_chunk = 32;                // tamaño de bloque / tile

  // Render a baja resolución + upscale (para subir FPS)
  float render_scale = 1.0f;           // 0.3..1.0

  // Normaliza / corrige argumentos
  void clamp_to_valid_ranges();
};
