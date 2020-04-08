#define _DISABLE_EXTENDED_ALIGNED_STORAGE 1
#include "Optimizer.h"
#include "Player.h"
#include "Selector.h"
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

using namespace std;

void parseBuildConstants() {
  const vector<Player> p = parsePlayers("players.csv");
  const size_t allPlayersSize = p.size();
  const size_t allPlayersSizeRounded = ((allPlayersSize + 7) / 8) * 8;

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
