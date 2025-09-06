#pragma once
#include "app_config.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Screensaver {
public:
  explicit Screensaver(const AppConfig& cfg);
  ~Screensaver();
  int run_seq(); // loop secuencial
  int run_omp(); // loop paralelo (usado por main_omp)

private:
  AppConfig cfg_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  bool init();
  void shutdown();
};