#pragma once
#include <chrono>

class FPSCounter {
public:
  void tick();
  double fps() const { return fps_; }
private:
  int frames_ = 0;
  double fps_ = 0.0;
  std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
};