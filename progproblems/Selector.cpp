#include "Selector.h"
#include "ParsedConstants.h"
#include "Simulator.h"
#include "lcg.h"
#include "parsing.h"
#include <algorithm>
#include <execution>
#include <random>

using namespace std;

static void normaldistf_boxmuller_avx(float *data, size_t count,
                                      LCG<__m256> &r) {
  // assert(count % 16 == 0);
  const __m256 twopi = _mm256_set1_ps(2.0f * 3.14159265358979323846f);
  const __m256 one = _mm256_set1_ps(1.0f);
  const __m256 minustwo = _mm256_set1_ps(-2.0f);

  for (size_t i = 0; i < count; i += 16) {
    __m256 u1 = _mm256_sub_ps(one, r()); // [0, 1) -> (0, 1]
    __m256 u2 = r();
    __m256 radius = _mm256_sqrt_ps(_mm256_mul_ps(minustwo, log256_ps(u1)));
    __m256 theta = _mm256_mul_ps(twopi, u2);
    __m256 sintheta, costheta;
    sincos256_ps(theta, &sintheta, &costheta);
    _mm256_store_ps(&data[i], _mm256_mul_ps(radius, costheta));
    _mm256_store_ps(&data[i + 8], _mm256_mul_ps(radius, sintheta));
  }
}

vector<string> enforceOwnershipLimits(
    const vector<Player> &p, const array<int, 256> &playerCounts,
    const vector<pair<uint8_t, float>> &ownershipLimits, int numLineups,
    uint64_t &disallowedSet1, uint64_t &disallowedSet2) {
  vector<string> playersToRemove;
  for (auto &x : ownershipLimits) {
    float percentOwned = (float)playerCounts[x.first] / (float)numLineups;
    if (percentOwned >= x.second) {
      if (x.first > 63) {
        disallowedSet2 |= (uint64_t)1 << (x.first - 64);
      } else {
        disallowedSet1 |= (uint64_t)1 << x.first;
      }
      playersToRemove.push_back(p[x.first].name);
    }
  }
  for (uint16_t i = 0; i < playerCounts.size(); i++) {
    if (playerCounts[i]) {
      float percentOwned = (float)playerCounts[i] / (float)numLineups;
      if (percentOwned > 0.6 &&
          find_if(ownershipLimits.begin(), ownershipLimits.end(), [i](auto &z) {
            return z.first == i;
          }) == ownershipLimits.end()) {
        if (i > 63) {
          disallowedSet2 |= (uint64_t)1 << (i - 64);
        } else {
          disallowedSet1 |= (uint64_t)1 << i;
        }
        playersToRemove.push_back(p[i].name);
      }
    }
  }
  return playersToRemove;
}

int Selector::selectorCore(const vector<lineup_t> &allLineups,
                           const vector<uint8_t> &corrPairs,
                           const vector<float> &corrCoeffs,
                           const array<float, 128> &projs,
                           const array<float, 128> &stdevs,
                           int lineupsIndexStart,
                           int lineupsIndexEnd, // request data
                           lineup_set &bestset  // request data
) {
  bestset.ev = 0.f;
  vector<int> lineupChunkStarts;
  for (int j = lineupsIndexStart; j < lineupsIndexEnd; j += lineupChunkSize) {
    lineupChunkStarts.push_back(j);
  }

  // allocate giant vector of memory for all lineup variations
  // lineupChunkStarts.size() * lineupChunkSize
  const size_t target_lineup_count = (1 + bestset.set.size());
  vector<lineup_t> allLineupCombinations(lineupChunkStarts.size() *
                                         lineupChunkSize * target_lineup_count);
  vector<lineup_set> chunkResults(lineupChunkStarts.size());
  transform(
      execution::par, lineupChunkStarts.begin(), lineupChunkStarts.end(),
      chunkResults.begin(),
      [&allLineups, &bestset, &projs, &stdevs, &corrPairs, &corrCoeffs,
       &allLineupCombinations, &target_lineup_count](int lineupChunkStart) {
        static thread_local size_t unaligned_alloc_size =
            (size_t)all_players_size * (size_t)SIMULATION_COUNT + 256;
        static thread_local unique_ptr<float[]> standardNormalsAlloc(
            new float[unaligned_alloc_size]);
        static thread_local size_t aligned_alloc_size =
            unaligned_alloc_size * 4;
        static thread_local void *standardNormals_unaligned =
            standardNormalsAlloc.get();
        static thread_local float *standardNormals =
            reinterpret_cast<float *>(std::align(
                128, 256, standardNormals_unaligned, aligned_alloc_size));
        static thread_local random_device rdev;

        /*int currentLineupCount =
            min(lineupChunkSize, (int)(allLineups.size()) - lineupChunkStart);*/
        const size_t n = (size_t)all_players_size * (size_t)SIMULATION_COUNT;
        unsigned seed1 =
            std::chrono::system_clock::now().time_since_epoch().count();
        alignas(256) LCG<__m256> lcg(seed1, rdev(), rdev(), rdev(), rdev(),
                                     rdev(), rdev(), rdev());

        normaldistf_boxmuller_avx(&standardNormals[0], n, lcg);

        int indexBegin = lineupChunkStart;
        int indexEnd = indexBegin + lineupChunkSize;

        lineup_t *results = &allLineupCombinations[lineupChunkStart];
        int k = 0;
        for (int i = indexBegin; i < indexEnd; i++) {
          if (bestset.set.size() > 0)
            memcpy(results + k, &bestset.set[0],
                   bestset.set.size() * sizeof(lineup_t));
          memcpy(results + k + bestset.set.size(), &allLineups[i],
                 sizeof(lineup_t));

          k += target_lineup_count;
        }

        auto [ev, it] = Simulator::runSimulationMaxWin(
            &standardNormals[0], results, target_lineup_count, &projs[0],
            &stdevs[0], corrPairs, corrCoeffs);
        lineup_set res;
        for (int i = 0; i < target_lineup_count; ++i) {
          res.set.push_back(results[it * target_lineup_count + i]);
        }
        res.ev = ev;
        return res;
      });
  auto itResult = min_element(chunkResults.begin(), chunkResults.end());
  bestset = *itResult;

  auto itLineups = find(allLineups.begin(), allLineups.end(),
                        bestset.set[bestset.set.size() - 1]);
  return distance(allLineups.begin(), itLineups);
}

// increase size to nearest multiple of 64 for clean loops in sim code.
void padAllLineupsSize(vector<lineup_t> &allLineups) {
  const size_t allLineupsSize = allLineups.size();
  const size_t allLineupsSizeRounded =
      ((allLineupsSize + (lineupChunkSize / 2)) / lineupChunkSize) *
      lineupChunkSize;
  constexpr unsigned int dead_index = all_players_size - 1;
  constexpr lineup_t dead_lineup = {dead_index, dead_index, dead_index,
                                    dead_index, dead_index, dead_index,
                                    dead_index, dead_index, dead_index};
  for (size_t i = allLineupsSize; i < allLineupsSizeRounded; ++i) {
    // should be dead index from playersSize -> roundedPlayersSize
    // won't work if playersSize is multiple of 8, will need to address later.
    allLineups.push_back(dead_lineup);
  }
}

void greedyLineupSelector() {
  const vector<Player> p = parsePlayers("players.csv");
  vector<tuple<string, string, float>> corr = parseCorr("corr.csv");

  vector<pair<string, float>> ownership = parseOwnership("ownership.csv");

  vector<uint8_t> corrPairs;
  vector<float> corrCoeffs;
  for (auto &s : corr) {
    // move those entries to the start of the array
    // only when we have the pair
    auto it = find_if(p.begin(), p.end(),
                      [&s](auto &p) { return p.name == get<0>(s); });
    auto itC = find_if(p.begin(), p.end(),
                       [&s](auto &p) { return p.name == get<1>(s); });
    if (it != p.end() && itC != p.end()) {
      float r = get<2>(s);
      float zr = sqrt(1 - r * r);
      corrPairs.push_back(it->index);
      corrPairs.push_back(itC->index);
      corrCoeffs.push_back(r);
      corrCoeffs.push_back(zr);
    }
  }

  unordered_map<string, uint8_t> playerIndices;
  array<int, 256> playerCounts{};

  // vectorized projection and stddev data
  alignas(256) array<float, 128> projs{};
  alignas(256) array<float, 128> stdevs{};
  for (const auto &x : p) {
    playerIndices.emplace(x.name, x.index);
    projs[x.index] = (x.proj);
    stdevs[x.index] = (x.stdDev);
  }

  vector<pair<uint8_t, float>> ownershipLimits;
  for (auto &x : ownership) {
    auto it = playerIndices.find(x.first);
    if (it != playerIndices.end()) {
      ownershipLimits.emplace_back(it->second, x.second);
    }
  }

  // unsigned seed1 =
  // std::chrono::system_clock::now().time_since_epoch().count();
  const auto start = chrono::steady_clock::now();

  vector<lineup_t> allLineups = parseLineups("output.csv", playerIndices);

  uint64_t currentDisallowedSet1 = 0;
  uint64_t currentDisallowedSet2 = 0;
  vector<string> currentPlayersRemoved;

  // choose lineup that maximizes objective
  // iteratively add next lineup that maximizes objective.
  lineup_set bestset;
  bestset.set.reserve(TARGET_LINEUP_COUNT);
  vector<int> bestsetIndex;
  bestsetIndex.reserve(TARGET_LINEUP_COUNT);

  // int z = 0;
  for (int i = 0; i < TARGET_LINEUP_COUNT; i++) {
    padAllLineupsSize(allLineups);
    int lineupsIndexStart = 0;
    int lineupsIndexEnd = allLineups.size();

    bestsetIndex.push_back(Selector::selectorCore(
        allLineups, corrPairs, corrCoeffs, projs, stdevs, lineupsIndexStart,
        lineupsIndexEnd, // request data
        bestset          // request data
        ));

    const auto end = chrono::steady_clock::now();
    const auto diff = end - start;
    const double msTime = chrono::duration<double, milli>(diff).count();
    cout << "Lineups: " << (i + 1) << " EV: " << bestset.ev
         << " elapsed time: " << msTime << endl;

    for (const auto &x : bestset.set[bestset.set.size() - 1]) {
      cout << p[x].name;
      cout << ",";
    }
    cout << endl;
    cout << endl;

    // rather than "enforced ownership" we should just have ownership caps
    // eg. DJ @ 60%, after player exceeds threshold, we can rerun optimizen, and
    // work with new player set
    for (const auto x : bestset.set[bestset.set.size() - 1]) {
      playerCounts[x]++;
    }

    if ((i > 1) && (i % 2 == 0) && ((i + 1) < TARGET_LINEUP_COUNT)) {
      uint64_t disallowedSet1 = 0;
      uint64_t disallowedSet2 = 0;
      vector<string> playersToRemove = enforceOwnershipLimits(
          p, playerCounts, ownershipLimits, bestset.set.size(), disallowedSet1,
          disallowedSet2);

      if (disallowedSet1 != currentDisallowedSet1 ||
          disallowedSet2 != currentDisallowedSet2) {
        cout << "Removing players: ";
        for (auto &s : playersToRemove) {
          cout << s << ",";
        }
        cout << endl;

        currentPlayersRemoved = playersToRemove;
        currentDisallowedSet1 = disallowedSet1;
        currentDisallowedSet2 = disallowedSet2;
        double msTime = 0;

        {
          Optimizer o;
          vector<OptimizerLineup> lineups = o.generateLineupN(
              p, playersToRemove, OptimizerLineup(), 0, msTime);
          // faster to parse vector<Players2> to allLineups
          allLineups.clear();
          for (auto &lineup : lineups) {
            int count = 0;
            lineup_t currentLineup;
            BitsetIter bitset(lineup.set);
            while (true) {
              int i = bitset.next();
              currentLineup[count] = ((uint8_t)i);
              if (!bitset.hasNext()) break;
              count++;
            }
            allLineups.push_back(currentLineup);
          }

          cout << "Done optimizer. inclusive time: " << msTime << endl;

          auto end = chrono::steady_clock::now();
          auto diff = end - start;
          double totalTime = chrono::duration<double, milli>(diff).count();
          cout << "Total elapsed time: " << totalTime << endl;
        }
      }
    }
  }

  cout << endl;

  auto end = chrono::steady_clock::now();
  auto diff = end - start;
  double msTime = chrono::duration<double, milli>(diff).count();

  ofstream myfile;
  myfile.open("outputset.csv");
  // output bestset
  myfile << msTime << "ms" << endl;
  myfile << bestset.ev;
  myfile << endl;
  for (auto &lineup : bestset.set) {
    for (auto &x : lineup) {
      myfile << p[x].name;
      myfile << ",";
    }
    myfile << endl;
  }

  myfile.close();
}
