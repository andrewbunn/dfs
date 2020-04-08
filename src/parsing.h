#pragma once
#include "Optimizer.h"
#include "Player.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<std::string> getNextLineAndSplitIntoTokens(std::istream &str);

std::vector<std::string> parseNames(std::string filename);
std::vector<std::tuple<std::string, std::string, float>>
parseCorr(std::string filename);
std::vector<Player> parsePlayers(std::string filename);
std::vector<std::vector<std::string>>
parseLineupString(const std::string filename);
std::vector<std::vector<std::string>>
parseLineupSet(const std::string filename);
std::vector<lineup_t>
parseLineups(std::string filename,
             const std::unordered_map<std::string, uint8_t> &playerIndices);
std::vector<OptimizerLineup> parseLineupsData(std::string filename);
void writeLineupsData(std::string filename,
                      std::vector<OptimizerLineup> &lineups);
void normalizeName(std::string &name);
std::vector<std::tuple<std::string, int, int>> parseCosts(std::string filename);
std::unordered_map<std::string, float> parseProjections(std::string filename);
std::vector<std::pair<std::string, float>> parseOwnership(std::string filename);
std::vector<std::string> parseRanks(std::string filename);
void parseHistoricalProjFiles();
std::unordered_map<std::string, float> parseProsStats();
std::unordered_map<std::string, float> parseYahooStats();
void saveLineupList(std::vector<Player> &p,
                    std::vector<OptimizerLineup> &lineups, std::string fileout,
                    double msTime);
float mixPlayerProjections(Player &p, float numberfire, float fpros,
                           float yahoo);
void removeDominatedPlayersProjFile();
void removeDominatedPlayers(std::string filein, std::string fileout);
void splitLineups(const std::string lineups);
void evaluateScore(std::string filename);
