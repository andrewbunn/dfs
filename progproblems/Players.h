#pragma once
#include <vector>
#include <array>
#include <nmmintrin.h>
#include <stdint.h>
#include <bitset>
#include <numeric>

// number of lineups to generate in optimizen - TODO make parameter
#define LINEUPCOUNT 100000
// number of simulations to run of a set of lineups to determine expected value
#define SIMULATION_COUNT 25000
// number of lineups we want to select from total pool
#define TARGET_LINEUP_COUNT 10//26
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

constexpr int numSlots = 9;
constexpr int numPositions = 5;

constexpr int MaxPositionCount[numPositions] = { 1, 3, 4, 2, 1 };
constexpr int PositionCount[numPositions] = { 1, 2, 3, 1, 1 };
constexpr int slots[numSlots] = { 0, 1, 1, 2, 2, 2, 3, 5/*flex*/, 4 };

struct Players2  {
    union {
        unsigned __int128 bits;
        std::bitset<128> set;
    };
    uint32_t hasFlex : 1;
    uint32_t posCounts : 31;
    float value;

    constexpr int getPosCount(int pos) const
    {
        uint32_t shift = pos * 3;
        uint32_t bitmask = 7 << shift;
        return (posCounts & bitmask) >> shift;
    }

    constexpr void setPosCount(int pos, int count)
    {
        uint32_t shift = pos * 3;
        uint32_t bitmask = 7 << shift;
        posCounts &= ~bitmask;
        posCounts |= (count << shift);
    }

    int getTotalCount() const
    {
        //return accumulate(posCounts.begin(), posCounts.end(), 0);
        int sum = 0;
        for (int i = 0; i < numPositions; ++i)
        {
            sum += getPosCount(i);
        }
        return sum;
    }

    __attribute__((always_inline)) 
    bool addPlayer(int pos, int posCount, float proj, int index)
    {
        if (!set[index])
        {
            set[index] = true;
            setPosCount(pos, posCount);
            //posCounts[pos]++;
            value += proj;
            return true;
        }
        return false;
    }

    inline bool tryAddPlayer(int pos, float proj, int index)
    {
        int posCount = getPosCount(pos);
        int diff = posCount - PositionCount[pos];
        if (diff < 0)
        {
            return addPlayer(pos, posCount, proj, index);
        }

        if (hasFlex || pos == 0 || pos == 4 || diff > 1)
        {
            return false;
        }

        bool succeeded = addPlayer(pos, posCount, proj, index);
        hasFlex = succeeded;
        return succeeded;
    }

    bool operator==(const Players2& other) {
        return bits == other.bits;
    }

    Players2() : bits(0), hasFlex(0), posCounts(0), value(0) {}
    static constexpr bool isRBPos(int pos) { return pos == 1 || pos == 2; }
    static constexpr bool isWRPos(int pos) { return pos == 3 || pos == 4 || pos == 5; }
} __attribute__((packed));
static_assert(sizeof(Players2) == 24, "Struct not properly packed.");

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

using lineup_t = std::array<uint8_t, numSlots>;

struct lineup_set
{
    vector<lineup_t> set;
    float ev;
    float stdev;
    float getSharpe()
    {
        return (ev - (10 * set.size())) / stdev;
    }
    lineup_set() : ev(0.f), stdev(1.f) {}
    lineup_set(vector<lineup_t>& s) : set(s), ev(0.f), stdev(1.f)  {}
};

bool operator<(const lineup_set& lhs, const lineup_set& rhs);