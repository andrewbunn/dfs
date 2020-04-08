#pragma once
#include "Optimizer.h"
#include "Player.h"
#include <vector>
#include <immintrin.h>

template <class T>
class LCG;

constexpr int SimulationTargetScore = 170;
class Simulator {
public:
  static void normaldistf_boxmuller_avx(float *data, size_t count,
                                        LCG<__m256> &r);
  static std::pair<float, int>
  runSimulationMaxWin(const float *standardNormals, const lineup_t *lineups,
                      int targetLineupCount, const float *projs,
                      const float *stdevs,
                      const std::vector<uint8_t> &corrPairs,
                      const std::vector<float> &corrCoeffs);
};
