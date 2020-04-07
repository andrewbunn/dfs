#pragma once
#include "Optimizer.h"
#include "Player.h"
#include <vector>

void greedyLineupSelector();
class Selector {
public:
  static int selectorCore(const std::vector<Player> &p,
                          const std::vector<lineup_t> &allLineups,
                          const std::vector<uint8_t> &corrPairs,
                          const std::vector<float> &corrCoeffs,
                          const std::array<float, 128> &projs,
                          const std::array<float, 128> &stdevs,
                          int lineupsIndexStart,
                          int lineupsIndexEnd, // request data
                          lineup_set &bestset  // request data
  );
};
