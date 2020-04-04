#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "Player.h"
#include "Players.h"
#include <unordered_map>

using namespace std;
struct Players2;

vector<string> getNextLineAndSplitIntoTokens(istream& str);

vector<string> parseNames(string filename);
vector<tuple<string, string, float>> parseCorr(string filename);
vector<Player> parsePlayers(string filename);
vector<vector<string>> parseLineupString(const string filename);
vector<vector<string>> parseLineupSet(const string filename);
vector<lineup_t> parseLineups(string filename, const unordered_map<string, uint8_t>& playerIndices);
vector<Players2> parseLineupsData(string filename);
void writeLineupsData(string filename, vector<Players2>& lineups);
void normalizeName(string& name);
vector<tuple<string, int, int>> parseCosts(string filename);
unordered_map<string, float> parseProjections(string filename);
vector<pair<string, float>> parseOwnership(string filename);
vector<string> parseRanks(string filename);
void parseHistoricalProjFiles();
unordered_map<string, float> parseProsStats();
unordered_map<string, float> parseYahooStats();