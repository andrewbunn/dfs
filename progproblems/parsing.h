#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "Player.h"
#include <unordered_map>

using namespace std;

vector<string> getNextLineAndSplitIntoTokens(istream& str);

vector<string> parseNames(string filename);
vector<pair<string, string>> parseCorr(string filename);
vector<Player> parsePlayers(string filename);
vector<vector<string>> parseLineupString(const string filename);
vector<vector<string>> parseLineupSet(const string filename);
vector<vector<uint8_t>> parseLineups(string filename, unordered_map<string, uint8_t>& playerIndices);
void normalizeName(string& name);
vector<tuple<string, int, int>> parseCosts(string filename);
unordered_map<string, float> parseProjections(string filename);
vector<pair<string, float>> parseOwnership(string filename);
vector<string> parseRanks(string filename);