#include "Optimizer.h"
#include "ParsedConstants.h"
#include "ScopedElapsedTime.h"
#include "parsing.h"
#include <chrono>
#include <execution>
#include <immintrin.h>

using namespace std;
// searches pruned based on this threshold. this is intentionally accessed
// non-atomically.
static float _g_min_Players = 0.f;
// each thread has a pre allocated vector for each depth so we don't have cross
// thread malloc contention.
thread_local array<vector<OptimizerLineup>, NumLineupSlots> _depth_arrs{};

void runPlayerOptimizerN(string filein, string fileout, string lineupstart) {
  const vector<Player> p = parsePlayers(filein);
  const vector<string> startingPlayers = parseNames(lineupstart);

  OptimizerLineup startingLineup;
  int budgetUsed = 0;
  for (auto &pl : startingPlayers) {
    auto it = find_if(p.begin(), p.end(),
                      [&pl](const Player &p) { return pl == p.name; });
    if (it != p.end()) {
      // TODO do special calc for isflex
      if (startingLineup.tryAddPlayer(false, it->pos, it->proj, it->index)) {
        budgetUsed += it->cost;
      }
    }
    cout << pl << ",";
  }

  {
    ScopedElapsedTime optimizerTime;
    vector<string> empty;
    Optimizer o;
    vector<OptimizerLineup> lineups =
        o.generateLineupN(p, empty, startingLineup, budgetUsed);
    auto msTime = optimizerTime.getElapsedTime();
    saveLineupList(p, lineups, fileout, msTime);
  }
}

void Optimizer::knapsackPositionsN3(const int budget, const int pos,
                                    const OptimizerLineup oldLineup,
                                    const vector<vector<Player>> &players,
                                    const int rbStartPos, const int wrStartPos,
                                    const int teStartPos,
                                    bitset<NumLineupSlots> skipPositionSet) {
  _depth_arrs[pos].reserve(LINEUPCOUNT * 2);
  _depth_arrs[pos].clear();

  if (unlikely(skipPositionSet.any())) {
    if (knapsack_helperSkipPosition(budget, pos + 1, oldLineup, players,
                                    rbStartPos, wrStartPos, teStartPos,
                                    skipPositionSet)) {
      return;
    }
  }

  // dont process players we've already calculated in transpositions:
  int startPos;
  bool isWR = false;
  bool isRB = false;
  bool isTE = false;
  if (OptimizerLineup::isRBPos(pos)) {
    isRB = true;
    startPos = rbStartPos;
  } else if (OptimizerLineup::isWRPos(pos)) {
    isWR = true;
    startPos = wrStartPos;
  } else if (OptimizerLineup::isTEPos(pos)) {
    isTE = true;
    startPos = 0;
  } else {
    startPos = 0;
  }

  vector<OptimizerLineup> &bestLineups = _depth_arrs[pos];

  const vector<Player> *playersArray = &players[pos];
  if (OptimizerLineup::isFlexPos(pos)) {
    const int index = OptimizerLineup::getFlexTransposeIndex(
        rbStartPos, wrStartPos, teStartPos);
    auto it = _filteredFlex.find(index);
    if (it != _filteredFlex.end()) {
      playersArray = &it->second;
    }
  }

  if (OptimizerLineup::isLastPos(pos) &&
      (oldLineup.value + last_highest_delta < _g_min_Players)) {
    return;
  }

  for (int i = startPos; i < playersArray->size(); i++) {
    const Player &p = (*playersArray)[i];
    OptimizerLineup currentLineup = oldLineup;
    if (p.cost <= budget) {
      if (currentLineup.tryAddPlayer(OptimizerLineup::isFlexPos(pos), p.pos,
                                     p.proj, p.index)) {
        if (OptimizerLineup::isLastPos(pos)) {
          // this only stays sorted if defenses are sorted decreasing order
          bestLineups.push_back(currentLineup);
        } else {
          const int originalLen = bestLineups.size();
          knapsackPositionsN3(budget - p.cost, pos + 1, currentLineup, players,
                              isRB ? i + 1 : rbStartPos,
                              isWR ? i + 1 : wrStartPos,
                              isTE ? i + 1 : teStartPos, skipPositionSet);

          vector<OptimizerLineup> &lineups = _depth_arrs[pos + 1];

          const auto filter = [&](const float min) {
            for (const auto &l : lineups) {
              if (l.value > min) {
                bestLineups.push_back(l);
              }
            }
          };
          if (originalLen >= LINEUPCOUNT) {
            // g_min is intentionally not atomic
            _g_min_Players =
                std::max<float>(bestLineups.back().value, _g_min_Players);
            filter(_g_min_Players);
          } else {
            if (lineups.size() == 0)
              continue;

            const float min = _g_min_Players;
            if (min < lineups.back().value) {
              bestLineups.insert(bestLineups.end(), lineups.begin(),
                                 lineups.end());
            } else if (min < lineups.front().value) {
              filter(min);
            }
          }

          inplace_merge(bestLineups.begin(), bestLineups.begin() + originalLen,
                        bestLineups.end());
          if (bestLineups.size() > LINEUPCOUNT) {
            bestLineups.resize(LINEUPCOUNT);
          }
        }
      }
    }
  }
}

vector<OptimizerLineup> Optimizer::knapsackPositionsN(
    const int budget, const int pos, const OptimizerLineup oldLineup,
    const vector<vector<Player>> &players, const int rbStartPos,
    const int wrStartPos, const int teStartPos,
    bitset<NumLineupSlots> skipPositionSet) {
  _depth_arrs[pos].reserve(LINEUPCOUNT * 2);
  _depth_arrs[pos].clear();

  if (unlikely(skipPositionSet.any())) {
    if (skipPositionSet.test(pos)) {
      skipPositionSet.set(pos, false);

      if (pos >= 2) {
        knapsackPositionsN3(budget, pos + 1, oldLineup, players, rbStartPos,
                            wrStartPos, 0, skipPositionSet);
        return _depth_arrs[pos + 1];
      }
      return knapsackPositionsN(budget, pos + 1, oldLineup, players, rbStartPos,
                                wrStartPos, teStartPos, skipPositionSet);
    }
  }

  // dont process players we've already calculated in transpositions:
  int startPos;
  bool isWR = false;
  bool isRB = false;
  if (OptimizerLineup::isRBPos(pos)) {
    isRB = true;
    startPos = rbStartPos;
  } else if (OptimizerLineup::isWRPos(pos)) {
    isWR = true;
    startPos = wrStartPos;
  } else {
    startPos = 0;
  }

  auto loop = [&](const Player &p) {
    OptimizerLineup currentLineup = oldLineup;
    if (p.cost <= budget) {
      if (currentLineup.tryAddPlayer(false, p.pos, p.proj, p.index)) {
        if (pos >= 2) {
          knapsackPositionsN3(budget - p.cost, pos + 1, currentLineup, players,
                              isRB ? (&p - &players[pos][0]) + 1 : rbStartPos,
                              isWR ? (&p - &players[pos][0]) + 1 : wrStartPos,
                              0, skipPositionSet);
          return _depth_arrs[pos + 1];
        }
        return knapsackPositionsN(
            budget - p.cost, pos + 1, currentLineup, players,
            isRB ? (&p - &players[pos][0]) + 1 : rbStartPos,
            isWR ? (&p - &players[pos][0]) + 1 : wrStartPos, 0,
            skipPositionSet);
      }
    }

    return vector<OptimizerLineup>(1, currentLineup);
  };

  vector<vector<OptimizerLineup>> lineupResults(players[pos].size() - startPos);
  transform(execution::par_unseq, begin(players[pos]) + startPos,
            end(players[pos]), begin(lineupResults), loop);

  vector<OptimizerLineup> &merged = _depth_arrs[pos];
  merged.clear();
  for (const auto &lineup : lineupResults) {
    const int originalLen = merged.size();
    if (originalLen >= LINEUPCOUNT) {
      // g_min not protected, but we don't care
      _g_min_Players = std::max<float>(merged.back().value, _g_min_Players);
    }

    const float min = _g_min_Players;
    for (const auto &l : lineup) {
      if (l.value > min) {
        merged.push_back(l);
      }
    }

    inplace_merge(merged.begin(), merged.begin() + originalLen, merged.end());
    if (merged.size() > LINEUPCOUNT) {
      merged.resize(LINEUPCOUNT);
    }
  }

  return merged;
}

bitset<NumLineupSlots>
getSkipPositionsSet(const OptimizerLineup currentPlayers) {
  bitset<NumLineupSlots> skipPositionsSet;
  if (currentPlayers.getTotalCount() > 0) {
    for (int i = 0; i < numPositions; i++) {
      int count = currentPlayers.getPosCount(i);
      if (count > 0) {
        if (i == 0) {
          skipPositionsSet.set(i);
        } else if (i == 1) {
          skipPositionsSet.set(i);
          if (count > 1) {
            skipPositionsSet.set(2);
            if (count > 2) {
              // flex
              skipPositionsSet.set(7);
            }
          }
        } else if (i == 2) {
          skipPositionsSet.set(3);
          if (count > 1) {
            skipPositionsSet.set(4);
            if (count > 2) {
              skipPositionsSet.set(5);
              if (count > 3) {
                skipPositionsSet.set(7);
              }
            }
          }
        } else if (i == 3) {
          skipPositionsSet.set(6);
          // 2 te?
        } else if (i == 4) {
          skipPositionsSet.set(8);
        }
      }
    }
  }
  return skipPositionsSet;
}

vector<OptimizerLineup> Optimizer::generateLineupN(
    const vector<Player> &p, const vector<string> &disallowedPlayers,
    const OptimizerLineup currentPlayers, const int budgetUsed) {
  vector<vector<Player>> playersByPos(NumLineupSlots);
  for (int i = 0; i < NumLineupSlots; i++) {
    for (auto &pl : p) {
      auto it =
          find(disallowedPlayers.begin(), disallowedPlayers.end(), pl.name);
      if (it == disallowedPlayers.end()) {
        if (pl.pos == slots[i]) {
          playersByPos[i].push_back(pl);
        } else if (slots[i] == 5 &&
                   (pl.pos == 1 || pl.pos == 2 || pl.pos == 3)) {
          playersByPos[i].push_back(pl);
        }
      }
    }
  }
  _filteredFlex.clear();
  // for each starting rb and wr pos pair, create players table
  for (int i = 0; i <= playersByPos[1].size(); i++) {
    for (int j = 0; j <= playersByPos[3].size(); j++) {
      for (int k = 0; k <= playersByPos[6].size(); k++) {
        const int index = OptimizerLineup::getFlexTransposeIndex(i, j, k);
        vector<Player> flexPlayers;
        // add rbs from rb start pos i
        for (int z = i; z < playersByPos[1].size(); z++) {
          flexPlayers.push_back(playersByPos[1][z]);
        }
        // add wrs from wr start pos j
        for (int z = j; z < playersByPos[3].size(); z++) {
          flexPlayers.push_back(playersByPos[3][z]);
        }
        // add te players
        for (int z = k; z < playersByPos[6].size(); z++) {
          flexPlayers.push_back(playersByPos[6][z]);
        }
        _filteredFlex.emplace(index, flexPlayers);
      }
    }
  }

  bitset<NumLineupSlots> skipPositionsSet = getSkipPositionsSet(currentPlayers);

  _g_min_Players = 0.f;
  vector<OptimizerLineup> output =
      knapsackPositionsN(100 - budgetUsed, 0, currentPlayers, playersByPos, 0,
                         0, 0, skipPositionsSet);
  return output;
}

__attribute__((noinline)) bool Optimizer::knapsack_helperSkipPosition(
    const int budget, const int pos, const OptimizerLineup oldLineup,
    const vector<vector<Player>> &players, const int rbStartPos,
    const int wrStartPos, const int teStartPos,
    bitset<NumLineupSlots> skipPositionSet) {
  if (skipPositionSet.test(pos)) {
    skipPositionSet.set(pos, false);
    if (pos >= 8) {
      _depth_arrs[8].push_back(oldLineup);
      return true;
    }
    knapsackPositionsN3(budget, pos + 1, oldLineup, players, rbStartPos,
                        wrStartPos, teStartPos, skipPositionSet);
    copy(_depth_arrs[pos + 1].begin(), _depth_arrs[pos + 1].end(),
         _depth_arrs[pos].begin());
    return true;
  }
  return false;
}

bool operator==(const OptimizerLineup &first, const OptimizerLineup &other) {
  return (first.set == other.set);
}

bool operator!=(const OptimizerLineup &first, const OptimizerLineup &other) {
  return (first.set != other.set);
}

bool operator<(const OptimizerLineup &lhs, const OptimizerLineup &rhs) {
  return lhs.value > rhs.value;
}

bool operator==(const std::array<uint64_t, 2> &first,
                const std::array<uint64_t, 2> &other) {
  return (first[0] == other[0]) && (first[1] == other[1]);
}

bool operator<(const lineup_set &lhs, const lineup_set &rhs) {
  // backwards so highest value is "lowest" (best ranked lineup)
  if (lhs.ev != rhs.ev) {
    return lhs.ev > rhs.ev;
  }
  return lhs.stdev < rhs.stdev;
}
