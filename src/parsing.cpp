#include "parsing.h"
#include "Optimizer.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <random>

using namespace std;

vector<string> getNextLineAndSplitIntoTokens(istream &str) {
  vector<string> result;
  string line;
  getline(str, line);

  stringstream lineStream(line);
  string cell;

  while (getline(lineStream, cell, ',')) {
    result.push_back(cell);
  }
  return result;
}

vector<string> parseNames(string filename) {
  vector<string> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  while (tokens.size() == 1) {
    result.emplace_back(tokens[0]);
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

vector<tuple<string, string, float>> parseCorr(string filename) {
  vector<tuple<string, string, float>> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  while (tokens.size() == 3) {
    result.emplace_back(tokens[0], tokens[1], stof(tokens[2]));
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

vector<Player> parsePlayers(string filename) {
  vector<Player> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  int count = 0;
  while (tokens.size() >= 5) {
    int c = atoi(tokens[1].c_str()) - 10;
    float p = (float)atof(tokens[2].c_str());
    int pos = atoi(tokens[3].c_str());
    float sd = (float)atof(tokens[4].c_str());
    if (pos == 0) {
      c -= 10;
    }
    if (tokens.size() == 5) {
      result.emplace_back(tokens[0], c, p, pos, count++, sd);
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

vector<vector<string>> parseLineupString(const string filename) {
  vector<vector<string>> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);

  while (tokens.size() >= 1) {
    if (tokens[0].size() > 0) {
      if (isalpha(tokens[0][0])) {
        result.push_back(tokens);
      } else {
      }
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

vector<vector<string>> parseLineupSet(const string filename) {
  vector<vector<string>> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  vector<string> current;

  while (tokens.size() >= 1) {
    if (tokens[0].size() > 0) {
      if (isalpha(tokens[0][0])) {
        result.push_back(tokens);
        current.clear();
      }
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

// for now just parse player names like output.csv current does, could make
// format easier to parse
vector<lineup_t>
parseLineups(string filename,
             const unordered_map<string, uint8_t> &playerIndices) {
  vector<lineup_t> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  lineup_t current;

  int c = 0;
  while (tokens.size() == 1) {
    if (tokens[0].size() > 0) {
      if (isalpha(tokens[0][0])) {
        auto it = playerIndices.find(tokens[0]);
        if (it != playerIndices.end()) {
          // current.push_back(it->second);
          current[c++] = it->second;
        } else {
          // throw invalid_argument("Invalid player: " + tokens[0]);
          cout << "Invalid player: " << tokens[0] << endl;
        }
      } else {
        // value/cost number, signifies end of lineup.
        // duration at start of file
        if (c > 0) {
          result.push_back(current);
          c = 0;
        }
      }
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

void writeLineupsData(string filename, vector<OptimizerLineup> &lineups) {
  ofstream myfile;
  myfile.open(filename);
  for (auto &lineup : lineups) {
    uint64_t set1 = (uint64_t)lineup.bits;
    myfile << set1;
    myfile << ",";
    uint64_t set2 = 0;
    for (int i = 64; i < 128; ++i) {
      if (lineup.set[i]) {
        set2 |= 1 << (i - 64);
      }
    }

    myfile << set2;
    myfile << ",";
    // for (auto x : lineup.posCounts)
    for (int i = 0; i < numPositions; i++) {
      myfile << (int)lineup.getPosCount(i);
      myfile << ",";
    }
    myfile << (int)lineup.getTotalCount();
    myfile << ",";
    myfile << lineup.value;
    myfile << ",";
    myfile << lineup.hasFlex;
    myfile << '\n';
  }
  myfile << flush;
  myfile.close();
}

vector<OptimizerLineup> parseLineupsData(string filename) {
  vector<OptimizerLineup> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  vector<uint8_t> current;

  // bitset1, bitset2, posCounts, totalCount, value, hasFlex
  while (tokens.size() >= 10) {
    int idx = 0;
    OptimizerLineup lineup;
    // lineup.bitset1 = stoull(tokens[idx++]);
    // lineup.bitset2 = stoull(tokens[idx++]);
    uint64_t set1 = stoull(tokens[idx++]);
    uint64_t set2 = stoull(tokens[idx++]);
    unsigned __int128 b = set2;
    b <<= 64;
    b |= set1;
    lineup.bits = b;
    for (int x = 0; x < numPositions; x++) {
      lineup.setPosCount(x, stoi(tokens[idx++]));
      // lineup.posCounts[x] = stoi(tokens[idx++]);
    }
    // lineup.totalCount = stoi(tokens[idx++]);
    idx++;
    lineup.value = stof(tokens[idx++]);
    lineup.hasFlex = stoi(tokens[idx++]) != 0;
    result.push_back(lineup);
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

void normalizeName(string &name) {
  // normalize to just 'a-z' + space
  // what about jr/sr?
  transform(name.begin(), name.end(), name.begin(), ::tolower);
  // return name;
  auto it = copy_if(name.begin(), name.end(), name.begin(),
                    [](char &c) { return (islower(c) != 0) || c == ' '; });
  name.resize(distance(name.begin(), it));

  // sr/jr
  static string sr = " sr";
  static string jr = " jr";
  static string ii = " ii";
  // will fuller v but dont want all v names
  // static string v = " v";

  it = find_end(name.begin(), name.end(), sr.begin(), sr.end());
  name.resize(distance(name.begin(), it));

  it = find_end(name.begin(), name.end(), jr.begin(), jr.end());
  name.resize(distance(name.begin(), it));

  it = find_end(name.begin(), name.end(), ii.begin(), ii.end());
  name.resize(distance(name.begin(), it));

  static array<string, 32> dsts = {"Baltimore",
                                   "Houston",
                                   "Arizona",
                                   "Denver",
                                   "Carolina",
                                   "Kansas City",
                                   "Chicago",
                                   "Jacksonville",
                                   "New England",
                                   "New Orleans",
                                   "Tampa Bay",
                                   "Los Angeles Rams",
                                   "Los Angeles Chargers",
                                   "Philadelphia",
                                   "New York Jets",
                                   "Dallas",
                                   "Cincinnati",
                                   "Cleveland",
                                   "Minnesota",
                                   "Washington",
                                   "San Francisco",
                                   "Atlanta",
                                   "New York Giants",
                                   "Miami",
                                   "Green Bay",
                                   "Seattle",
                                   "Tennessee",
                                   "Pittsburgh",
                                   "Oakland",
                                   "Detroit",
                                   "Indianapolis",
                                   "Buffalo"};

  for_each(dsts.begin(), dsts.end(), [](string &n) {
    transform(n.begin(), n.end(), n.begin(), ::tolower);
  });

  auto i = find_if(dsts.begin(), dsts.end(),
                   [&name](string &n) { return name.find(n) != string::npos; });
  if (i != dsts.end()) {
    if (*i == "washington") {
      if (name[0] == 'd' || name[0] == 't') {
        // rb dwayne/deandre
      } else {
        name = *i;
      }
    } else {
      name = *i;
    }
  }

  if (name == "robert kelley") {
    name = "rob kelley";
  } else if (name == "will fuller v") {
    name = "will fuller";
  }
}

vector<tuple<string, int, int>> parseCosts(string filename) {
  vector<tuple<string, int, int>> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  while (tokens.size() == 3) {
    int c = stoi(tokens[1]);
    int pos = stoi(tokens[2]);
    normalizeName(tokens[0]);
    // dont add david johnson TE
    if (tokens[0] != "david johnson" || pos == 1) {
      result.emplace_back(tokens[0], c, pos);
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

unordered_map<string, float> parseProjections(string filename) {
  unordered_map<string, float> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  // two david johnsons, just only add first value for now, should be fine
  while (tokens.size() >= 2) {
    float proj;
    if (tokens.size() == 2) {
      proj = stof(tokens[1]);
    } else {
      proj = stof(tokens[2]);
    }
    normalizeName(tokens[0]);
    if (result.find(tokens[0]) != result.end()) {
      cout << tokens[0] << endl;
    } else {
      result.emplace(tokens[0], proj);
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }
  cout << endl;
  return result;
}

vector<pair<string, float>> parseOwnership(string filename) {
  vector<pair<string, float>> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  // two david johnsons, just only add first value for now, should be fine
  while (tokens.size() >= 2) {
    float proj = stof(tokens[tokens.size() - 1]);
    normalizeName(tokens[0]);
    result.emplace_back(tokens[0], proj);
    tokens = getNextLineAndSplitIntoTokens(file);
  }
  return result;
}

vector<string> parseRanks(string filename) {
  vector<string> result;
  ifstream file(filename);
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  while (tokens.size() >= 1) {
    normalizeName(tokens[0]);

    result.emplace_back(tokens[0]);
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return result;
}

#define POSFILTER(pos) ((pos) == 0)
unordered_map<string, float> parseFantasyResults() {
  ifstream file("playersHistorical.csv");
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);
  // skip first line
  tokens = getNextLineAndSplitIntoTokens(file);
  unordered_map<string, float> results;
  while (tokens.size() == 5) {
    normalizeName(tokens[0]);
    int pos = stoi(tokens[1]);

    if (POSFILTER(pos)) {
      int wk = stoi(tokens[3]);
      int yr = stoi(tokens[2]);
      ostringstream id;
      id << tokens[0] << "," << pos << "," << setfill('0') << setw(2) << wk
         << "," << yr;
      results.emplace(id.str(), stof(tokens[4]));
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  return results;
}

void parseYahooProjFiles(unordered_map<string, float> &performanceProjections) {
  string location = "..\\ffaprojs\\";
  // basically want, player, week, projection
  array<int, 4> weeks = {2, 6, 8, 10};
  // pos data?
  // basically we want a "performance id" = (player, week, year) -> proj
  for (int i : weeks) {
    ostringstream stream;
    stream << location << "yahoo-wk" << setfill('0') << setw(2) << i;
    stream << ".csv";
    ifstream file(stream.str());
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    tokens = getNextLineAndSplitIntoTokens(file); // skip first line
    while (tokens.size() > 8) {
      normalizeName(tokens[2]);
      int pos = 0;
      if (tokens[5] == "\"QB\"") {
        pos = 0;
      } else if (tokens[5] == "\"RB\"") {
        pos = 1;
      } else if (tokens[5] == "\"WR\"") {
        pos = 2;
      } else if (tokens[5] == "\"TE\"") {
        pos = 3;
      }
      if (POSFILTER(pos)) {
        ostringstream id;
        id << tokens[2] << "," << pos << "," << setfill('0') << setw(2) << i
           << ","
           << "2016";
        auto it = copy_if(tokens[8].begin(), tokens[8].end(), tokens[8].begin(),
                          [](char c) { return c != '\"'; });
        tokens[8].resize(distance(tokens[8].begin(), it));
        performanceProjections.emplace(id.str(), stof(tokens[8]));
      }
      tokens = getNextLineAndSplitIntoTokens(file);
    }
  }

  cout << performanceProjections.size() << endl;
}

void parseNFLProjFiles(unordered_map<string, float> &performanceProjections) {
  string location = "..\\ffaprojs\\";
  // basically want, player, week, projection
  array<int, 1> weeks = {10};
  // pos data?
  // basically we want a "performance id" = (player, week, year) -> proj
  for (int i : weeks) {
    ostringstream stream;
    stream << location << "nfl-wk" << setfill('0') << setw(2) << i;
    stream << ".csv";
    ifstream file(stream.str());
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    tokens = getNextLineAndSplitIntoTokens(file); // skip first line
    while (tokens.size() > 8) {
      normalizeName(tokens[2]);
      int pos = 0;
      if (tokens[5] == "\"QB\"") {
        pos = 0;
      } else if (tokens[5] == "\"RB\"") {
        pos = 1;
      } else if (tokens[5] == "\"WR\"") {
        pos = 2;
      } else if (tokens[5] == "\"TE\"") {
        pos = 3;
      }

      if (POSFILTER(pos)) {
        ostringstream id;
        id << tokens[2] << "," << pos << "," << setfill('0') << setw(2) << i
           << ","
           << "2016";
        auto it = copy_if(tokens[8].begin(), tokens[8].end(), tokens[8].begin(),
                          [](char c) { return c != '\"'; });
        tokens[8].resize(distance(tokens[8].begin(), it));
        performanceProjections.emplace(id.str(), stof(tokens[8]));
      }
      tokens = getNextLineAndSplitIntoTokens(file);
    }
  }

  cout << performanceProjections.size() << endl;
}

void parseNumberFireProjFiles(
    unordered_map<string, float> &performanceProjections) {
  string location = "..\\numberfireprojs\\";
  // basically want, player, week, projection
  const int weeks = 13;
  // pos data?
  // basically we want a "performance id" = (player, week, year) -> proj
  for (int i = 1; i <= weeks; i++) {
    ostringstream stream;
    stream << location << "w" << setfill('0') << setw(2) << i;
    stream << ".csv";
    ifstream file(stream.str());
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    while (tokens.size() == 3) {
      normalizeName(tokens[0]);
      int pos = stoi(tokens[1]);

      if (POSFILTER(pos)) {
        ostringstream id;
        id << tokens[0] << "," << pos << "," << setfill('0') << setw(2) << i
           << ","
           << "2016";
        performanceProjections.emplace(id.str(), stof(tokens[2]));
      }
      tokens = getNextLineAndSplitIntoTokens(file);
    }
  }

  cout << performanceProjections.size() << endl;
}

int getSourceId(array<string, 8> &sources, string source) {
  auto i = find_if(sources.begin(), sources.end(), [&source](string &n) {
    return source.find(n) != string::npos;
  });

  if (i == sources.end()) {
    // cout << source;
    return -1;
  } else {
    return (int)distance(sources.begin(), i);
  }
}

void parseHistoricalProjFiles() {
  // fantasypros = .25 * yahoo + .25 * espn + .25 * cbs + .25 * STATS
  // just input STATS into linear model
  static array<string, 8> sources = {
      "espn",
      //"fftoday",
      "yahoo", "numberfire",
      //"nfl",
      //"sharks",
      "cbs", "fantasypros",
      //"nerd"
  };
  ifstream file("playerProjections.csv");
  vector<string> tokens = getNextLineAndSplitIntoTokens(file);

  vector<unordered_map<string, float>> histProjections(sources.size());
  parseYahooProjFiles(histProjections[1]);
  parseNFLProjFiles(histProjections[3]);
  while (tokens.size() == 6) {
    normalizeName(tokens[0]);
    int pos = stoi(tokens[1]);
    int wk = stoi(tokens[3]);
    // dont have real data for wk 14 yet
    // skip TE for now
    if (wk < 14 && POSFILTER(pos)) {
      int yr = stoi(tokens[2]);
      ostringstream id;
      id << tokens[0] << "," << pos << "," << setfill('0') << setw(2) << wk
         << "," << yr;
      int source = getSourceId(sources, tokens[5]);
      if (source >= 0) {
        // may also want to "prune" projections here
        histProjections[source].emplace(id.str(), stof(tokens[4]));
      }
    }
    tokens = getNextLineAndSplitIntoTokens(file);
  }

  // nerd size so small, probably unusable
  int numberfireTable = 2;
  // int ESPNTable = 0;
  // int yahooTable = 1;
  // int cbsTable = 3;
  parseNumberFireProjFiles(histProjections[numberfireTable]);
  cout << histProjections.size() << endl;

  unordered_map<string, float> results = parseFantasyResults();

  // yahoo missing week 8 2016?
  // create tables for those who have entry in each source
  int startingTable = 4; // 7; // fpros has lower entries
  int datasetSize =
      startingTable + 1 + 1; // tables including startingtable and results
  unordered_map<string, float> &prosTable = histProjections[startingTable];
  vector<pair<string, vector<float>>> dataTable;
  for (auto &entry : prosTable) {
    vector<float> allProjs;
    for (int i = 0; i < startingTable; i++) {
      auto it = histProjections[i].find(entry.first);
      if (it == histProjections[i].end()) {
        // can't use this data point
        if (entry.second >= 5) {
          cout << entry.first << ": " << sources[i] << endl;
        }
        break;
      } else {
        allProjs.push_back(it->second);
      }
    }

    if (allProjs.size() == datasetSize - 2) {
      // apparently fpros is actually: cbs, espn, numberfire, stats, fftoday
      // need fftoday to split out separate
      float STATSval = entry.second;
      // TEMP:

      allProjs.push_back(STATSval);
      float avg = accumulate(allProjs.begin(), allProjs.end(), 0.f) /
                  (float)allProjs.size();
      auto it = results.find(entry.first);
      if (it != results.end() && avg >= 4.5f) {
        allProjs.push_back(it->second);
        // Normalize data for svr:

        dataTable.emplace_back(entry.first, allProjs);
      } else {
        cout << entry.first << ": no fantasy result data" << endl;
      }
    }
  }

  cout << dataTable.size() << endl;

  // output user friendly file and data training file? also we want to split by
  // position

  {
    // for now since we dont have enough data, just output projection data
    // format:  espn, yahoo, numberfire, nfl, sharks, cbs, pros, real
    ofstream myfile;
    myfile.open("dataset.csv");

    ofstream names;
    names.open("dataset-index.csv");

    for (auto &i : dataTable) {
      names << i.first << '\n';
      bool first = true;
      for (auto val : i.second) {
        if (!first) {
          myfile << ",";
        } else {
          first = false;
        }
        myfile << val;
      }
      myfile << '\n';
    }

    names.close();
    myfile.close();
  }
}

// general model for now: 70% yahoo, 10% espn, 10% cbs, 10% stats
// basically 60% yahoo, 40% fpros
// for QB: probably use close to even average of yahoo, cbs, stats, numberfire
// def probably the same for now?

unordered_map<string, float> parseProsStats() {
  unordered_map<string, float> results;
  for (int i = 0; i <= 4; i++) {
    ostringstream stream;
    stream << "fpproj-" << i;
    stream << ".csv";
    ifstream file(stream.str());
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    while (tokens.size() >= 6) {
      normalizeName(tokens[0]);
      int pos = i;
      ostringstream id;
      float proj = stof(tokens[tokens.size() - 1]);
      float rec = 0.f;
      if (i == 0) {
      } else if (i == 1 || i == 2) {
        rec = stof(tokens[4]);
      } else if (i == 3) {
        rec = stof(tokens[1]);
      }

      proj += rec * .5f;

      if (tokens[0] != "david johnson" || pos == 1) {
        if (tokens[0] != "cleveland" || pos == 4) {
          if (results.find(tokens[0]) != results.end()) {
            cout << tokens[0] << endl;
            if (tokens[0] != "ty montgomery" && proj > 5 &&
                tokens[0] != "daniel brown" && tokens[0] != "neal sterling") {
              // throw invalid_argument("Invalid player: " + tokens[0]);
              cout << "Invalid player: " << tokens[0] << endl;
            }
          } else {
            results.emplace(tokens[0], proj);
          }
        }
      }
      tokens = getNextLineAndSplitIntoTokens(file);
    }
  }

  // cout << results.size() << endl;
  return results;
}

unordered_map<string, float> parseYahooStats() {
  unordered_map<string, float> results;
  for (int i = 0; i <= 4; i++) {
    ostringstream stream;
    stream << "yahooproj-" << i;
    stream << ".csv";
    ifstream file(stream.str());
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    while (tokens.size() >= 10) {
      normalizeName(tokens[0]);
      int pos = i;
      ostringstream id;
      float proj = stof(tokens[1]);
      float rec = pos < 4 ? stof(tokens[9]) : 0.f;
      proj += rec * .5f;
      /*
      float payds = stof(tokens[1]);
      float patds = stof(tokens[2]);
      float ints = stof(tokens[3]);
      float ruyds = stof(tokens[5]);
      float rutds = stof(tokens[6]);
      float rec = stof(tokens[8]);
      float reyds = stof(tokens[9]);
      float retds = stof(tokens[10]);
      float rettds = stof(tokens[11]);
      float tpconv = stof(tokens[12]);
      float fls = stof(tokens[13]);
      float proj = payds * .04 + patds * 4 + ints * -1 + ruyds * .1 + rutds * 6
      + fls * -2 + rec * .5 + reyds * .1 + retds * 6 + rettds *6 + tpconv * 2;*/
      // may also want to "prune" projections here

      if (tokens[0] != "david johnson" || pos == 1) {
        if (results.find(tokens[0]) != results.end()) {
          cout << tokens[0] << endl;
          ;
          // if (tokens[0] != "ty montgomery" && tokens[0] != "daniel brown" &&
          // tokens[0] != "neal sterling")
          if (tokens[0] != "ty montgomery" && proj > 5 &&
              results.find(tokens[0])->second != proj) {
            // throw invalid_argument("Invalid player: " + tokens[0]);
            cout << "Invalid player: " << tokens[0] << endl;
          }
        } else {
          results.emplace(tokens[0], proj);
        }
      }
      tokens = getNextLineAndSplitIntoTokens(file);
    }
  }

  // cout << results.size() << endl;
  return results;
}

void saveLineupList(const vector<Player> &p,
                    const vector<OptimizerLineup> &lineups,
                    const string fileout, const double msTime) {
  ofstream myfile;
  myfile.open(fileout);
  myfile << msTime << " ms" << '\n';

  for (auto lineup : lineups) {
    int totalcost = 0;
    bitset<128> bitset = lineup.set;
    int count = 0;
    int totalCount = lineup.getTotalCount();
    for (int i = 0; i < 128 && bitset.any() && count < totalCount; i++) {
      if (bitset[i]) {
        bitset[i] = false;
        count++;
        myfile << p[i].name;
        totalcost += p[i].cost;
        myfile << '\n';
      }
    }

    myfile << lineup.value;
    myfile << '\n';
    myfile << totalcost;
    myfile << '\n';
  }

  myfile << flush;
  myfile.close();
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

      myfile << '\n';
    }
  }
  myfile << flush;
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
      allPlayers << '\n';
    }
    allPlayers << flush;

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

      myfile << '\n';
    }
  }
  myfile << flush;
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
    myfile << '\n';
  }
  myfile << '\n';
  myfile << '\n';

  for (auto &lineup : setB) {
    for (auto &x : lineup) {
      myfile << x;
      myfile << ",";
    }
    myfile << '\n';
  }
  myfile << flush;
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
      myfile << '\n';
    }
    myfile << endl;

    myfile.close();
  }
}
