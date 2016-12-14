#include "Players.h"

bool operator==(const Players2& first, const Players2& other) {
    return (first.bitset1 == other.bitset1) && (first.bitset2 == other.bitset2);
}

bool operator!=(const Players2& first, const Players2& other) {
    return (first.bitset1 != other.bitset1) || (first.bitset2 != other.bitset2);
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

bool operator==(const array<uint64_t, 2>& first, const array<uint64_t, 2>& other) {
    return (first[0] == other[0]) && (first[1] == other[1]);
}

bool operator<(const lineup_set& lhs, const lineup_set& rhs)
{
    // backwards so highest value is "lowest" (best ranked lineup)
    float diff = lhs.ev - rhs.ev;
    if (diff == 0)
    {
        return lhs.stdev < rhs.stdev;
    }
    else
    {
        return diff > 0;
    }
}