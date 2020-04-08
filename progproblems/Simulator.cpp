#include "Simulator.h"
#include "ParsedConstants.h"
#include "Selector.h"
#include <immintrin.h>
using namespace std;

float sum8(__m256 x) {
  // hiQuad = ( x7, x6, x5, x4 )
  const __m128 hiQuad = _mm256_extractf128_ps(x, 1);
  // loQuad = ( x3, x2, x1, x0 )
  const __m128 loQuad = _mm256_castps256_ps128(x);
  // sumQuad = ( x3 + x7, x2 + x6, x1 + x5, x0 + x4 )
  const __m128 sumQuad = _mm_add_ps(loQuad, hiQuad);
  // loDual = ( -, -, x1 + x5, x0 + x4 )
  const __m128 loDual = sumQuad;
  // hiDual = ( -, -, x3 + x7, x2 + x6 )
  const __m128 hiDual = _mm_movehl_ps(sumQuad, sumQuad);
  // sumDual = ( -, -, x1 + x3 + x5 + x7, x0 + x2 + x4 + x6 )
  const __m128 sumDual = _mm_add_ps(loDual, hiDual);
  // lo = ( -, -, -, x0 + x2 + x4 + x6 )
  const __m128 lo = sumDual;
  // hi = ( -, -, -, x1 + x3 + x5 + x7 )
  const __m128 hi = _mm_shuffle_ps(sumDual, sumDual, 0x1);
  // sum = ( -, -, -, x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 )
  const __m128 sum = _mm_add_ss(lo, hi);
  return _mm_cvtss_f32(sum);
}

float sum8_v2(__m256 x) {
  __m256 t1 = _mm256_hadd_ps(x, x);
  __m256 t2 = _mm256_hadd_ps(t1, t1);
  __m128 t3 = _mm256_extractf128_ps(t2, 1);
  __m128 t4 = _mm_add_ss(_mm256_castps256_ps128(t2), t3);
  return _mm_cvtss_f32(t4);
}

pair<float, int> Simulator::runSimulationMaxWin(
    const float *standardNormals, const lineup_t *lineups,
    const int targetLineupCount, const float *projs, const float *stdevs,
    const vector<uint8_t> &corrPairs, const vector<float> &corrCoeffs) {
  array<int, lineupChunkSize> winningThresholdsHit{0};

  for (int index = 0; index < SIMULATION_COUNT; index++) {
    const float *playerStandardNormals =
        &standardNormals[all_players_size * index];
    alignas(256) array<float, 128> playerScores;

    for (int i = 0; i < all_players_size; i += 8) {
      auto vsd = _mm256_load_ps(reinterpret_cast<float const *>(stdevs + i));
      auto vsn = _mm256_loadu_ps(
          reinterpret_cast<float const *>(playerStandardNormals + i));
      auto vr = _mm256_mul_ps(vsd, vsn);
      _mm256_store_ps(reinterpret_cast<float *>(&playerScores[i]), vr);
    }

    for (int i = 1; i < corrPairs.size(); i += 2) {
      const float z1 =
          playerStandardNormals[corrPairs[i - 1]] * corrCoeffs[i - 1];
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

    const __m256i maskAll = _mm256_set1_epi32(0xFFFFFFFF);
    for (int i = 0; i < lineupChunkSize; ++i) {
      int winnings = 0;
      for (int k = 0; k < targetLineupCount; k++) {
        auto &lineup = lineups[k + i * targetLineupCount];

        /*auto vindex = _mm256_set_epi32(lineup[0], lineup[1], lineup[2],
           lineup[3], lineup[4], lineup[5], lineup[6], lineup[7]);*/
        auto vindex = _mm256_maskload_epi32(
            reinterpret_cast<int const *>(&lineup[0]), maskAll);
        auto v = _mm256_i32gather_ps(&playerScores[0], vindex, 4);

        float lineupScore = sum8(v) + playerScores[lineup[8]];
        if (lineupScore >= 170) {
          winnings = 1;
        }
      }
      winningThresholdsHit[i] += winnings;
    }
  }

  auto it =
      max_element(winningThresholdsHit.begin(), winningThresholdsHit.end());
  float expectedValue = (float)(*it) / (float)SIMULATION_COUNT;

  return {expectedValue, distance(winningThresholdsHit.begin(), it)};
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
