#pragma once
#include "Optimizer.h"
#include "Player.h"
#include <vector>

class Simulator {
public:
  static std::pair<float, int>
  runSimulationMaxWin(const float *standardNormals, const lineup_t *lineups,
                      int targetLineupCount, const float *projs,
                      const float *stdevs,
                      const std::vector<uint8_t> &corrPairs,
                      const std::vector<float> &corrCoeffs);
};
