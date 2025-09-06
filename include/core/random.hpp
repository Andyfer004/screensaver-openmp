#pragma once
#include <random>

struct RNG {
  explicit RNG(unsigned int seed): gen(seed) {}
  float uniform(float a, float b) {
    std::uniform_real_distribution<float> d(a,b); return d(gen);
  }
  int uniform_int(int a, int b) {
    std::uniform_int_distribution<int> d(a,b); return d(gen);
  }
  std::mt19937 gen;
};