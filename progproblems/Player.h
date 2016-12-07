#pragma once
#include <string>

using namespace std;

struct Player {
    string name;
    float proj;
    float stdDev;
    uint8_t cost;
    uint8_t pos;
    uint8_t index;
    Player(string s, uint8_t c, float p, uint8_t pos, uint8_t idx, float sd) : name(s), cost(c), proj(p), stdDev(sd), pos(pos), index(idx)
    {}

    Player() {}
};