#pragma once
#include "Player.h"
#include <array>
#include <bitset>
#include <nmmintrin.h>
#include <numeric>
#include <stdint.h>
#include <unordered_map>
#include <vector>

// number of lineups to generate in optimizen - TODO make parameter
constexpr size_t LINEUPCOUNT = 100000;
// number of simulations to run of a set of lineups to determine expected value
constexpr size_t SIMULATION_COUNT = 25000;
// number of lineups we want to select from total pool
constexpr size_t TARGET_LINEUP_COUNT = 10; // 26

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

enum Position { qb = 0, rb = 1, wr = 2, te = 3, def = 4 };

constexpr int NumLineupSlots = 9;
constexpr int numPositions = 5;

constexpr int MaxPositionCount[numPositions] = {1, 3, 4, 2, 1};
constexpr uint8_t PositionCount[numPositions] = {1, 2, 3, 1, 1};
constexpr int slots[NumLineupSlots] = {0, 1, 1, 2, 2, 2, 3, 5 /*flex*/, 4};

void runPlayerOptimizerN(std::string filein, std::string fileout,
                         std::string lineupstart);

struct OptimizerLineup {
  static constexpr bool isRBPos(int pos) { return pos == 1 || pos == 2; }
  static constexpr bool isWRPos(int pos) {
    return pos == 3 || pos == 4 || pos == 5;
  }
  static constexpr bool isTEPos(int pos) { return pos == 6; }
  static constexpr bool isFlexPos(int pos) { return pos == 7; }
  static constexpr bool isLastPos(int pos) { return pos == 8; }
  static constexpr int getFlexTransposeIndex(const int rbStartPos,
                                             const int wrStartPos,
                                             const int teStartPos) {
    return teStartPos * 65536 + rbStartPos * 256 + wrStartPos;
  }

  union {
    unsigned __int128 bits;
    std::bitset<128> set;
  };
  uint32_t hasFlex : 1;
  uint32_t posCounts : 31;
  float value;

  constexpr int getPosCount(int pos) const {
    const uint32_t shift = pos * numBits_PosCount;
    const uint32_t bitmask = bitMask_PosCount << shift;
    return (posCounts & bitmask) >> shift;
  }

  constexpr void setPosCount(int pos, int count) {
    const uint32_t shift = pos * numBits_PosCount;
    const uint32_t bitmask = bitMask_PosCount << shift;
    posCounts &= ~bitmask;
    posCounts |= (count << shift);
  }

  int getTotalCount() const {
    int sum = 0;
    for (int i = 0; i < numPositions; ++i) {
      sum += getPosCount(i);
    }
    return sum;
  }

  OptimizerLineup() : bits(0), hasFlex(0), posCounts(0), value(0) {}

  bool operator==(const OptimizerLineup &other) { return bits == other.bits; }

  inline bool tryAddPlayer(bool isFlex, int pos, float proj, int index) {
    const int posCount = getPosCount(pos);
    const int diff = posCount - PositionCount[pos];
    if (diff < 0) {
      return addPlayer(pos, posCount, proj, index);
    }
    if (!isFlex || hasFlex || diff > 0) {
      return false;
    }
    const bool succeeded = addPlayer(pos, posCount, proj, index);
    hasFlex = succeeded;
    return succeeded;
  }

private:
  static constexpr uint32_t numBits_PosCount = 4;
  static constexpr uint32_t bitMask_PosCount = (1 << numBits_PosCount) - 1;
  __attribute__((always_inline)) bool addPlayer(int pos, int posCount,
                                                float proj, int index) {
    // the check set should be unnecessary if not using skip pos
    // but maybe covering a bug?
    if (!set[index]) {
      set[index] = true;
      setPosCount(pos, posCount + 1);
      value += proj;
      return true;
    }
    return false;
  }

} __attribute__((packed));
static_assert(sizeof(OptimizerLineup) == 24, "Struct not properly packed.");

bool operator==(const OptimizerLineup &first, const OptimizerLineup &other);
bool operator!=(const OptimizerLineup &first, const OptimizerLineup &other);
bool operator<(const OptimizerLineup &lhs, const OptimizerLineup &rhs);
bool operator==(const std::array<uint64_t, 2> &first,
                const std::array<uint64_t, 2> &other);

struct BitsetIter {
  union {
    unsigned __int128 bits;
    std::bitset<128> set;
  };

  BitsetIter(std::bitset<128> s) : set(s) {}

  __attribute__((always_inline)) int next() {
    unsigned __int128 bt = bits;
    uint64_t b1 = bt;
    int offset = 0;
    if (!b1) {
      bt >>= 64;
      b1 = bt;
      offset = 64;
    }
    auto ctz = __builtin_ctzll(b1);
    int index = ctz + offset;
    set[index] = false;
    return index;
  }

  bool hasNext() { return set.any(); }
};

class set128_hash {
public:
  std::size_t operator()(const std::array<uint64_t, 2> &bs) const {
    return std::hash<uint64_t>()(bs[0]) ^ std::hash<uint64_t>()(bs[1]);
  }
};

// using lineup_t = std::array<uint8_t, NumLineupSlots>;
using lineup_t = std::array<uint32_t, NumLineupSlots>;

struct lineup_set {
  std::vector<lineup_t> set;
  float ev;
  float stdev;
  float getSharpe() { return (ev - (10 * set.size())) / stdev; }
  lineup_set() : ev(0.f), stdev(1.f) {}
  lineup_set(std::vector<lineup_t> &s) : set(s), ev(0.f), stdev(1.f) {}
};

bool operator<(const lineup_set &lhs, const lineup_set &rhs);

struct IntHasher {
  std::size_t operator()(const int k) const {
    uint32_t x = k;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
  }
};

class Optimizer {
public:
  Optimizer() {}
  std::vector<OptimizerLineup>
  generateLineupN(const std::vector<Player> &p,
                  const std::vector<std::string> &disallowedPlayers,
                  const OptimizerLineup currentPlayers, const int budgetUsed);

private:
  std::vector<OptimizerLineup> knapsackPositionsN(
      const int budget, const int pos, const OptimizerLineup oldLineup,
      const std::vector<std::vector<Player>> &players, const int rbStartPos,
      const int wrStartPos, const int teStartPos,
      std::bitset<NumLineupSlots> skipPositionSet);

  void knapsackPositionsN3(const int budget, const int pos,
                           const OptimizerLineup oldLineup,
                           const std::vector<std::vector<Player>> &players,
                           const int rbStartPos, const int wrStartPos,
                           const int teStartPos,
                           std::bitset<NumLineupSlots> skipPositionSet);

  __attribute__((noinline)) bool knapsack_helperSkipPosition(
      const int budget, const int pos, const OptimizerLineup oldLineup,
      const std::vector<std::vector<Player>> &players, const int rbStartPos,
      const int wrStartPos, const int teStartPos,
      std::bitset<NumLineupSlots> skipPositionSet);
  void initializeThreadLocalData();

  std::unordered_map<int, const std::vector<Player>, IntHasher> _filteredFlex;
  // each thread has a pre allocated vector for each depth so we don't have
  // cross thread malloc contention.
  static thread_local std::array<std::vector<OptimizerLineup>, NumLineupSlots>
      _depth_arrs;
};
