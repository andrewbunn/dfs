#pragma once
#include <vector>
#include <array>


// number of lineups to generate in optimizen - TODO make parameter
#define LINEUPCOUNT 100000
// number of simulations to run of a set of lineups to determine expected value
#define SIMULATION_COUNT 20000
// number of random lineup sets to select
#define RANDOM_SET_COUNT 10000
// number of lineups we want to select from total pool
#define TARGET_LINEUP_COUNT 30
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
    uint64_t bitset1;
    uint64_t bitset2;
    array<uint8_t, 5> posCounts;
    uint8_t totalCount;
    float value;
    bool hasFlex;

    inline bool addPlayer(int pos, float proj, int index)
    {
        if (index > 63)
        {
            uint64_t bit = (uint64_t)1 << (index - 64);
            if ((bitset2 & bit) == 0)
            {
                posCounts[pos]++;
                bitset2 |= bit;
                totalCount++;
                value += proj;
                return true;
            }
        }
        else
        {
            uint64_t bit = (uint64_t)1 << index;
            if ((bitset1 & bit) == 0)
            {
                posCounts[pos]++;
                bitset1 |= bit;
                totalCount++;
                value += proj;
                return true;
            }
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

    bool Players2::operator==(const Players2& other) {
        return (bitset1 == other.bitset1) && (bitset2 == other.bitset2);
    }

    Players2() : totalCount(0), value(0), bitset1(0), bitset2(0), hasFlex(false)
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
    lineup_set(vector<vector<uint8_t>>& s) : ev(0.f), stdev(1.f), set(s) {}
};

bool operator<(const lineup_set& lhs, const lineup_set& rhs);