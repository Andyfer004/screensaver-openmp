#include "core/cli.hpp"
#include "core/app_config.hpp"
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>   // std::find
#include <cstring>     // std::strcmp
#include <cstdio>      // std::fprintf

// -----------------------------------------------------
// Función auxiliar: busca una opción en argv y devuelve
// el valor siguiente a la clave (ej: -w 1280).
// Devuelve nullptr si no se encuentra.
// -----------------------------------------------------
static const char* get_opt(char** begin, char** end, const std::string& key) {
  char** it = std::find(begin, end, key);
  if (it != end && ++it != end) return *it;
  return nullptr;
}

// -----------------------------------------------------
// Muestra ayuda de uso por consola, explicando todas
// las opciones disponibles y sus rangos válidos.
// Se invoca con `--help`.
// -----------------------------------------------------
static void print_help(const char* exe) {
  std::fprintf(stdout,
    "Usage: %s [options]\n"
    "  -w <int>              width (>=160)\n"
    "  -h <int>              height (>=120)\n"
    "  -n <int>              octaves (1..12)\n"
    "  --seed <u32>\n"
    "  --lacunarity <f>      1.5..3.0\n"
    "  --persistence <f>     0.05..0.95\n"
    "  --zspeed <f>          0..5\n"
    "  --palette <name>      nebula|inferno|ice|bw\n"
    "  --vsync <0|1>\n"
    "  --render-scale <f>    0.3..1.0 (low-res render + upscale)\n"
    "  --schedule <static|dynamic|guided|auto>\n"
    "  --chunk <int>         (1..512)\n"
    "  --title-fps <0|1>     (alias de show_fps)\n",
    exe);
}

// -----------------------------------------------------
// parse_cli
// Entrada:
//   argc, argv -> argumentos de la línea de comandos.
// Salida:
//   Objeto AppConfig con la configuración del programa.
// Descripción:
//   - Procesa cada opción soportada (width, height, n, etc).
//   - Aplica programación defensiva con clamp_to_valid_ranges().
//   - Permite ejecutar `--help` para mostrar ayuda y salir.
// -----------------------------------------------------
AppConfig parse_cli(int argc, char** argv) {
  AppConfig cfg;

  // Si el usuario pide ayuda (--help), mostrar y salir
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0) {
      print_help(argv[0]);
      std::exit(0);
    }
  }

  // Lectura de parámetros básicos
  auto* v = get_opt(argv, argv+argc, std::string("-w"));
  if (v) cfg.width = std::atoi(v);
  v = get_opt(argv, argv+argc, std::string("-h"));
  if (v) cfg.height = std::atoi(v);
  v = get_opt(argv, argv+argc, std::string("-n"));
  if (v) cfg.n = std::atoi(v);

  // Opciones adicionales
  v = get_opt(argv, argv+argc, std::string("--seed"));
  if (v) cfg.seed = static_cast<unsigned int>(std::strtoul(v, nullptr, 10));
  v = get_opt(argv, argv+argc, std::string("--lacunarity"));
  if (v) cfg.lacunarity = std::atof(v);
  v = get_opt(argv, argv+argc, std::string("--persistence"));
  if (v) cfg.persistence = std::atof(v);
  v = get_opt(argv, argv+argc, std::string("--zspeed"));
  if (v) cfg.zspeed = std::atof(v);
  v = get_opt(argv, argv+argc, std::string("--palette"));
  if (v) cfg.palette = v;
  v = get_opt(argv, argv+argc, std::string("--vsync"));
  if (v) cfg.vsync = (std::string(v)=="1"||std::string(v)=="true"||std::string(v)=="on");

  // Extras: renderizado en baja resolución + opciones OpenMP
  v = get_opt(argv, argv+argc, std::string("--render-scale"));
  if (v) cfg.render_scale = std::atof(v);
  v = get_opt(argv, argv+argc, std::string("--schedule"));
  if (v) cfg.omp_schedule = v;
  v = get_opt(argv, argv+argc, std::string("--chunk"));
  if (v) cfg.omp_chunk = std::atoi(v);
  v = get_opt(argv, argv+argc, std::string("--title-fps"));
  if (v) cfg.show_fps = (std::string(v)=="1"||std::string(v)=="true"||std::string(v)=="on");

  // Aplicar programación defensiva:
  // asegura que los parámetros estén dentro de rangos válidos
  cfg.clamp_to_valid_ranges();
  return cfg;
}