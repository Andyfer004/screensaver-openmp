#include "core/cli.hpp"
#include "core/screensaver.hpp"
#include <iostream>

int main(int argc, char** argv) {
  AppConfig cfg = parse_cli(argc, argv);
  std::cout << "[OMP] " << cfg.window_title << "\n"
            << "  size=" << cfg.width << "x" << cfg.height
            << "  N(octaves)=" << cfg.n
            << "  palette=" << cfg.palette
            << "  zspeed=" << cfg.zspeed << std::endl;
  Screensaver app(cfg);
  return app.run_omp();
}