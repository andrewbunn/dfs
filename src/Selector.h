#pragma once
#include "Optimizer.h"
#include "Player.h"
#include <vector>

constexpr int lineupChunkSize = 128;

void greedyLineupSelector();
class Selector {
public:
  static int selectorCore(const std::vector<lineup_t> &allLineups,
                          const std::vector<uint8_t> &corrPairs,
                          const std::vector<float> &corrCoeffs,
                          const std::array<float, 128> &projs,
                          const std::array<float, 128> &stdevs,
                          const int lineupsIndexStart,
                          const int lineupsIndexEnd, lineup_set &bestset);
};
