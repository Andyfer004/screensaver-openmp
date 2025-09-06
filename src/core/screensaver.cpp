#include "core/screensaver.hpp"
#include "core/field.hpp"
#include "core/fps_counter.hpp"

#include <SDL.h>
#include <cstdio>
#include <vector>
#include <chrono>
#include <cstring>    // memcpy
#include <algorithm>  // min/max
#include <cmath>

#if defined(_OPENMP)
  #include <omp.h>   // OpenMP (paralelismo en memoria compartida)
#endif

// =====================================================
//  Mini HUD: dibujar texto 5x7 y rectángulo semitransparente
//  (sin SDL_ttf; fuente bitmap mínima para mostrar FPS, etc.)
// =====================================================
namespace hud {

struct Glyph { uint8_t rows[7]; };
static const Glyph* get(char c);

// 5x7 (bitmask por fila). Cada byte codifica 5 bits útiles.
static const Glyph G0{{0x1E,0x11,0x13,0x15,0x19,0x11,0x1E}};
static const Glyph G1{{0x04,0x0C,0x14,0x04,0x04,0x04,0x1F}};
static const Glyph G2{{0x1E,0x01,0x01,0x1E,0x10,0x10,0x1F}};
static const Glyph G3{{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}};
static const Glyph G4{{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}};
static const Glyph G5{{0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}};
static const Glyph G6{{0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}};
static const Glyph G7{{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}};
static const Glyph G8{{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}};
static const Glyph G9{{0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}};
static const Glyph SP{{0x00,0x00,0x00,0x00,0x00,0x00,0x00}};
static const Glyph DOT{{0x00,0x00,0x00,0x00,0x00,0x06,0x06}};
static const Glyph GF{{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}}; // F
static const Glyph GP{{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}}; // P
static const Glyph GS{{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}; // S
static const Glyph GX{{0x11,0x0A,0x04,0x04,0x0A,0x11,0x11}}; // x

// Devuelve el glyph para el caracter solicitado.
inline const Glyph* get(char c){
  switch(c){
    case '0':return &G0; case '1':return &G1; case '2':return &G2; case '3':return &G3; case '4':return &G4;
    case '5':return &G5; case '6':return &G6; case '7':return &G7; case '8':return &G8; case '9':return &G9;
    case ' ':return &SP; case '.':return &DOT; case 'F':return &GF; case 'P':return &GP; case 'S':return &GS;
    case 'x':return &GX; default:return &SP;
  }
}

// Escritura segura de un pixel RGBA en el framebuffer (clamp bounds)
static inline void put(std::vector<uint32_t>& pix,int W,int H,int x,int y,uint32_t rgba){
  if((unsigned)x<(unsigned)W && (unsigned)y<(unsigned)H) pix[y*W+x]=rgba;
}

// Dibuja un carácter 5x7 a escala `s` con sombra
static void draw_char(std::vector<uint32_t>& pix,int W,int H,int x,int y,char c,uint32_t rgba,int s=3){
  const Glyph* g=get(c);
  for(int ry=0; ry<7; ++ry){
    uint8_t row=g->rows[ry];
    for(int rx=0; rx<5; ++rx){
      if(((row>>(4-rx))&1)==0) continue;
      for(int dy=0; dy<s; ++dy)
        for(int dx=0; dx<s; ++dx)
          put(pix,W,H,x+rx*s+dx,y+ry*s+dy,rgba);
    }
  }
}

// Dibuja una cadena con sombra sutil (1px) para mejorar legibilidad
static void draw_text(std::vector<uint32_t>& pix,int W,int H,int x,int y,const char* s,uint32_t rgba,int scale=3){
  int cx=x;
  for(const char* p=s; *p; ++p){
    draw_char(pix,W,H,cx+1,y+1,*p,0x80000000u,scale); // sombra
    draw_char(pix,W,H,cx,  y,  *p,rgba,scale);
    cx+= (5*scale + 1*scale); // ancho de glyph + espacio
  }
}

// Composición alpha sobre (fg over bg)
static inline uint32_t blend_over(uint32_t bg, uint32_t fg){
  uint8_t a=(fg>>24)&0xFF; if(a==0) return bg; if(a==255) return fg;
  float af=a/255.f;
  uint8_t br=(bg>>16)&0xFF, bgc=(bg>>8)&0xFF, bb=bg&0xFF;
  uint8_t fr=(fg>>16)&0xFF, fgc=(fg>>8)&0xFF, fb=fg&0xFF;
  uint8_t r=(uint8_t)(fr*af + br*(1-af));
  uint8_t g=(uint8_t)(fgc*af + bgc*(1-af));
  uint8_t b=(uint8_t)(fb*af + bb*(1-af));
  return 0xFF000000u | (r<<16) | (g<<8) | b;
}

// Rellena un rectángulo con alpha mezclando sobre el framebuffer
static void fill_rect_blend(std::vector<uint32_t>& pix,int W,int H,int x,int y,int w,int h,uint32_t rgba){
  int x1=std::max(0,x), y1=std::max(0,y);
  int x2=std::min(W,x+w), y2=std::min(H,y+h);
  for(int yy=y1; yy<y2; ++yy){
    uint32_t* row=pix.data()+yy*W;
    for(int xx=x1; xx<x2; ++xx) row[xx]=blend_over(row[xx],rgba);
  }
}
} // namespace hud
// ------------------------------------------------------------------------


// =====================================================
//    Screensaver: ciclo de vida SDL
// =====================================================

Screensaver::Screensaver(const AppConfig& cfg): cfg_(cfg) {}
Screensaver::~Screensaver() { shutdown(); }

// Inicializa SDL, crea ventana, renderer y textura de destino.
// Maneja errores con mensajes claros y limpia recursos si falla algo.
bool Screensaver::init(){
  if(SDL_Init(SDL_INIT_VIDEO)!=0){
    std::fprintf(stderr,"[SDL] Init error: %s\n",SDL_GetError());
    return false;
  }
  Uint32 flags=SDL_WINDOW_SHOWN;
  window_=SDL_CreateWindow(cfg_.window_title.c_str(),
                           SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                           cfg_.width,cfg_.height,flags);
  if(!window_){
    std::fprintf(stderr,"[SDL] CreateWindow error: %s\n",SDL_GetError());
    SDL_Quit(); return false;
  }
  Uint32 rflags=SDL_RENDERER_ACCELERATED | (cfg_.vsync?SDL_RENDERER_PRESENTVSYNC:0);
  renderer_=SDL_CreateRenderer(window_,-1,rflags);
  if(!renderer_){
    std::fprintf(stderr,"[SDL] CreateRenderer error: %s\n",SDL_GetError());
    SDL_DestroyWindow(window_); SDL_Quit(); return false;
  }
  texture_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,
                             cfg_.width,cfg_.height);
  if(!texture_){
    std::fprintf(stderr,"[SDL] CreateTexture error: %s\n",SDL_GetError());
    SDL_DestroyRenderer(renderer_); SDL_DestroyWindow(window_); SDL_Quit(); return false;
  }
  // Calidad de escalado (0=nearest, 1=linear). Elegimos nearest por nitidez/HUD.
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
  return true;
}

// Libera recursos SDL en orden seguro.
void Screensaver::shutdown(){
  if(texture_){ SDL_DestroyTexture(texture_); texture_=nullptr; }
  if(renderer_){ SDL_DestroyRenderer(renderer_); renderer_=nullptr; }
  if(window_){ SDL_DestroyWindow(window_); window_=nullptr; }
  SDL_Quit();
}


// =====================================================
// render_loop: ejecuta el bucle de render (seq/omp)
// - Construye NebulaField
// - Dibuja la imagen en un framebuffer RAM (pixels)
// - Sube la textura a GPU y presenta
// - Modo full-res o low-res+upscale según render_scale
// En modo OpenMP se muestra sincronización explícita:
//   * omp for (tiles) + collapse(2)
//   * barrier + single (evita data races con SDL)
// =====================================================
static int render_loop(SDL_Renderer* renderer, SDL_Texture* texture, const AppConfig& cfg, bool use_omp){
  NebulaField field(cfg);
  FPSCounter fps;
  using clk=std::chrono::steady_clock;
  auto t0=clk::now();

  const int W=cfg.width, H=cfg.height;
  std::vector<uint32_t> pixels((size_t)W*H);
  SDL_Event ev{}; bool running=true;

#if defined(_OPENMP)
  // Configurar política de scheduling si se solicitó (runtime control)
  if(use_omp){
    omp_sched_t kind=omp_sched_static;
    if      (cfg.omp_schedule=="dynamic") kind=omp_sched_dynamic;
    else if (cfg.omp_schedule=="guided")  kind=omp_sched_guided;
    else if (cfg.omp_schedule=="auto")    kind=omp_sched_auto;
    omp_set_schedule(kind,cfg.omp_chunk);

    // Log de confirmación de scheduling/hilos
    omp_sched_t k2; int ch2; omp_get_schedule(&k2,&ch2);
    const char* kname=(k2==omp_sched_static)?"static":(k2==omp_sched_dynamic)?"dynamic":(k2==omp_sched_guided)?"guided":"auto";
    std::printf("[OMP] max_threads=%d schedule=%s chunk=%d\n", omp_get_max_threads(), kname, ch2);
    std::fflush(stdout);
  }
#endif

  // Escala de render (low-res interno para subir FPS)
  const float s = std::clamp(cfg.render_scale, 0.3f, 1.0f);

  // Tamaño de tile (usamos chunk como guía; clamp para cache-friendliness)
  int TS = (cfg.omp_chunk>0 ? cfg.omp_chunk : 32);
  TS = std::max(8, std::min(64, TS));

  while(running){
    // Entrada: salir con ESC o cerrar ventana
    while(SDL_PollEvent(&ev)){
      if(ev.type==SDL_QUIT) running=false;
      if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_ESCAPE) running=false;
    }
    float t = std::chrono::duration<float>(clk::now()-t0).count();

    if(s >= 0.999f){
      // ================= FULL-RES (tiling) =================
      // Procesa por tiles para mejorar localidad de cache y disminuir false sharing.
      const int ntx=(W+TS-1)/TS, nty=(H+TS-1)/TS;

#if defined(_OPENMP)
      if(use_omp){
        #pragma omp parallel
        {
          // Cada hilo toma tiles según la política omp_set_schedule(...)
          #pragma omp for collapse(2) schedule(runtime)
          for(int ty=0; ty<nty; ++ty){
            for(int tx=0; tx<ntx; ++tx){
              int y0=ty*TS, y1=std::min(H,y0+TS);
              int x0=tx*TS, x1=std::min(W,x0+TS);
              for(int y=y0; y<y1; ++y){
                uint32_t* row=pixels.data()+y*W;
                #pragma omp simd
                for(int x=x0; x<x1; ++x){
                  row[x]=field.sample_pixel(x,y,t);
                }
              }
            }
          }

          // ---- Sincronización explícita ----
          #pragma omp barrier     // espera a que todos terminen sus tiles
          #pragma omp single      // solo un hilo sube/Present (SDL no es thread-safe)
          { /* hook opcional de sync; Upload/Present se realiza más abajo */ }
        }
      } else
#endif
      {
        // Secuencial: barrido por filas
        for(int y=0; y<H; ++y){
          uint32_t* row=pixels.data()+y*W;
          for(int x=0; x<W; ++x) row[x]=field.sample_pixel(x,y,t);
        }
      }

    } else {
      // ============== LOW-RES + UPSCALE ===================
      // 1) Renderiza en un buffer reducido (SW x SH).
      // 2) Escala al framebuffer final (W x H).
      const int SW=std::max(1,(int)std::floor(W*s));
      const int SH=std::max(1,(int)std::floor(H*s));
      static std::vector<uint32_t> lowres; lowres.assign((size_t)SW*SH,0);

#if defined(_OPENMP)
      if(use_omp){
        #pragma omp parallel
        {
          // a) Calcular lowres por tiles
          const int ntx=(SW+TS-1)/TS, nty=(SH+TS-1)/TS;
          #pragma omp for collapse(2) schedule(runtime)
          for(int ty=0; ty<nty; ++ty){
            for(int tx=0; tx<ntx; ++tx){
              int y0=ty*TS, y1=std::min(SH,y0+TS);
              int x0=tx*TS, x1=std::min(SW,x0+TS);
              for(int sy=y0; sy<y1; ++sy){
                for(int sx=x0; sx<x1; ++sx){
                  // Muestreo centrado para evitar aliasing duro
                  int XX=std::min(W-1,(int)((sx+0.5f)/s));
                  int YY=std::min(H-1,(int)((sy+0.5f)/s));
                  lowres[sy*SW+sx]=field.sample_pixel(XX,YY,t);
                }
              }
            }
          }

          // b) Upscale filas completas (paralelo)
          #pragma omp for schedule(static)
          for(int y=0; y<H; ++y){
            int sy=std::min(SH-1,(int)(y*s));
            uint32_t* row=pixels.data()+y*W;
            const uint32_t* srow=lowres.data()+sy*SW;
            #pragma omp simd
            for(int x=0; x<W; ++x){
              int sx=std::min(SW-1,(int)(x*s));
              row[x]=srow[sx];
            }
          }
        }
      } else
#endif
      {
        // Secuencial: calcular lowres y luego escalar
        for(int sy=0; sy<SH; ++sy){
          int YY=std::min(H-1,(int)((sy+0.5f)/s));
          for(int sx=0; sx<SW; ++sx){
            int XX=std::min(W-1,(int)((sx+0.5f)/s));
            lowres[sy*SW+sx]=field.sample_pixel(XX,YY,t);
          }
        }
        for(int y=0; y<H; ++y){
          int sy=std::min(SH-1,(int)(y*s));
          uint32_t* row=pixels.data()+y*W;
          const uint32_t* srow=lowres.data()+sy*SW;
          for(int x=0; x<W; ++x){
            int sx=std::min(SW-1,(int)(x*s));
            row[x]=srow[sx];
          }
        }
      }
    }

    // ----- HUD: mostrar FPS, hilos, n y scale (sobre el framebuffer) -----
    fps.tick();
    if (cfg.show_fps){
      char hudtxt[96];
#if defined(_OPENMP)
      int th = use_omp ? omp_get_max_threads() : 1;
#else
      int th = 1;
#endif
      std::snprintf(hudtxt,sizeof(hudtxt),"FPS %.1f  x%d  n=%d  s=%.2f",
                    fps.fps(), th, cfg.n, s);

      // Tamaño del texto según resolución
      int scale_px = (W>=1600?4:(W>=1100?3:3));
      int text_w = (int)std::strlen(hudtxt) * (5*scale_px + 1*scale_px);
      int text_h = 7*scale_px;

      // Caja semitransparente + texto con sombra
      hud::fill_rect_blend(pixels,W,H,8,8,text_w+14,text_h+14,0x66000000u);
      hud::draw_text(pixels,W,H,15,15,hudtxt,0xFFFFFFFFu,scale_px);
    }

    // ----- Upload a textura + presentar en la ventana -----
    void* tex_pixels=nullptr; int pitch=0;
    SDL_LockTexture(texture,nullptr,&tex_pixels,&pitch);
    for(int y=0;y<H;++y){
      std::memcpy((uint8_t*)tex_pixels + y*pitch, pixels.data()+y*W, W*sizeof(uint32_t));
    }
    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer,texture,nullptr,nullptr);
    SDL_RenderPresent(renderer);
  }
  return 0;
}


// =====================================================
//   Entradas públicas: correr en secuencial o OpenMP
// =====================================================
int Screensaver::run_seq(){
  if(!init()) return 1;
  int rc = render_loop(renderer_,texture_,cfg_,false);
  shutdown(); return rc;
}
int Screensaver::run_omp(){
  if(!init()) return 1;
  int rc = render_loop(renderer_,texture_,cfg_,true);
  shutdown(); return rc;
}
