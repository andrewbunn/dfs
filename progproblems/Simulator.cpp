#include "Simulator.h"
#include <immintrin.h>
using namespace std;

pair<float, int> Simulator::runSimulationMaxWin(
    const float *standardNormals, const lineup_t *lineups,
    int currentLineupCount, int targetLineupCount,
    const vector<Player> &allPlayers, const float *projs, const float *stdevs,
    const vector<uint8_t> &corrPairs, const vector<float> &corrCoeffs) {
  // evaluate multiple lineups at once, reduce number of calls to this fn
  array<int, 64> winningThresholdsHit{0};

  for (int index = 0; index < SIMULATION_COUNT; index++) {
    const float *playerStandardNormals =
        &standardNormals[allPlayers.size() * index];
    alignas(256) array<float, 128> playerScores;

    const size_t all_players_size = 88; // constexpr parse from players file
    // const size_t max = allPlayers.size();
    // probably do loop iters to playerssize rounded up to mult of 8? or maybe
    // keep 128 since unrolled
    for (int i = 0; i < all_players_size; i += 8) {
      auto vsd = _mm256_load_ps(reinterpret_cast<float const *>(stdevs + i));
      auto vsn = _mm256_loadu_ps(
          reinterpret_cast<float const *>(playerStandardNormals + i));
      auto vr = _mm256_mul_ps(vsd, vsn);
      _mm256_store_ps(reinterpret_cast<float *>(&playerScores[i]), vr);
    }

    for (int i = 1; i < corrPairs.size(); i += 2) {
      float z1 = playerStandardNormals[corrPairs[i - 1]] * corrCoeffs[i - 1];
      playerScores[corrPairs[i]] =
          stdevs[i] *
          (playerStandardNormals[corrPairs[i]] * corrCoeffs[i] + z1);
    }

    for (int i = 0; i < all_players_size; i += 8) {
      auto vpr = _mm256_load_ps(reinterpret_cast<float const *>(projs + i));
      auto vsc = _mm256_load_ps(reinterpret_cast<float *>(&playerScores[i]));
      auto vr = _mm256_add_ps(vpr, vsc);
      _mm256_store_ps(reinterpret_cast<float *>(&playerScores[i]), vr);
    }

    // if we pass all lineup sets here, have to eval them all efficiently in
    // parallel? 64 lineup sets here, can scatter/gather from small arr to big
    // arr real opt target here:
    int c = 0;
    // vgatherdps
    // lineup needs to be bitmask of indices into playerScores
    // then horizontal add
    // _mm256_i32gather_ps (float const* base_addr, __m256i vindex, const int
    // scale) _mm256_load_epi32 idx =
    // _mm256_set_epi32(100,101,102,103,104,105,106,107); // if we want to load
    // this, then lineup needs to be 256 wide or can set it from lineup somehow?
    // _mm256_i32gather_ps (&playerScores[0], vindex, 4) scale bytes -> floats,
    // could also do 8 to cover more area can only hold 8 :(((((
    for (int i = 0; i < currentLineupCount; ++i) {
      // auto& l = lineups[i];
      int winnings = 0;
      for (int k = 0; k < targetLineupCount; k++) {
        auto &lineup = lineups[k + i * targetLineupCount];
        float lineupScore = 0.f;
        for (auto player : lineup) {
          lineupScore += playerScores[player];
        }
        // only count > threshold
        if (lineupScore >= 170) {
          winnings = 1;
        }
      }
      winningThresholdsHit[c++] += winnings;
    }
  }
  int c = 0;
  int imax = 0;
  int wmax = 0;
  for (auto w : winningThresholdsHit) {
    if (w > wmax) {
      wmax = w;
      imax = c;
    }
    c++;
  }
  float expectedValue = (float)wmax / (float)SIMULATION_COUNT;

  return {expectedValue, imax};
}

#define CONTENDED_PLACEMENT_SLOTS 14

// cutoffs is sorted array of the average score for each prize cutoff
// alternatively we can do regression to have function of value -> winnings
inline float determineWinnings(float score,
                               array<
                                   uint8_t, CONTENDED_PLACEMENT_SLOTS> &placements /*, vector<float>& winningsCutoffs, vector<float>& winningsValues*/) {
  static array<int, 22> winnings = {
      10000, 5000, 3000, 2000, 1000, 750, 500, 400, 300, 200, 150,
      100,   75,   60,   50,   45,   40,  35,  30,  25,  20,  15};

  static array<float, 22> cutoffs = {
      181.14, 179.58, 173.4,  172.8,  171.14, 168.7,  167,    165.1,
      162.78, 160,    158.68, 156.3,  154.2,  151.64, 149.94, 147.18,
      142.4,  138.18, 135.1,  130.24, 126.34, 121.3};
  // number of top level placements we can have in each slot
  static array<uint8_t, CONTENDED_PLACEMENT_SLOTS> slotsAvailable = {
      1, 1, 1, 1, 1, 5, 5, 5, 10, 10, 10, 25, 25, 50};

  // binary search array of cutoffs
  auto it =
      lower_bound(cutoffs.begin(), cutoffs.end(), score, greater<float>());
  float value;
  if (it != cutoffs.end()) {
    if (it - cutoffs.begin() < CONTENDED_PLACEMENT_SLOTS) {
      // we have 50 slots available so as long as lineupset coun t doesnt
      // increase this can't overflow. still almost impossible
      while (placements[it - cutoffs.begin()] ==
             slotsAvailable[it - cutoffs.begin()]) {
        it++;
      }
      placements[it - cutoffs.begin()]++;
    }
    value = static_cast<float>(winnings[it - cutoffs.begin()]);
  } else {
    value = 0;
  }
  return value;
}
