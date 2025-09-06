#include "core/app_config.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>

static inline int   clampi(int v, int lo, int hi){ return std::max(lo, std::min(hi, v)); }
static inline float clampf(float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); }

/**
 * @brief Clamps and normalizes AppConfig parameters to valid ranges and values.
 *
 * Ensures that all configuration parameters are within reasonable and supported limits:
 * - Clamps window dimensions (`width`, `height`) to minimum values.
 * - Clamps noise parameters (`n`, `lacunarity`, `persistence`, `zspeed`) to valid ranges.
 * - Clamps rendering scale (`render_scale`) to supported range.
 * - Normalizes and validates OpenMP schedule (`omp_schedule`), falling back to "static" if invalid.
 * - Clamps OpenMP chunk size (`omp_chunk`) to a reasonable range.
 * - Normalizes and validates color palette (`palette`), falling back to "nebula" if invalid.
 *
 * Emits warnings to stderr if invalid values are detected and corrected.
 */
void AppConfig::clamp_to_valid_ranges() {
  // límites razonables
  width  = std::max(160, width);
  height = std::max(120, height);

  // parámetros de ruido
  n           = clampi(n, 1, 12);
  lacunarity  = clampf(lacunarity, 1.5f, 3.0f);
  persistence = clampf(persistence, 0.05f, 0.95f);
  zspeed      = clampf(zspeed, 0.0f, 5.0f);

  // render low-res
  render_scale = clampf(render_scale, 0.3f, 1.0f);

  // normaliza schedule a minúsculas y valida
  for (char &ch : omp_schedule)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (omp_schedule!="static" && omp_schedule!="dynamic" &&
      omp_schedule!="guided" && omp_schedule!="auto") {
    std::fprintf(stderr, "[warn] invalid --schedule '%s' -> using 'static'\n",
                 omp_schedule.c_str());
    omp_schedule = "static";
  }

  // chunk razonable
  omp_chunk = clampi(omp_chunk, 1, 512);

  // paletas permitidas
  for (char &ch : palette)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (palette!="nebula" && palette!="inferno" && palette!="ice" && palette!="bw") {
    std::fprintf(stderr, "[warn] Invalid --palette '%s' -> using 'nebula'\n",
                 palette.c_str());
    palette = "nebula";
  }
}
