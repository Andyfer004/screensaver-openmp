#include "core/field.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

// -------------------- Helpers de color (HSL) --------------------
static void rgb_to_hsl(uint8_t R, uint8_t G, uint8_t B, float& h, float& s, float& l) {
  float r = R / 255.f, g = G / 255.f, b = B / 255.f;
  float mx = std::max(r, std::max(g, b));
  float mn = std::min(r, std::min(g, b));
  l = (mx + mn) * 0.5f;
  if (mx == mn) { h = s = 0.f; return; }
  float d = mx - mn;
  s = (l > 0.5f) ? d / (2.f - mx - mn) : d / (mx + mn);
  if (mx == r)      h = (g - b) / d + (g < b ? 6.f : 0.f);
  else if (mx == g) h = (b - r) / d + 2.f;
  else              h = (r - g) / d + 4.f;
  h *= 60.f; // grados
}
static float hlerp(float p, float q, float t) {
  if (t < 0.f) t += 1.f; if (t > 1.f) t -= 1.f;
  if (t < 1.f/6.f) return p + (q - p) * 6.f * t;
  if (t < 1.f/2.f) return q;
  if (t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
  return p;
}
static void hsl_to_rgb(float h, float s, float l, uint8_t& R, uint8_t& G, uint8_t& B) {
  h = std::fmod(h < 0 ? h + 360.f : h, 360.f);
  float H = h / 360.f;
  float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
  float p = 2.f * l - q;
  float r = hlerp(p, q, H + 1.f/3.f);
  float g = hlerp(p, q, H);
  float b = hlerp(p, q, H - 1.f/3.f);
  R = uint8_t(std::clamp(r, 0.f, 1.f) * 255.f);
  G = uint8_t(std::clamp(g, 0.f, 1.f) * 255.f);
  B = uint8_t(std::clamp(b, 0.f, 1.f) * 255.f);
}

// -------------------- NebulaField --------------------
NebulaField::NebulaField(const AppConfig& cfg) : cfg_(cfg) {
  // Genera dos colores extremos pseudoaleatorios a partir de --seed
  // (usamos hash_u32 para derivar bytes; si quieres evitar tonos muy oscuros,
  // levantamos un poco los mínimos con un offset)
  uint64_t now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  uint32_t h1 = hash_u32(uint32_t(now ^ (cfg_.seed * 1234u)));
  uint32_t h2 = hash_u32(uint32_t((now >> 32) ^ (cfg_.seed * 5678u)));


  auto lift = [](uint8_t c){ return uint8_t(50 + (c % 180)); }; 

  baseR1 = lift( uint8_t( (h1      ) & 0xFF ) );
  baseG1 = lift( uint8_t( (h1 >>  8) & 0xFF ) );
  baseB1 = lift( uint8_t( (h1 >> 16) & 0xFF ) );

  baseR2 = lift( uint8_t( (h2      ) & 0xFF ) );
  baseG2 = lift( uint8_t( (h2 >>  8) & 0xFF ) );
  baseB2 = lift( uint8_t( (h2 >> 16) & 0xFF ) );
}

inline float NebulaField::smooth(float x) {
  // smootherstep: 6x^5 - 15x^4 + 10x^3
  return x*x*x*(x*(x*6.f - 15.f) + 10.f);
}
inline float NebulaField::lerp(float a, float b, float t) { return a + (b - a) * t; }

uint32_t NebulaField::pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  // ARGB8888 (A en los bits altos)
  return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// Hash barato (suficiente para value noise)
uint32_t NebulaField::hash_u32(uint32_t x) const {
  x ^= cfg_.seed;
  x *= 0x27d4eb2d;
  x ^= x >> 15;
  x *= 0x85ebca6b;
  x ^= x >> 13;
  return x;
}

// Value noise 3D + trilineal
float NebulaField::noise3(float x, float y, float z) const {
  int xi = int(std::floor(x)), yi = int(std::floor(y)), zi = int(std::floor(z));
  float xf = x - xi, yf = y - yi, zf = z - zi;
  float u = smooth(xf), v = smooth(yf), w = smooth(zf);

  auto cell = [&](int dx, int dy, int dz) {
    uint32_t h = hash_u32(
      uint32_t(xi + dx) * 73856093u ^
      uint32_t(yi + dy) * 19349663u ^
      uint32_t(zi + dz) * 83492791u
    );
    return (h & 0xFFFF) / 65535.f; // [0,1]
  };

  float c000 = cell(0,0,0), c100 = cell(1,0,0), c010 = cell(0,1,0), c110 = cell(1,1,0);
  float c001 = cell(0,0,1), c101 = cell(1,0,1), c011 = cell(0,1,1), c111 = cell(1,1,1);

  float x00 = lerp(c000, c100, u);
  float x10 = lerp(c010, c110, u);
  float x01 = lerp(c001, c101, u);
  float x11 = lerp(c011, c111, u);

  float y0 = lerp(x00, x10, v);
  float y1 = lerp(x01, x11, v);

  return lerp(y0, y1, w); // [0,1]
}

// fBm clásico (suma de ruidos a distintas escalas)
float NebulaField::fbm(float x, float y, float z, int octaves) const {
  float amp = 1.f, freq = 1.f;
  float sum = 0.f, norm = 0.f;
  for (int i = 0; i < octaves; ++i) {
    float n = noise3(x*freq, y*freq, z*freq);  // [0,1]
    sum += (n*2.f - 1.f) * amp;                // [-1,1] * amp
    norm += amp;
    amp *= cfg_.persistence;
    freq *= cfg_.lacunarity;
  }
  return sum / std::max(norm, 1e-6f); // ~[-1,1]
}

// Ridged fBm (resalta crestas/filamentos)
float NebulaField::rfbm(float x, float y, float z, int octaves) const {
  float amp = 1.f, freq = 1.f;
  float sum = 0.f, norm = 0.f;
  for (int i = 0; i < octaves; ++i) {
    float n = noise3(x*freq, y*freq, z*freq);    // [0,1]
    float ridge = 1.f - std::fabs(n*2.f - 1.f);  // [0,1]
    ridge = ridge * ridge;                       // afilar
    sum += ridge * amp;
    norm += amp;
    amp *= cfg_.persistence;
    freq *= cfg_.lacunarity;
  }
  return sum / std::max(norm, 1e-6f); // [0,1]
}

// Paletas clásicas (ya no se usan, pero las dejamos por si las quieres activar)
void NebulaField::palette_nebula(float v, uint8_t& r, uint8_t& g, uint8_t& b) const {
  float t = std::clamp(v, 0.f, 1.f);
  if (t < 0.25f) { float k = t / 0.25f; r=uint8_t(0*(1-k)+10*k); g=uint8_t(0*(1-k)+25*k); b=uint8_t(5*(1-k)+140*k); }
  else if (t < 0.5f) { float k=(t-0.25f)/0.25f; r=uint8_t(10*(1-k)+100*k); g=uint8_t(25*(1-k)+20*k); b=uint8_t(140*(1-k)+200*k); }
  else if (t < 0.75f){ float k=(t-0.5f)/0.25f; r=uint8_t(100*(1-k)+210*k); g=uint8_t(20*(1-k)+30*k); b=uint8_t(200*(1-k)+120*k); }
  else               { float k=(t-0.75f)/0.25f; r=uint8_t(210*(1-k)+255*k); g=uint8_t(30*(1-k)+185*k); b=uint8_t(120*(1-k)+80*k); }
}
void NebulaField::palette_inferno(float v, uint8_t& r, uint8_t& g, uint8_t& b) const {
  float t = std::clamp(v, 0.f, 1.f);
  r = uint8_t(20 + 235 * t);
  g = uint8_t(10 + 120 * std::pow(t, 1.2f));
  b = uint8_t(5  +  30 * std::pow(1.f - t, 2.f));
}
void NebulaField::palette_ice(float v, uint8_t& r, uint8_t& g, uint8_t& b) const {
  float t = std::clamp(v, 0.f, 1.f);
  r = uint8_t(20 + 40 * (1.f - t));
  g = uint8_t(80 + 140 * t);
  b = uint8_t(140 + 115 * t);
}
void NebulaField::palette_bw(float v, uint8_t& r, uint8_t& g, uint8_t& b) const {
  uint8_t k = uint8_t(std::clamp(v, 0.f, 1.f) * 255.f);
  r = g = b = k;
}

// Pixel final (warp + swirl + filamentos + estrellas + viñeta + paleta aleatoria)
uint32_t NebulaField::sample_pixel(int x, int y, float t) const {
  // Coordenadas normalizadas y centradas
  float uN = float(x) / float(std::max(1, cfg_.width));
  float vN = float(y) / float(std::max(1, cfg_.height));
  float sx = (uN - 0.5f) * 1.9f;
  float sy = (vN - 0.5f) * 1.9f;
  float z  = t * cfg_.zspeed;

  // Domain warp (turbulencia)
  float w1x = (noise3(sx*0.9f + 2.1f, sy*0.9f,       z*0.6f) * 2.f - 1.f);
  float w1y = (noise3(sx*0.9f,       sy*0.9f + 3.7f, z*0.6f) * 2.f - 1.f);
  float w2x = (noise3(sx*1.7f + 5.3f, sy*1.7f,       z*1.1f) * 2.f - 1.f);
  float w2y = (noise3(sx*1.7f,       sy*1.7f + 4.2f, z*1.1f) * 2.f - 1.f);
  float warp1 = 0.42f, warp2 = 0.18f;
  float wx = sx + w1x * warp1 + w2x * warp2;
  float wy = sy + w1y * warp1 + w2y * warp2;

  // Swirl (remolino) dependiente del radio + leve spin temporal
  float r2 = wx*wx + wy*wy;
  float ang = 0.65f * (1.f - std::exp(-r2 * 0.9f)) + 0.18f * t;
  float cs = std::cos(ang), sn = std::sin(ang);
  float rx =  cs * wx - sn * wy;
  float ry =  sn * wx + cs * wy;

  // Composición: base fBm + filamentos ridged
  int   oct   = std::max(1, cfg_.n);
  float base  = fbm (rx, ry, z, oct);              // ~[-1,1]
  float rid   = rfbm(rx*1.8f, ry*1.8f, z, oct);    // [0,1]
  float v0    = std::clamp((base + 1.f) * 0.5f, 0.f, 1.f);
  float shade = 0.55f * v0 + 0.45f * std::pow(rid, 1.5f);

  // Contraste + core + gamma
  shade = shade * 1.28f - 0.14f;
  shade = std::clamp(shade, 0.f, 1.f);
  float core = 0.28f * std::exp(-r2 * 1.1f);
  shade = std::clamp(shade + core, 0.f, 1.f);
  shade = std::pow(shade, 1.4f);

  // -------- Paleta ALEATORIA por seed (sin paletas predefinidas) --------
  uint8_t r = uint8_t(baseR1 * (1.0f - shade) + baseR2 * shade);
  uint8_t g = uint8_t(baseG1 * (1.0f - shade) + baseG2 * shade);
  uint8_t b = uint8_t(baseB1 * (1.0f - shade) + baseB2 * shade);

  // Aumentar saturación para que no se vean grises
  float h,s,l;
  rgb_to_hsl(r,g,b,h,s,l);
  s = std::min(1.f, s * 1.4f);  // boost
  hsl_to_rgb(h,s,l,r,g,b);
  // Rotación de tono (hue) dependiente de seed + animación suave en el tiempo
  {
    float h, s, l;
    rgb_to_hsl(r, g, b, h, s, l);
    float base_h = float(cfg_.seed % 360);       // depende de --seed
    float anim_h = 35.f * std::sin(t * 0.17f);   // oscilación suave
    h = h + base_h * 0.25f + anim_h;
    hsl_to_rgb(h, s, l, r, g, b);
  }

  // Estrellas con parpadeo (pocas, mezcla aditiva)
  uint32_t hh = hash_u32(uint32_t(x) * 2654435761u ^ uint32_t(y) * 1013904223u);
  float rnd = (hh & 0xFFFFFF) / float(0xFFFFFF);
  if (rnd > 0.9980f) { // ~0.2%
    float tw = 0.5f + 0.5f * std::sin(t * (4.0f + (hh % 997) * 0.012f));
    uint8_t star = uint8_t(210 + 45 * tw);
    r = std::min<int>(255, int(r) + star);
    g = std::min<int>(255, int(g) + star);
    b = std::min<int>(255, int(b) + star);
  }

  return pack_rgba(r, g, b, 255);
}