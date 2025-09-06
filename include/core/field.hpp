#pragma once
#include "app_config.hpp"
#include <cstdint>
#include <string>

class NebulaField {
public:
  explicit NebulaField(const AppConfig& cfg);

  // Genera el píxel ARGB8888 para (x,y) en tiempo t (segundos)
  uint32_t sample_pixel(int x, int y, float t) const;

private:
  AppConfig cfg_;
    // Colores extremos de la paleta aleatoria (derivados de --seed)
  uint8_t baseR1, baseG1, baseB1;
  uint8_t baseR2, baseG2, baseB2;


  // Helpers
  static inline float smooth(float x);
  static inline float lerp(float a, float b, float t);
  static inline uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

  // Ruido / fBm
  uint32_t hash_u32(uint32_t x) const;
  float noise3(float x, float y, float z) const;               // [0,1]
  float fbm(float x, float y, float z, int octaves) const;     // ~[-1,1]
  float rfbm(float x, float y, float z, int octaves) const;    // [0,1] filamentos (ridged)

  // Paletas
  void palette_nebula(float v, uint8_t& r, uint8_t& g, uint8_t& b) const;
  void palette_inferno(float v, uint8_t& r, uint8_t& g, uint8_t& b) const;
  void palette_ice(float v, uint8_t& r, uint8_t& g, uint8_t& b) const;
  void palette_bw(float v, uint8_t& r, uint8_t& g, uint8_t& b) const;
};