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

bool operator==(const Players2& first, const Players2& other) {
    return (first.bitset1 == other.bitset1) && (first.bitset2 == other.bitset2);
}

bool operator<(const Players2& lhs, const Players2& rhs)
{
    // backwards so highest value is "lowest" (best ranked lineup)
    float diff = lhs.value - rhs.value;
    if (diff == 0)
    {
        if (lhs.bitset2 == rhs.bitset2)
        {
            return lhs.bitset1 > rhs.bitset1;
        }
        else
        {
            return lhs.bitset2 > rhs.bitset2;
        }
    }
    else
    {
        return diff > 0;
    }
}

typedef vector<Players2> lineup_list;


bool operator==(const array<uint64_t, 2>& first, const array<uint64_t, 2>& other) {
    return (first[0] == other[0]) && (first[1] == other[1]);
}

class set128_hash {
public:
    std::size_t operator() (const array<uint64_t, 2>& bs) const
    {
        return hash<uint64_t>()(bs[0]) ^ hash<uint64_t>()(bs[1]);
    }
};