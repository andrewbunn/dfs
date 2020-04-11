#include "Simulator.h"
#include "ParsedConstants.h"
#include "Selector.h"
#include <immintrin.h>
using namespace std;

// 128 hadd more efficient than 256
float sum8(__m256 x) {
  const auto hiQuad = _mm256_extractf128_ps(x, 1);
  const auto loQuad = _mm256_castps256_ps128(x);
  const auto sumQuad = _mm_add_ps(loQuad, hiQuad);
  const auto loDual = sumQuad;
  const auto hiDual = _mm_movehl_ps(sumQuad, sumQuad);
  const auto sumDual = _mm_add_ps(loDual, hiDual);
  const auto lo = sumDual;
  const auto hi = _mm_shuffle_ps(sumDual, sumDual, 0x1);
  const auto sum = _mm_add_ss(lo, hi);
  return _mm_cvtss_f32(sum);
}

pair<float, int> Simulator::runSimulationMaxWin(
    const float *standardNormals, const lineup_t *lineups,
    const int targetLineupCount, const float *projs, const float *stdevs,
    const vector<uint8_t> &corrPairs, const vector<float> &corrCoeffs) {
  alignas(256) array<float, 128> playerScores;
  array<int, lineupChunkSize> winningThresholdsHit{0};

  for (int index = 0; index < SIMULATION_COUNT; index++) {
    constexpr auto avx_width_bytes = 256 / 8;
    constexpr auto floats_per_avx = avx_width_bytes / 4;
    const float *playerStandardNormals =
        &standardNormals[all_players_size * index];

    for (int i = 0; i < all_players_size; i += floats_per_avx) {
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

    for (int i = 0; i < all_players_size; i += floats_per_avx) {
      auto vpr = _mm256_load_ps(reinterpret_cast<float const *>(projs + i));
      auto vsc = _mm256_load_ps(reinterpret_cast<float *>(&playerScores[i]));
      auto vr = _mm256_add_ps(vpr, vsc);
      _mm256_store_ps(reinterpret_cast<float *>(&playerScores[i]), vr);
    }

    const auto maskAll = _mm256_set1_epi32(0xFFFFFFFF);
    for (int i = 0; i < lineupChunkSize; ++i) {
      int winnings = 0;
      for (int k = 0; k < targetLineupCount; k++) {
        auto &lineup = lineups[k + i * targetLineupCount];

        const auto vindex = _mm256_maskload_epi32(
            reinterpret_cast<int const *>(&lineup[0]), maskAll);
        const auto v = _mm256_i32gather_ps(&playerScores[0], vindex, 4);

        const float lineupScore = sum8(v) + playerScores[lineup[8]];
        if (lineupScore >= SimulationTargetScore) {
          winnings = 1;
          break;
        }
      }
      winningThresholdsHit[i] += winnings;
    }
  }

  const auto it =
      max_element(winningThresholdsHit.begin(), winningThresholdsHit.end());
  const float expectedValue = (float)(*it) / (float)SIMULATION_COUNT;

  return {expectedValue, distance(winningThresholdsHit.begin(), it)};
}
