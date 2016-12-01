#pragma once
#include <climits>
#include <random>

class xor128 {
public:
    static constexpr unsigned min() { return 0u; }
    static constexpr unsigned max() { return UINT_MAX; }
    unsigned operator()() { return random(); }
    xor128();
    xor128(unsigned s) { w = s; }
private:
    unsigned x = 123456789u, y = 362436069u, z = 521288629u, w;
    unsigned random();
};
xor128::xor128() {
    std::random_device rd;
    w = rd();

    for (int i = 0; i<10; i++) {
        random();
    }
}
unsigned xor128::random() {
    unsigned t;
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    return w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
}
