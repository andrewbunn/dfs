#pragma once
#include <vector>
#include <array>

using namespace std;

enum Position {
    qb = 0,
    rb = 1,
    wr = 2,
    te = 3,
    def = 4
};

int numPositions = 5;

int MaxPositionCount[5] = { 1, 3, 4, 2, 1 };
int PositionCount[5] = { 1, 2, 3, 1, 1 };
int slots[9] = { 0, 1, 1, 2, 2, 2, 3, 5/*flex*/, 4 };


struct Players2 {
    uint64_t bitset;
    array<uint8_t, 5> posCounts;
    uint8_t totalCount;
    float value;
    bool hasFlex;

    inline bool addPlayer(int pos, float proj, int index)
    {
        uint64_t bit = (uint64_t)1 << index;
        if ((bitset & bit) == 0)
        {
            posCounts[pos]++;
            bitset |= bit;
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

    bool Players2::operator==(const Players2& other) {
        return bitset == other.bitset;
    }

    Players2() : totalCount(0), value(0), bitset(0), /*costOfUnfilledPositions(90),*/ hasFlex(false)
    {
        posCounts.fill(0);
    }
};

bool operator==(const Players2& first, const Players2& other) {
    return first.bitset == other.bitset;
}

bool operator<(const Players2& lhs, const Players2& rhs)
{
    // backwards so highest value is "lowest" (best ranked lineup)
    float diff = lhs.value - rhs.value;
    if (diff == 0)
    {
        return lhs.bitset > rhs.bitset;
    }
    else
    {
        return diff > 0;
    }
}

size_t players_hash(const Players2& ps)
{
    return hash<uint64_t>()(ps.bitset);
}

typedef vector<Players2> lineup_list;