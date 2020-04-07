#pragma once
#include <string>

struct Player {
  std::string name;
  float proj;
  float stdDev;
  uint8_t cost;
  uint8_t pos;
  uint8_t index;
  Player(std::string s, uint8_t c, float p, uint8_t pos, uint8_t idx, float sd)
      : name(s), proj(p), stdDev(sd), cost(c), pos(pos), index(idx) {}

  Player() {}
};
