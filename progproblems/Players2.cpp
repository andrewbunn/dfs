#include "Players.h"
#include <immintrin.h>

bool operator==(const Players2& first, const Players2& other) {
    return (first.bits == other.bits);
}

bool operator!=(const Players2& first, const Players2& other) {
    return (first.bits != other.bits);
}

bool operator<(const Players2& lhs, const Players2& rhs)
{
    if (lhs.value != rhs.value)
        return lhs.value > rhs.value;
    return lhs.bits > rhs.bits;
}

bool operator==(const array<uint64_t, 2>& first, const array<uint64_t, 2>& other) {
    return (first[0] == other[0]) && (first[1] == other[1]);
}

bool operator<(const lineup_set& lhs, const lineup_set& rhs)
{
    // backwards so highest value is "lowest" (best ranked lineup)
    if (lhs.ev != rhs.ev)
    {
        return lhs.ev > rhs.ev;
        
    }
    return lhs.stdev < rhs.stdev;
}