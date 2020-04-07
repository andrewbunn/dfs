#define _DISABLE_EXTENDED_ALIGNED_STORAGE 1
#include "Optimizer.h"
#include "Player.h"
#include "Selector.h"
#include "Simulator.h"
#include "parsing.h"
#include "test.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <execution>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr uint64_t next_pow2(uint64_t x) {
  return (x == 1) ? 1 : 1 << (64 - __builtin_clzl(x - 1));
}

using namespace std;

void runPlayerOptimizerN(string filein, string fileout, string lineupstart) {
  vector<Player> p = parsePlayers(filein);
  vector<string> startingPlayers = parseNames(lineupstart);

  OptimizerLineup startingLineup;
  // if we can't add player, go to next round
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

    // output which set we're processing
    cout << pl << ",";
  }

  double msTime = 0;
  vector<string> empty;
  Optimizer o;
  vector<OptimizerLineup> lineups =
      o.generateLineupN(p, empty, startingLineup, budgetUsed, msTime);
  saveLineupList(p, lineups, fileout, msTime);
}

float mixPlayerProjections(Player &p, float numberfire, float fpros,
                           float yahoo) {
  if (p.pos == 0) {
    // for QB: probably use close to even average of yahoo, cbs, stats,
    // numberfire could ignore espn when dling data?
    return fpros;
  } else if (p.pos == 4) {
    return fpros;
    // return fpros * 2 - numberfire;
  } else {
    // for now just 70% yahoo, 30% fpros (fpros includes numberfire)
    // return yahoo * .3 + fpros * 1 + numberfire * -.3;
    return fpros * 2 - numberfire;
  }
}

void removeDominatedPlayersProjFile() {
  unordered_map<string, float> stds;

  for (int i = 0; i <= 4; i++) {
    ostringstream stream;
    stream << "std" << i;
    stream << ".csv";
    unordered_map<string, float> std = parseProjections(stream.str());
    for (auto &x : std) {
      stds.emplace(x.first, x.second);
    }
  }
  vector<tuple<string, int, int>> costs = parseCosts("costs.csv");
  // unordered_map<string, float> numfire = parseProjections("projs.csv");

  unordered_map<string, float> fpros = parseProsStats();

  vector<tuple<string, int, float, int, float>> playersResult;
  vector<float> sdevs;
  for (auto &p : fpros) {
    if (p.first == "cleveland") {
      continue;
    }

    {
      auto it =
          find_if(costs.begin(), costs.end(), [&](tuple<string, int, int> &x) {
            return get<0>(x) == p.first;
          });
      if (it != costs.end()) {
        int pos = get<1>(*it);
        float proj = p.second;

        float sdev = 0.f;
        auto itnfstd = stds.find(p.first);
        if (itnfstd != stds.end()) {
          sdev = itnfstd->second;
        }
        playersResult.emplace_back(p.first, pos, proj, get<2>(*it), sdev);
        // sdevs.push_back(sdev);
      }
    }
  }

  ofstream myfile;
  myfile.open("players.csv");
  // for a slot, if there is a player cheaper cost but > epsilon score, ignore
  // no def for now?
  for (int i = 0; i <= 4; i++) {
    vector<tuple<string, int, float, int, float>> positionPlayers;
    copy_if(playersResult.begin(), playersResult.end(),
            back_inserter(positionPlayers),
            [i](tuple<string, int, float, int, float> &p) {
              return (get<3>(p) == i);
            });

    // sort by value, descending
    sort(positionPlayers.begin(), positionPlayers.end(),
         [](tuple<string, int, float, int, float> &a,
            tuple<string, int, float, int, float> &b) {
           return get<2>(a) > get<2>(b);
         });

    // biggest issue is for rb/wr we dont account for how many we can use.
    for (int j = 0; j < positionPlayers.size(); j++) {
      // remove all players with cost > player
      // auto& p = positionPlayers[j];
      auto it = remove_if(
          positionPlayers.begin() + j + 1, positionPlayers.end(),
          [&](tuple<string, int, float, int, float> &a) {
            // PositionCount
            static float minValue[5] = {8, 8, 8, 5, 5};
            return get<2>(a) < minValue[i] ||
                   (count_if(positionPlayers.begin(),
                             positionPlayers.begin() + j + 1,
                             [&](tuple<string, int, float, int, float> &p) {
                               static float epsilon = 1;

                               // probably want minvalue by pos 8,8,8,5,5?
                               // probably want a bit more aggression here, if
                               // equal cost but ones player dominates the other
                               // cost > current player && value < current
                               // player
                               int costDiff = (get<1>(p) - get<1>(a));
                               float valueDiff = get<2>(p) - get<2>(a);
                               bool lessValuable = (valueDiff > epsilon);
                               bool atLeastAsExpensive = costDiff <= 0;
                               return (atLeastAsExpensive && lessValuable) ||
                                      costDiff <= -3;
                             }) >= PositionCount[i]);
          });

      positionPlayers.resize(distance(positionPlayers.begin(), it));
    }

    for (auto &p : positionPlayers) {
      myfile << get<0>(p);
      myfile << ",";
      myfile << get<1>(p);
      myfile << ",";
      myfile << get<2>(p);
      myfile << ",";
      myfile << get<3>(p);
      myfile << ",";
      myfile << get<4>(p);
      myfile << ",";

      myfile << endl;
    }
  }
  myfile.close();
}

void removeDominatedPlayers(string filein, string fileout) {
  vector<Player> players = parsePlayers(filein);
  unordered_map<string, float> yahoo = parseYahooStats();
  unordered_map<string, float> fpros = parseProsStats();
  cout << "Mixing projections" << endl;
  for (auto &p : players) {
    auto itpros = fpros.find(p.name);
    // auto ity = yahoo.find(p.name);

    // linear reg models here:

    // players we dont want to include can just have large negative diff
    if (itpros != fpros.end() /*&& ity != yahoo.end() || (itpros != fpros.end() && (p.pos != 4 && p.pos != 0))*/)
        {
      if (p.name == "david johnson" && p.pos == 3) {
        p.proj = 0.f;
      } else {
        p.proj = mixPlayerProjections(p, p.proj, itpros->second,
                                      /*ity != yahoo.end() ? ity->second :*/ 0);
      }
    } else {
      // gillislee
      cout << p.name << endl;
      if (p.pos != 4) {
        // ignore player if it wasnt high on yahoo/pros?
        p.proj = 0.f;
      }
    }
  }

  ofstream myfile;
  myfile.open(fileout);

  ofstream allPlayers;
  allPlayers.open("allplayers.csv");
  // for a slot, if there is a player cheaper cost but > epsilon score, ignore
  // no def for now?
  for (int i = 0; i <= 4; i++) {
    vector<Player> positionPlayers;
    copy_if(players.begin(), players.end(), back_inserter(positionPlayers),
            [i](Player &p) { return (p.pos == i); });

    // sort by value, descending
    sort(positionPlayers.begin(), positionPlayers.end(),
         [](Player &a, Player &b) { return a.proj > b.proj; });

    for (auto &p : positionPlayers) {
      allPlayers << p.name;
      allPlayers << ",";
      allPlayers << (static_cast<int>(p.cost) + 10 + ((p.pos == 0) ? 10 : 0));
      allPlayers << ",";
      allPlayers << static_cast<float>(p.proj);
      allPlayers << ",";
      allPlayers << static_cast<int>(p.pos);
      allPlayers << ",";
      allPlayers << static_cast<float>(p.stdDev);
      allPlayers << ",";
      allPlayers << endl;
    }

    // biggest issue is for rb/wr we dont account for how many we can use.
    for (int j = 0; j < positionPlayers.size(); j++) {
      // remove all players with cost > player
      // auto& p = positionPlayers[j];
      auto it = remove_if(
          positionPlayers.begin() + j + 1, positionPlayers.end(),
          [&](Player &a) {
            // PositionCount
            static float minValue[5] = {8, 7, 7, 5, 5};
            return a.proj < minValue[i] ||
                   (count_if(positionPlayers.begin(),
                             positionPlayers.begin() + j + 1, [&](Player &p) {
                               static float epsilon = 1;

                               // probably want minvalue by pos 8,8,8,5,5?
                               // probably want a bit more aggression here, if
                               // equal cost but ones player dominates the other
                               // cost > current player && value < current
                               // player
                               int costDiff = (p.cost - a.cost);
                               float valueDiff = p.proj - a.proj;
                               bool lessValuable = (valueDiff > epsilon);
                               bool atLeastAsExpensive = costDiff <= 0;
                               return (atLeastAsExpensive && lessValuable) ||
                                      costDiff <= -3;
                             }) >= PositionCount[i]);
          });

      positionPlayers.resize(distance(positionPlayers.begin(), it));
    }

    for (auto &p : positionPlayers) {
      myfile << p.name;
      myfile << ",";
      myfile << (static_cast<int>(p.cost) + 10 + ((p.pos == 0) ? 10 : 0));
      myfile << ",";
      myfile << static_cast<float>(p.proj);
      myfile << ",";
      myfile << static_cast<int>(p.pos);
      myfile << ",";
      myfile << static_cast<float>(p.stdDev);
      myfile << ",";

      myfile << endl;
    }
  }
  allPlayers.close();
  myfile.close();
}

void splitLineups(const string lineups) {
  vector<vector<string>> allLineups;
  allLineups = parseLineupString(lineups);
  vector<vector<string>> originalOrder = allLineups;
  unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
  default_random_engine generator(seed1);
  shuffle(allLineups.begin(), allLineups.end(), generator);

  vector<vector<string>> setA;
  vector<vector<string>> setB;

  int i = 0;
  partition_copy(allLineups.begin(), allLineups.end(), back_inserter(setA),
                 back_inserter(setB), [&i](vector<string> &l) {
                   bool setA = i++ < 7;
                   return setA;
                 });
  sort(setA.begin(), setA.end(),
       [&originalOrder](vector<string> &la, vector<string> &lb) {
         return find(originalOrder.begin(), originalOrder.end(), la) <
                find(originalOrder.begin(), originalOrder.end(), lb);
       });
  sort(setB.begin(), setB.end(),
       [&originalOrder](vector<string> &la, vector<string> &lb) {
         return find(originalOrder.begin(), originalOrder.end(), la) <
                find(originalOrder.begin(), originalOrder.end(), lb);
       });

  ofstream myfile;
  myfile.open("outputsetSplit.csv");
  for (auto &lineup : setA) {
    for (auto &x : lineup) {
      myfile << x;
      myfile << ",";
    }
    myfile << endl;
  }
  myfile << endl;
  myfile << endl;

  for (auto &lineup : setB) {
    for (auto &x : lineup) {
      myfile << x;
      myfile << ",";
    }
    myfile << endl;
  }

  myfile.close();
}

void evaluateScore(string filename) {
  vector<vector<string>> allLineups;
  allLineups = parseLineupString("outputset.csv");
  // vector<vector<string>> allLineups;
  // allLineups = parseLineupSet("outputsetsharpe-final.csv");
  // allLineups = parseLineupSet(filename);
  unordered_map<string, float> results = parseProjections("playerResults.csv");
  vector<float> scores;
  for (auto &lineup : allLineups) {
    float score = 0.f;
    for (auto &name : lineup) {
      auto it = results.find(name);
      if (it != results.end()) {
        score += it->second;
      }
    }
    scores.push_back(score);
  }

  {
    ofstream myfile;
    myfile.open("outputset-scores.csv");
    int i = 0;
    for (auto &lineup : allLineups) {
      for (auto &name : lineup) {
        myfile << name;
        myfile << ",";
      }
      myfile << scores[i++];
      myfile << endl;
    }
    myfile << endl;

    myfile.close();
  }
}

void parseBuildConstants() {
  const vector<Player> p = parsePlayers("players.csv");
  const size_t allPlayersSize = p.size();
  const size_t allPlayersSizeRounded = ((allPlayersSize + 4) / 8) * 8;

  auto firstDef = find_if(p.begin(), p.end(),
                          [](auto &pl) { return pl.pos == Position::def; });
  const float firstDefenseProj = firstDef->proj + .05f;

  cout << "Updating players size: " << allPlayersSizeRounded << endl;
  ofstream myfile;
  myfile.open("ParsedConstants.h");
  myfile << "#pragma once" << endl;
  myfile << "constexpr size_t all_players_size = " << allPlayersSizeRounded
         << ";" << endl;
  myfile << "constexpr float last_highest_delta = " << firstDefenseProj << ";"
         << endl;
  myfile.close();
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    if (strcmp(argv[1], "optimizen") == 0) {
      string filein, fileout, lineupstart;
      if (argc > 2) {
        filein = argv[2];
      } else {
        filein = "players.csv";
      }

      if (argc > 3) {
        fileout = argv[3];
      } else {
        fileout = "output.csv";
      }

      if (argc > 4) {
        lineupstart = argv[4];
      } else {
        lineupstart = "";
      }
      runPlayerOptimizerN(filein, fileout, lineupstart);
    }

    if (strcmp(argv[1], "import") == 0) {
      string fileout;
      if (argc > 2) {
        fileout = argv[2];
      } else {
        fileout = "players.csv";
      }
      removeDominatedPlayersProjFile();
    }

    if (strcmp(argv[1], "dominateplayers") == 0) {
      string filein, fileout;
      if (argc > 2) {
        filein = argv[2];
      } else {
        filein = "mergedPlayers.csv";
      }

      if (argc > 3) {
        fileout = argv[3];
      } else {
        fileout = "players.csv";
      }
      removeDominatedPlayers(filein, fileout);
    }

    if (strcmp(argv[1], "splituplineups") == 0) {
      splitLineups("outputset.csv");
    }

    if (strcmp(argv[1], "eval") == 0) {
      string playersFile;
      if (argc > 2) {
        playersFile = argv[2];
      } else {
        playersFile = "outputsetsharpe.csv";
      }
      evaluateScore(playersFile);
    }

    if (strcmp(argv[1], "greedyselect") == 0) {
      greedyLineupSelector();
    }

    if (strcmp(argv[1], "parsehistproj") == 0) {
      parseHistoricalProjFiles();
    }

    if (strcmp(argv[1], "parsefpros") == 0) {
      parseProsStats();
    }

    if (strcmp(argv[1], "parseconstants") == 0) {
      parseBuildConstants();
    }
  }
  return 0;
}
