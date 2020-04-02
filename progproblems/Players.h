#pragma once
#include <vector>
#include <array>
#include <nmmintrin.h>
#include <stdint.h>
#include <bitset>

// number of lineups to generate in optimizen - TODO make parameter
#define LINEUPCOUNT 100000
// number of simulations to run of a set of lineups to determine expected value
#define SIMULATION_COUNT 25000
// number of lineups we want to select from total pool
#define TARGET_LINEUP_COUNT 26
// number of pools to generate
#define NUM_ITERATIONS_OWNERSHIP 100
#define STOCHASTIC_OPTIMIZER_RUNS 50

using namespace std;

enum Position {
    qb = 0,
    rb = 1,
    wr = 2,
    te = 3,
    def = 4
};

constexpr int numPositions = 5;

constexpr int MaxPositionCount[5] = { 1, 3, 4, 2, 1 };
constexpr int PositionCount[5] = { 1, 2, 3, 1, 1 };
constexpr int slots[9] = { 0, 1, 1, 2, 2, 2, 3, 5/*flex*/, 4 };

struct Players2 {
    union {
        unsigned __int128 bits;
        std::bitset<128> set;
    };
    array<uint8_t, 5> posCounts;
    uint8_t totalCount;
    float value;
    bool hasFlex;

    inline bool addPlayer(int pos, float proj, int index)
    {
        if (!set[index])
        {
            set[index] = true;
            posCounts[pos]++;
            totalCount++;
            value += proj;
            return true;
        }
        return false;
    }

    bool tryAddPlayer(int pos, float proj, int index)
    {
        int diff = posCounts[pos] - PositionCount[pos];
        if (diff < 0)
        {
            return addPlayer(pos, proj, index);
        }

        if (hasFlex || pos == 0 || pos == 4 || diff > 1)
        {
            return false;
        }

        bool succeeded = addPlayer(pos, proj, index);
        hasFlex = succeeded;
        return succeeded;
    }

    bool operator==(const Players2& other) {
        return bits == other.bits;
    }

    Players2() : bits(0), totalCount(0), value(0), hasFlex(false)
    {
        posCounts.fill(0);
    }
};


bool operator==(const Players2& first, const Players2& other);
bool operator!=(const Players2& first, const Players2& other);
bool operator<(const Players2& lhs, const Players2& rhs);
bool operator==(const array<uint64_t, 2>& first, const array<uint64_t, 2>& other);

class set128_hash {
public:
    std::size_t operator() (const array<uint64_t, 2>& bs) const
    {
        return hash<uint64_t>()(bs[0]) ^ hash<uint64_t>()(bs[1]);
    }
};

struct lineup_set
{
    vector<vector<uint8_t>> set;
    float ev;
    float stdev;
    float getSharpe()
    {
        return (ev - (10 * set.size())) / stdev;
    }
    lineup_set() : ev(0.f), stdev(1.f) {}
    lineup_set(vector<vector<uint8_t>>& s) : set(s), ev(0.f), stdev(1.f)  {}
};

bool operator<(const lineup_set& lhs, const lineup_set& rhs);