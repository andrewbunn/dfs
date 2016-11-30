#include <vector>
#include <array>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <queue>
#include <functional>
#include <random>
#include <cstdint>
#include <chrono>
#include <ppl.h>
#include <numeric>
#include <bitset>

// number of lineups to generate in optimizen - TODO make parameter
#define LINEUPCOUNT 10
// number of simulations to run of a set of lineups to determine expected value
#define SIMULATION_COUNT 1000
// number of random lineup sets to select
#define RANDOM_SET_COUNT 50000
// number of lineups we want to select from total pool
#define TARGET_LINEUP_COUNT 50
// number of pools to generate
#define NUM_ITERATIONS_OWNERSHIP 50
#define STOCHASTIC_OPTIMIZER_RUNS 500

using namespace concurrency;
using namespace std;

enum Position {
    qb = 0,
    rb = 1,
    wr = 2,
    te = 3,
    def = 4
};

int numPositions = 5;

int MaxPositionCount[5] = { 1, 3, 4, 2, 1 };
int PositionCount[5] = { 1, 2, 3, 1, 1 };
int slots[9] = {0, 1, 1, 2, 2, 2, 3, 5/*flex*/, 4 };

struct Player {
    string name;
    float proj;
    uint8_t cost;
    uint8_t pos;
    uint8_t index;
    Player(string s, uint8_t c, float p, uint8_t pos, uint8_t idx) : name(s), cost(c), proj(p), pos(pos), index(idx)
    {}

    Player() {}
};

struct PlayerSim : public Player
{
    float stdDev;
    normal_distribution<float> distribution;

    PlayerSim(string s, uint8_t c, float p, uint8_t pos, uint8_t idx, float sd)
        : Player(s, c, p, pos, idx), stdDev(sd), distribution(p, sd) {}

    PlayerSim(Player p, float sd)
        : Player(p), stdDev(sd), distribution(p.proj, sd) {}
};

struct Players {
    array<int, 9> p;
    array<int, 5> posCounts;
    int totalCount;
    float value;
    bool hasFlex;

    inline void addPlayer(int pos, float proj, int index)
    {
            posCounts[pos]++;
            p[totalCount++] = index;
            value += proj;
    }

    bool tryAddPlayer(int pos, float proj, int index)
    {
        int diff = posCounts[pos] - PositionCount[pos];
        if (diff < 0)
        {
            addPlayer(pos, proj, index);
            return true;
        }

        if (hasFlex || pos == 0 || pos == 4 || diff > 1)
        {
            return false;
        }

        addPlayer(pos, proj, index);
        hasFlex = true;
        return true;
    }

    Players() : totalCount(0), value(0), /*costOfUnfilledPositions(90),*/ hasFlex(false)
    {
        posCounts.fill(0);
    }
};

struct Players2 {
    uint64_t bitset;
    array<uint8_t, 5> posCounts;
    uint8_t totalCount;
    float value;
    bool hasFlex;

    inline bool addPlayer(int pos, float proj, int index)
    {
        uint64_t bit = (uint64_t)1 << index;
        if ((bitset & bit) == 0)
        {
            posCounts[pos]++;
            bitset |= bit;
            totalCount++;
            value += proj;
            return true;
        }
        return false;
    }

    bool tryAddPlayer(int pos, float proj, int index)
    {
        int diff = posCounts[pos] - PositionCount[pos];
        if (diff < 0)
        {
            return addPlayer(pos, proj, index);
        }

        if (hasFlex || pos == 0 || pos == 4 || diff > 1)
        {
            return false;
        }

        bool succeeded = addPlayer(pos, proj, index);
        hasFlex = succeeded;
        return succeeded;
    }

    bool Players2::operator==(const Players2& other) {
        return bitset == other.bitset;
    }

    Players2() : totalCount(0), value(0), bitset(0), /*costOfUnfilledPositions(90),*/ hasFlex(false)
    {
        posCounts.fill(0);
    }
};

bool operator==(const Players2& first, const Players2& other) {
    return first.bitset == other.bitset;
}

bool operator<(const Players2& lhs, const Players2& rhs)
{
    // backwards so highest value is "lowest" (best ranked lineup)
    float diff = lhs.value - rhs.value;
    if (diff == 0)
    {
        return lhs.bitset > rhs.bitset;
    }
    else
    {
        return diff > 0;
    }
}

size_t players_hash(const Players2& ps)
{
    return hash<uint64_t>()(ps.bitset);
}

vector<string> getNextLineAndSplitIntoTokens(istream& str)
{
    vector<string>   result;
    string                line;
    getline(str, line);

    stringstream          lineStream(line);
    string                cell;

    while (getline(lineStream, cell, ','))
    {
        result.push_back(cell);
    }
    return result;
}

vector<string> parseNames(string filename)
{
    vector<string> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    int count = 0;
    while (tokens.size() == 1)
    {
        result.emplace_back(tokens[0]);
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}

vector<Player> parsePlayers(string filename)
{
    vector<Player> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    int count = 0;
    while (tokens.size() >= 4)
    {
        int c = atoi(tokens[1].c_str()) - 10;
        float p = (float)atof(tokens[2].c_str());
        int pos = atoi(tokens[3].c_str());
        if (pos == 0)
        {
            c -= 10;
        }
        if (tokens.size() == 4)
        {
            result.emplace_back(tokens[0], c, p, pos, count++);
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}
vector<vector<string>> parseLineupString(const string filename)
{
    vector<vector<string>> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    vector<string> current;

    while (tokens.size() == 1)
    {
        if (tokens[0].size() > 0)
        {
            if (isalpha(tokens[0][0]))
            {
                current.push_back(tokens[0]);
            }
            else
            {
                // value/cost number, signifies end of lineup.
                // duration at start of file
                if (current.size() > 0)
                {
                    result.push_back(current);
                    current.clear();
                }
            }
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}
// for now just parse player names like output.csv current does, could make format easier to parse
vector<vector<uint8_t>> parseLineups(string filename, unordered_map<string, uint8_t>& playerIndices)
{
    vector<vector<uint8_t>> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    vector<uint8_t> current;

    while (tokens.size() == 1)
    {
        if (tokens[0].size() > 0)
        {
            if (isalpha(tokens[0][0]))
            {
                auto it = playerIndices.find(tokens[0]);
                if (it != playerIndices.end())
                {
                    current.push_back(it->second);
                }
                else
                {
                    throw invalid_argument("Invalid player: " + tokens[0]);
                }
            }
            else
            {
                // value/cost number, signifies end of lineup.
                // duration at start of file
                if (current.size() > 0)
                {
                    result.push_back(current);
                    current.clear();
                }
            }
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}

// can transpose players of same type
Players2 knapsackPositions(int budget, int pos, const Players2 oldLineup, const vector<vector<Player>>& players)
{
    if (pos >= 9)
    {
        return oldLineup;
    }

    Players2 bestLineup = oldLineup;
    auto loop = [&](const Player& p)
    {
        Players2 currentLineup = oldLineup;
        if (p.cost <= budget)
        {
            if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
            {
                return knapsackPositions(budget - p.cost, pos + 1, currentLineup, players);
            }
        }
        return currentLineup;
    };
    if (pos <= 2)
    {
        //parallel_for(size_t(0), players[pos].size(), loop);
        // could parallel transform each to a "lineup", keep best lineup
        vector<Players2> lineupResults(players[pos].size());
        //parallel_for_each(begin(players[pos]), end(players[pos]), loop);
        parallel_transform(begin(players[pos]), end(players[pos]), begin(lineupResults), loop);

        for (auto lineup : lineupResults)
        {
            if (lineup.value > bestLineup.value)
            {
                bestLineup = lineup;
            }
        }
    }
    else
    {

        for (int i = 0; i < players[pos].size(); i++)
        {
            const Player& p = players[pos][i];
            Players2 currentLineup = oldLineup;
            if (p.cost <= budget)
            {
                if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
                {
                    Players2 lineup = knapsackPositions(budget - p.cost, pos + 1, currentLineup, players);
                    if (lineup.value > bestLineup.value)
                    {
                        bestLineup = lineup;
                    }
                }
            }
        }
    }
    return bestLineup;
}

typedef vector<Players2> lineup_list;



// can transpose players of same type
lineup_list knapsackPositionsN(int budget, int pos, const Players2 oldLineup, const vector<vector<Player>>& players, int rbStartPos, int wrStartPos, int skipPositionSet)
{
    while (skipPositionSet != 0 && (skipPositionSet & (1 << pos)) != 0)
    {
        skipPositionSet &= ~(1 << pos);
        pos++;
    }
    if (pos >= 9)
    {
        return lineup_list(1, oldLineup);
    }


    // dont process players we've already calculated in transpositions:
    int startPos;
    bool isWR = false;
    bool isRB = false;
    if (pos == 1 || pos == 2)
    {
        isRB = true;
        startPos = rbStartPos;
    }
    else if (pos == 3 || pos == 4 || pos == 5)
    {
        isWR = true;
        startPos = wrStartPos;
    }
    else
    {
        startPos = 0;
    }
    auto loop = [&](const Player& p)
    {
        Players2 currentLineup = oldLineup;
        if (p.cost <= budget)
        {
            if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
            {
                return knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players, isRB ? (&p - &players[pos][0]) + 1 : 0, isWR ? (&p - &players[pos][0]) + 1 : 0, skipPositionSet);
            }
        }

        return lineup_list(1, currentLineup);
    };
    if (pos <= 2)
    {
        vector<lineup_list> lineupResults(players[pos].size() - startPos);
        parallel_transform(begin(players[pos]) + startPos, end(players[pos]), begin(lineupResults), loop);

        lineup_list merged;
        merged.reserve(LINEUPCOUNT * lineupResults.size());
        for (auto& lineup : lineupResults)
        {
            merged.insert(merged.end(), lineup.begin(), lineup.end());
            sort(merged.begin(), merged.end());
            unique(merged.begin(), merged.end());
            if (merged.size() > LINEUPCOUNT)
            {
                merged.resize(LINEUPCOUNT);
            }
        }

        return merged;
    }
    else
    {
        lineup_list bestLineups;
        bestLineups.reserve(2 * LINEUPCOUNT);
        bestLineups.push_back(oldLineup);
        for (int i = startPos; i < players[pos].size(); i++)
        {
            const Player& p = players[pos][i];
            Players2 currentLineup = oldLineup;
            if (p.cost <= budget)
            {
                if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
                {
                    // optimization to inline last call
                    if (pos == 8)
                    {
                        bestLineups.push_back(currentLineup);
                    }
                    else
                    {
                        lineup_list lineups = knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players, isRB ? i + 1 : 0, isWR ? i + 1 : 0, skipPositionSet);
                        bestLineups.insert(bestLineups.end(), lineups.begin(), lineups.end());
                    }
                    sort(bestLineups.begin(), bestLineups.end());
                    unique(bestLineups.begin(), bestLineups.end());

                    if (bestLineups.size() > LINEUPCOUNT)
                    {
                        bestLineups.resize(LINEUPCOUNT);
                    }
                }
            }
        }
        return bestLineups;
    }
}

Players2 generateLineup(vector<Player>& p)
{
    vector<vector<Player>> playersByPos(9);
    for (int i = 0; i < 9; i++)
    {
        for (auto& pl : p)
        {
            if (pl.pos == slots[i])
            {
                playersByPos[i].push_back(pl);
            }
            else if (slots[i] == 5 && (pl.pos == 1 || pl.pos == 2 || pl.pos == 3))
            {
                playersByPos[i].push_back(pl);
            }
        }
    }

    return knapsackPositions(100, 0, Players2(), playersByPos);
}

lineup_list generateLineupN(vector<Player>& p, vector<string>& disallowedPlayers, Players2 currentPlayers, int budgetUsed, double& msTime)
{
    auto start = chrono::steady_clock::now();
    vector<vector<Player>> playersByPos(9);
    for (int i = 0; i < 9; i++)
    {
        for (auto& pl : p)
        {
            auto it = find(disallowedPlayers.begin(), disallowedPlayers.end(), pl.name);
            if (it == disallowedPlayers.end())
            {
                if (pl.pos == slots[i])
                {
                    playersByPos[i].push_back(pl);
                }
                else if (slots[i] == 5 && (pl.pos == 1 || pl.pos == 2 || pl.pos == 3))
                {
                    playersByPos[i].push_back(pl);
                }
            }
        }
    }

    // skip positions
    // 2^9 int to 
    int skipPositionsSet = 0;
    if (currentPlayers.totalCount > 0)
    {
        for (int i = 0; i < currentPlayers.posCounts.size(); i++)
        {
            int count = currentPlayers.posCounts[i];
            if (count > 0)
            {
                if (i == 0)
                {
                    skipPositionsSet |= 1;
                }
                else if (i == 1)
                {
                    skipPositionsSet |= 2;
                    if (count > 1)
                    {
                        skipPositionsSet |= 4;
                        if (count > 2)
                        {
                            // flex
                            skipPositionsSet |= 1 << 7;
                        }
                    }
                }
                else if (i == 2)
                {
                    skipPositionsSet |= 8;
                    if (count > 1)
                    {
                        skipPositionsSet |= 16;
                        if (count > 2)
                        {
                            skipPositionsSet |= 32;
                            if (count > 3)
                            {
                                skipPositionsSet |= 1 << 7;
                            }
                        }
                    }
                }
                else if (i == 3)
                {
                    skipPositionsSet |= 1 << 6;
                }
                else if (i == 4)
                {
                    skipPositionsSet |= 1 << 9;
                }

            }
        }
    }
    // budget is wrong
    lineup_list output = knapsackPositionsN(100 - budgetUsed, 0, currentPlayers, playersByPos, 0, 0, skipPositionsSet);

    auto end = chrono::steady_clock::now();
    auto diff = end - start;
    msTime = chrono::duration <double, milli>(diff).count();
    return output;
}

void runPlayerOptimizer(string filein, string fileout)
{
    vector<Player> p = parsePlayers(filein);
    auto start = chrono::steady_clock::now();

    Players2 lineup = generateLineup(p);
    auto end = chrono::steady_clock::now();
    auto diff = end - start;

    ofstream myfile;
    myfile.open(fileout);
    myfile << chrono::duration <double, milli>(diff).count() << " ms" << endl;

        int totalcost = 0;
        uint64_t bitset = lineup.bitset;
        int count = 0;
        for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
        {
            if (bitset & 1 == 1)
            {
                count++;
                myfile << p[i].name;
                totalcost += p[i].cost;
                myfile << endl;
            }
            bitset = bitset >> 1;
        }

        myfile << lineup.value;
        myfile << endl;
        myfile << totalcost;
        myfile << endl;
        myfile << endl;

    myfile.close();
}

void saveLineupList(vector<Player>& p, lineup_list& lineups, string fileout, double msTime)
{
    ofstream myfile;
    myfile.open(fileout);
    myfile << msTime << " ms" << endl;

    for (auto lineup : lineups)
    {
        int totalcost = 0;
        uint64_t bitset = lineup.bitset;
        int count = 0;
        for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
        {
            if (bitset & 1 == 1)
            {
                count++;
                myfile << p[i].name;
                totalcost += p[i].cost;
                myfile << endl;
            }
            bitset = bitset >> 1;
        }

        myfile << lineup.value;
        myfile << endl;
        myfile << totalcost;
        myfile << endl;
    }

    myfile.close();
}

void runPlayerOptimizerN(string filein, string fileout, string lineupstart)
{
    vector<Player> p = parsePlayers(filein);
    vector<string> startingPlayers = parseNames(lineupstart);

    Players2 startingLineup;
    // if we can't add player, go to next round
    int budgetUsed = 0;
    for (auto& pl : startingPlayers)
    {
        auto it = find_if(p.begin(), p.end(), [&pl](const Player& p) {
            return pl == p.name;
        });
        if (it != p.end())
        {
            startingLineup.tryAddPlayer(it->pos, it->proj, it->index);
            budgetUsed += it->cost;
        }

        // output which set we're processing
        cout << pl << ",";
    }

    double msTime = 0;
    lineup_list lineups = generateLineupN(p, vector<string>(), startingLineup, budgetUsed, msTime);
    saveLineupList(p, lineups, fileout, msTime);
}

void normalizeName(string& name)
{
    // normalize to just 'a-z' + space
    // what about jr/sr?
    transform(name.begin(), name.end(), name.begin(), ::tolower);
    //return name;
    auto it = copy_if(name.begin(), name.end(), name.begin(), 
        [](char& c) {
        return (islower(c) != 0) || c == ' ';
    });
    name.resize(distance(name.begin(), it));

    // sr/jr
    static string sr = " sr";
    static string jr = " jr";

    it = find_end(name.begin(), name.end(), sr.begin(), sr.end());
    name.resize(distance(name.begin(), it));

    it = find_end(name.begin(), name.end(), jr.begin(), jr.end());
    name.resize(distance(name.begin(), it));

    array<string, 32> dsts = {
        "Baltimore",
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
            "Los Angeles",
            "Philadelphia",
            "New York Jets",
            "Dallas",
            "Cincinnati",
            "Cleveland",
            "San Diego",
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
            "Buffalo"
    };

    for_each(dsts.begin(), dsts.end(), [](string& n) {
        transform(n.begin(), n.end(), n.begin(), ::tolower);
    });

    auto i = find_if(dsts.begin(), dsts.end(), [&name](string& n){
        return name.find(n) != string::npos;
    });
    if (i != dsts.end())
    {
        name = *i;
    }
}

vector<tuple<string, int, int>> parseCosts(string filename)
{
    vector<tuple<string, int, int>> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    int count = 0;
    while (tokens.size() == 3)
    {
        int c = stoi(tokens[1]);
        int pos = stoi(tokens[2]);
        normalizeName(tokens[0]);
        // dont add david johnson TE
        if (tokens[0] != "david johnson" || pos == 1)
        {
            result.emplace_back(tokens[0], c, pos);
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}

unordered_map<string, float> parseProjections(string filename)
{
    unordered_map<string, float> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    int count = 0;
    // two david johnsons, just only add first value for now, should be fine
    while (tokens.size() == 2)
    {
        float proj = stof(tokens[1]);
        normalizeName(tokens[0]);
        if (result.find(tokens[0]) != result.end())
        {
            cout << tokens[0] << endl;
        }
        else
        {
            result.emplace(tokens[0], proj);
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }
    cout << endl;
    return result;
}

vector<pair<string, float>> parseOwnership(string filename)
{
    vector<pair<string, float>> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    int count = 0;
    // two david johnsons, just only add first value for now, should be fine
    while (tokens.size() >= 2)
    {
        float proj = stof(tokens[tokens.size() - 1]);
        normalizeName(tokens[0]);
        result.emplace_back(tokens[0], proj);
        tokens = getNextLineAndSplitIntoTokens(file);
    }
    return result;
}

vector<string> parseRanks(string filename)
{
    vector<string> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    while (tokens.size() >= 1)
    {
        normalizeName(tokens[0]);

        result.emplace_back(tokens[0]);
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}


// "tweak projections" take generated values from costs + numfire, perform swaps based on input
void tweakProjections(vector<tuple<string, int, float, int>>& positionPlayers, int pos)
{
    ostringstream stream;
    stream << "rankings-pos" << pos << ".csv";
    vector<string> ranks = parseRanks(stream.str());

    for (int i = 0; i < ranks.size(); i++)
    {
        auto& name = ranks[i];
        // get rank
        // if rank off by threshold, swap, swap values
        auto it = find_if(positionPlayers.begin() + i, positionPlayers.end(), [&name](tuple<string, int, float, int>& p){
            return (get<0>(p) == name);
        });
        if (it != positionPlayers.end())
        {
            int diff = distance(positionPlayers.begin() + i, it);
            if (diff >= 1)
            {
                // bring elem at it to i, shifting other elements down, get proj for this position first
                //float proj = get<2>(positionPlayers[i]);

                for (int j = i + diff; j > i; j--)
                {
                    swap(positionPlayers[j], positionPlayers[j - 1]);
                    float proj = get<2>(positionPlayers[j]);
                    get<2>(positionPlayers[j]) = get<2>(positionPlayers[j - 1]);
                    get<2>(positionPlayers[j - 1]) = proj;
                }
            }
        }
    }

}

void playerStrictDominator(vector<Player>& players)
{
    for (int i = 0; i <= 4; i++)
    {
        vector<Player> positionPlayers;
        copy_if(players.begin(), players.end(), back_inserter(positionPlayers), [i](Player& p) {
            return (p.pos == i);
        });

        // sort by value, descending
        sort(positionPlayers.begin(), positionPlayers.end(), [](Player& a, Player& b) { return a.proj > b.proj; });

        // biggest issue is for rb/wr we dont account for how many we can use.
        for (int j = 0; j < positionPlayers.size(); j++)
        {
            // remove all players with cost > player
            //auto& p = positionPlayers[j];
            auto it = remove_if(positionPlayers.begin() + j + 1, positionPlayers.end(), [&](Player& a) {

                // PositionCount

                static float minValue[5] = { 8, 8, 8, 5, 5 };
                return a.proj < minValue[i] ||
                    (count_if(positionPlayers.begin(), positionPlayers.begin() + j + 1, [&](Player& p) {
                    static float epsilon = 0;

                    // probably want minvalue by pos 8,8,8,5,5?
                    // probably want a bit more aggression here, if equal cost but ones player dominates the other
                    // cost > current player && value < current player
                    int costDiff = (p.cost - a.cost);
                    float valueDiff = p.proj - a.proj;
                    bool lessValuable = (valueDiff > epsilon);
                    bool atLeastAsExpensive = costDiff <= 0;
                    return (atLeastAsExpensive && lessValuable) ||
                        costDiff <= -3;
                }) >= MaxPositionCount[i]);
            });

            positionPlayers.resize(distance(positionPlayers.begin(), it));
        }
        
        auto itR = remove_if(players.begin(), players.end(), [&](Player& a)
        {
            if (a.pos == i)
            {
                auto it = find_if(positionPlayers.begin(), positionPlayers.end(), [&](Player& b)
                {
                    return a.name == b.name;
                });
                return it == positionPlayers.end();
            }
            return false;
        });
        players.resize(distance(players.begin(), itR));
    }

    for (int i = 0; i < players.size(); i++)
    {
        players[i].index = i;
    }
}

// different idea, curious to see results
void stochasticOptimizer()
{
    // randomly choose "results" of the week to get an optimal lineup set given those results
    // save that lineup set
    // then run simulations with those sets to select optimal sharpe based set

    vector<Player> p = parsePlayers("players.csv");

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);
    int iterations = STOCHASTIC_OPTIMIZER_RUNS;
    for (int i = 0; i < iterations; i++)
    {
        // randomly get values for each player, recreate players array and remove dominated players for fast optimization
        vector<Player> playersRandom;
        playersRandom.reserve(p.size());

        for (auto& player : p)
        {
            normal_distribution<float> distribution(player.proj, 6.0f);
            float newProj = distribution(generator);
            playersRandom.emplace_back(player.name, player.cost, newProj, player.pos, player.index);
        }

        // remove dominated players 
        playerStrictDominator(playersRandom);
        cout << playersRandom.size() << endl;
        // generate lineups and save
        double msTime = 0;
        lineup_list lineups = generateLineupN(playersRandom, vector<string>(), Players2(), 0, msTime);
        
        {
            ostringstream stream;
            stream << "output-" << i << ".csv";
            saveLineupList(playersRandom, lineups, stream.str(), msTime);
        }
    }
}

// main flaw is that we need lots of ownership restrictions to ensure we're not too overweight on any players.
// but putting in lots of players "excludes" a lot of value 
// i think we can try an iterative process to determine "ownership" file but might need more mutex based selection because of limiting pool so much
// also can open up players.csv to allow more options with this.
// this works great!
// ideally want more "iterations" since randomly drawing even 50 sets won't have good representation of combinations
// ways to improve are to specify ownership relationships (stacks, avoid certain stacks)
// so {includes}{excludes}{weighting}
// then it might be a reasonable strategy to iterate driver -> simulator to get enough diversity to find optimal "sharpe" value
// then we just need to improve stdev data for better simulations (ideally improve projections)
// figure out best way to use bigger pool but still get approximate player ownership targets
// improve simulations with better cutoff estimation - 
// numberfire has CI for projection can extrapolation stddev from that
// std dev of receptions points + stdv from CI, for now just use CI by itself
// needs to be not multiset driven but some other way to express allowed players/forced players
void ownershipDriver(string playersFile, string ownershipFile)
{
    // goal is to "choose" player and output lineups given chosen set
    // we have list of players where ownership is limited, for each super set of those players, output X lineups
    // then in simulations, choose sets that match ownership

    vector<Player> p = parsePlayers(playersFile);
    vector<pair<string, float>> ownership = parseOwnership(ownershipFile);
    
    vector<string> targetPlayers(ownership.size());
    transform(ownership.begin(), ownership.end(), targetPlayers.begin(), [](const pair<const string,float>& it)
    {
        return it.first;
    });
    
    // X sets where its based on randomness, so 20? for now

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);
    uniform_real_distribution<float> distribution(0.f, 1.0f);
    // 2^n
    for (int i = 0; i < NUM_ITERATIONS_OWNERSHIP; i++)
    {
        // now targetplayers is all players we want to "toggle"
        vector<string> includedPlayers;
        vector<string> excludedPlayers;
        // needs to be some relationships between players:
        // eg. murray/david johnson should be mutex but maybe not disallowed players
        shuffle(targetPlayers.begin(), targetPlayers.end(), generator);
        partition_copy(targetPlayers.begin(), targetPlayers.end(), back_inserter(includedPlayers), back_inserter(excludedPlayers), [&](string& player)
        {
            // randomly choose player with probability
            // flaw is that if, example, a qb is listed after another qb, second qbs odds are even lower.
            // one option is to shuffle target players... other option is to have mutex weight on qb for example (25% tom brady, 25% brees, 10% bortles, {remaining is anyone else})
            int index = &player - &targetPlayers[0];
            return distribution(generator) <= ownership[index].second;
        });
        Players2 startingLineup;
        bool valid = true;
        // if we can't add player, go to next round
        int budgetUsed = 0;
        for (auto& pl : includedPlayers)
        {
            auto it = find_if(p.begin(), p.end(), [&pl](const Player& p) {
                return pl == p.name;
            });
            if (it != p.end())
            {
                if (!startingLineup.tryAddPlayer(it->pos, it->proj, it->index))
                {
                    valid = false;
                    i--;
                    break;
                }
                budgetUsed += it->cost;
            }

            // output which set we're processing
            cout << pl << ",";
        }
        if (!valid)
        {
            cout << "invalid";
            continue;
        }

        {
            double msTime = 0;
            lineup_list lineups = generateLineupN(p, excludedPlayers, startingLineup, budgetUsed, msTime);

            // skip over lineups where
            if (lineups[0].totalCount < 9 || lineups[0].value < 121)
            {
                cout << "low value: " << lineups[0].value << "," << lineups[0].totalCount;
                i--;
                continue;
            }
            else
            {
                ostringstream stream;
                stream << "output-" << i << ".csv";
                saveLineupList(p, lineups, stream.str(), msTime);
            }
        }
        cout << endl;
    }
}

void removeDominatedPlayers(string filein, string fileout)
{
    vector<Player> players = parsePlayers(filein);

    unordered_map<string, float> differencing = parseProjections("diffs.csv");
    for (auto& p : players)
    {
        auto itDiff = differencing.find(p.name);

        // players we dont want to include can just have large negative diff
        if (itDiff != differencing.end())
        {
            p.proj += itDiff->second;
        }
    }

    ofstream myfile;
    myfile.open(fileout);
    // for a slot, if there is a player cheaper cost but > epsilon score, ignore
    // no def for now?
    for (int i = 0; i <= 4; i++)
    {
        vector<Player> positionPlayers;
        copy_if(players.begin(), players.end(), back_inserter(positionPlayers), [i](Player& p) {
            return (p.pos == i);
        });

        // sort by value, descending
        sort(positionPlayers.begin(), positionPlayers.end(), [](Player& a, Player& b) { return a.proj > b.proj; });

        // biggest issue is for rb/wr we dont account for how many we can use.
        for (int j = 0; j < positionPlayers.size(); j++)
        {
            // remove all players with cost > player
            //auto& p = positionPlayers[j];
            auto it = remove_if(positionPlayers.begin() + j + 1, positionPlayers.end(), [&](Player& a) {

                // PositionCount
                static float minValue[5] = { 8, 8, 8, 5, 5 };
                return a.proj < minValue[i] ||
                    (count_if(positionPlayers.begin(), positionPlayers.begin() + j + 1, [&](Player& p) {
                    static float epsilon = 1;

                    // probably want minvalue by pos 8,8,8,5,5?
                    // probably want a bit more aggression here, if equal cost but ones player dominates the other
                    // cost > current player && value < current player
                    int costDiff = (p.cost - a.cost);
                    float valueDiff = p.proj - a.proj;
                    bool lessValuable = (valueDiff > epsilon);
                    bool atLeastAsExpensive = costDiff <= 0;
                    return (atLeastAsExpensive && lessValuable) ||
                        costDiff <= -3;
                }) >= MaxPositionCount[i]);
            });

            positionPlayers.resize(distance(positionPlayers.begin(), it));
        }

        for (auto& p : positionPlayers)
        {
            myfile << p.name;
            myfile << ",";
            myfile << (static_cast<int>(p.cost) + 10 + ((p.pos == 0) ? 10 : 0));
            myfile << ",";
            myfile << static_cast<float>(p.proj);
            myfile << ",";
            myfile << static_cast<int>(p.pos);
            myfile << ",";

            myfile << endl;
        }
    }
    myfile.close();
}

// or calculate?
void importProjections(string fileout, bool tweaked, bool tweakAndDiff)
{
    // generate csv with our format: player, cost, proj, posid
    // one option is start with analytics results, pull down
    // how are we going to customize? pull down jake ciely and "swap"

    // can use larger player sets if we remove "dominated players"
    // player is dominated if there are n (positional count) players of lesser price with higher value
    // initial set can have a threshold to let in more options for toggling

    // most helpful would be scraper for yahoo, numberfire

    // for now just assimilate data we parsed from sites
    vector<tuple<string, int, int>> costs = parseCosts("costs.csv");
    unordered_map<string, float> projections = parseProjections("projs.csv");
    unordered_map<string, float> differencing = parseProjections("diffs.csv");
    vector<tuple<string, int, float, int>> playersResult;
    for (auto& player : costs)
    {
        auto iter = projections.find(get<0>(player));
        if (iter != projections.end())
        {
            float proj = iter->second;
            if (!tweaked || tweakAndDiff)
            {
                auto itDiff = differencing.find(get<0>(player));

                // players we dont want to include can just have large negative diff
                if (itDiff != differencing.end())
                {
                    proj += itDiff->second;
                }
            }
            playersResult.emplace_back(get<0>(player), get<1>(player), proj, get<2>(player));
        }
        else
        {
            cout << get<0>(player) << endl;
        }
    }


    ofstream myfile;
    myfile.open(fileout);
    // for a slot, if there is a player cheaper cost but > epsilon score, ignore
    // no def for now?
    for (int i = 0; i <= 4; i++)
    {
        vector<tuple<string, int, float, int>> positionPlayers;
        copy_if(playersResult.begin(), playersResult.end(), back_inserter(positionPlayers), [i](tuple<string, int, float, int>& p){
            return (get<3>(p) == i);
        });

        // sort by value, descending
        sort(positionPlayers.begin(), positionPlayers.end(), [](tuple<string, int, float, int>& a, tuple<string, int, float, int>& b) { return get<2>(a) > get<2>(b); });

        // don't rely much on "tweaked projections", maybe a few lineups using this method
        // more importantly, "personal tweaks" via differencing lists
        // differencing file can contain players we like/dont like for the week with adjusted projs (+2, -2, *)
        if (tweaked)
        {
            tweakProjections(positionPlayers, i);
        }
        // getRankings for tweaking

        // biggest issue is for rb/wr we dont account for how many we can use.
        for (int j = 0; j < positionPlayers.size(); j++)
        {
            // remove all players with cost > player
            //auto& p = positionPlayers[j];
            auto it = remove_if(positionPlayers.begin() + j + 1, positionPlayers.end(), [&](tuple<string, int, float, int>& a){

                // PositionCount
                static float minValue[5] = { 8, 8, 8, 5, 5 };
                return get<2>(a) < minValue[i] ||
                    (count_if(positionPlayers.begin(), positionPlayers.begin() + j + 1, [&](tuple<string, int, float, int>& p){
                    static float epsilon = 1;

                    // probably want minvalue by pos 8,8,8,5,5?
                    // probably want a bit more aggression here, if equal cost but ones player dominates the other
                    // cost > current player && value < current player
                    int costDiff = (get<1>(p) - get<1>(a));
                    float valueDiff = get<2>(p) - get<2>(a);
                    bool lessValuable = (valueDiff > epsilon);
                    bool atLeastAsExpensive = costDiff <= 0;
                    return (atLeastAsExpensive && lessValuable) || 
                        costDiff <= -3;
                }) >= MaxPositionCount[i]);
            });

            positionPlayers.resize(distance(positionPlayers.begin(), it));
        }

        for (auto& p : positionPlayers)
        {
            myfile << get<0>(p);
            myfile << ",";
            myfile << get<1>(p);
            myfile << ",";
            myfile << get<2>(p);
            myfile << ",";
            myfile << get<3>(p);
            myfile << ",";

            myfile << endl;
        }
    }
    myfile.close();
}

// monte carlo simulations
// input: list of lineups (output of optimizen?)
// players projections, std dev (normal_distribution)
// for a simulation we select a subset of lineups
// run N simulations:
// randomly generate score for all involved players, then determine total prize winnings based on scores of each lineup
// use some backdata to calculate lineup score -> prize winnings
// average prize winnings over all simulations to determine EV of that lineup set

inline float generateScore(uint8_t player, const vector<PlayerSim>& allPlayers, default_random_engine& generator)
{
    const PlayerSim& p = allPlayers[player];
    // need to research what std dev should be
    //return p.distribution(generator);
    normal_distribution<float> distribution(p.proj, 10.0f);
    return distribution(generator);
}

#define CONTENDED_PLACEMENT_SLOTS 14

// cutoffs is sorted array of the average score for each prize cutoff
// alternatively we can do regression to have function of value -> winnings
inline float determineWinnings(float score, array<uint8_t, CONTENDED_PLACEMENT_SLOTS>& placements/*, vector<float>& winningsCutoffs, vector<float>& winningsValues*/)
{
    static array<int, 22> winnings = {
        10000,
        5000,
        3000,
        2000,
        1000,
        750,
        500,
        400,
        300,
        200,
        150,
        100,
        75,
        60,
        50,
        45,
        40,
        35,
        30,
        25,
        20,
        15
    };

    static array<float, 22> cutoffs = {
        181.14,
        179.58,
        173.4,
        172.8,
        171.14,
        168.7,
        167,
        165.1,
        162.78,
        160,
        158.68,
        156.3,
        154.2,
        151.64,
        149.94,
        147.18,
        142.4,
        138.18,
        135.1,
        130.24,
        126.34,
        121.3
    };
    // number of top level placements we can have in each slot
    static array<uint8_t, CONTENDED_PLACEMENT_SLOTS> slotsAvailable = {
        1,
        1,
        1,
        1,
        1,
        5,
        5,
        5,
        10,
        10,
        10,
        25,
        25,
        50
    };

    // binary search array of cutoffs
    auto it = lower_bound(cutoffs.begin(), cutoffs.end(), score, greater<float>());
    float value;
    if (it != cutoffs.end())
    {
        if (it - cutoffs.begin() < CONTENDED_PLACEMENT_SLOTS)
        {
            // we have 50 slots available so as long as lineupset coun t doesnt increase this can't overflow. still almost impossible
            while ((placements[it - cutoffs.begin()] == slotsAvailable[it - cutoffs.begin()]))
            {
                it++;
            }
            placements[it - cutoffs.begin()]++;
        }
        value = static_cast<float>(winnings[it - cutoffs.begin()]);
    }
    else
    {
        value = 0;
    }
    return value;
}
// ideas for improvements:
// customized deviations
// account for contest types/costs/prizes
// estimate thresholds specific to a given week? (how much obvious value is out there?)
//
// So we generate large chunk of lineups using one set of player limitations/projections
// then generate other sets, how do we incorporate those together?
// we could enforce % of lineups selected from each of those sets but how they would get selected/weighted ultimately depends
// on projections we use in simulation
// essentially, I want to somehow indicate that i want to significantly limit exposure to some players (eg. marshall)
// so lets say marshall is ~10% target. marshall allowed lineups are chosen 10% of time in set selection! done
// so we do our lineup generation in similar process to what we always did, but rather than just getting one lineup, we generate tons, and use those for simulation
// and enforce our "asset mix"
// then we need more data to drive our "asset mix"
//
// then in simulation, we can track things like variance of a set as well and select "best mix" based on more than just EV.
//
// other ways to improve: estimate cutoffs more accurately for a given week based on ownership? and then the random values we generate for a simulation affect those lines based on ownership
// 
//

pair<float, float> runSimulation(const vector<vector<uint8_t>>& lineups, const vector<PlayerSim>& allPlayers)
{
    float winningsTotal = 0.f;
    float runningAvg = 0.f;
    bool converged = false;
    static vector<float> simulationResults(SIMULATION_COUNT);
    parallel_transform(begin(simulationResults), end(simulationResults), begin(simulationResults), [&lineups, &allPlayers](const float&)
    {
        static thread_local unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
        static thread_local mt19937 generatorTh(seed1);

        // only 64 players considered? need universal index
        // map of index -> score
        array<float, 64> playerScores = {};
        // keep track of times we win high placing since that excludes additional same placements

        array<uint8_t, CONTENDED_PLACEMENT_SLOTS> placementCount = {};
        // create map of player -> generated score
        // for each lineup -> calculate score
        float winnings = 0.f;
        for (auto& lineup : lineups)
        {
            float lineupScore = 0.f;
            for (auto& player : lineup)
            {
                if (playerScores[player] == 0)
                {
                    playerScores[player] = generateScore(player, allPlayers, generatorTh);
                }
                lineupScore += playerScores[player];
            }
            // lookup score -> winnings
            // need to account for my own entries for winnings
            winnings += determineWinnings(lineupScore/*, winningsCutoffs, winningsValues*/, placementCount);
        }
        return winnings;
    });

    // we can calculate std dev per lineup and calculate risk of whole set
    // i guess just using simulated variance of set is fine for now
    winningsTotal = accumulate(simulationResults.begin(), simulationResults.end(), 0.f);
    // calculate std dev here:
    // sqrt (1/n * [(val - mean)^2 + ]


    float expectedValue = winningsTotal / SIMULATION_COUNT;
    transform(simulationResults.begin(), simulationResults.end(), simulationResults.begin(), [&expectedValue](float& val)
    {
        // variation below target:
        float diff = val - 10 * TARGET_LINEUP_COUNT;  //(val - expectedValue);
        if (diff < 0)
        {
            return diff * diff;
        }
        else
        {
            return 0.0f;
        }
    });

    float stdDev = accumulate(simulationResults.begin(), simulationResults.end(), 0.f);
    stdDev = sqrt(stdDev / SIMULATION_COUNT);

    return make_pair(expectedValue, stdDev);
}


// Reservoir sampling algo
inline vector<vector<uint8_t>> getRandomSet(const vector<vector<uint8_t>>& starterSet, const vector<vector<uint8_t>>& allLineups, default_random_engine& generator)
{
    vector<vector<uint8_t>> set = starterSet;
    // replace elements with gradually decreasing probability
    for (int i = TARGET_LINEUP_COUNT; i < allLineups.size(); i++)
    {
        uniform_int_distribution<int> distribution(0, i);
        int j = distribution(generator);
        if (j < TARGET_LINEUP_COUNT)
        {
            set[j] = allLineups[i];
        }
    }

    return set;
}

inline vector<vector<uint8_t>> getTargetSet(vector<vector<vector<uint8_t>>>& allLineups, default_random_engine& generator)
{
    vector<vector<uint8_t>> set(TARGET_LINEUP_COUNT);
    int currentIndex = 0;
    /*
    // satisfy ownership first then pull from last set
    // so for marshall pull from marshal sets at random?
    // easier is just have sets specified in ownership
    // marshall set, 
    // for now i can manually do it with output files or something
    // 
    int totalsets = NUM_ITERATIONS_OWNERSHIP;
    //int totalsets = STOCHASTIC_OPTIMIZER_RUNS;
    static uniform_int_distribution<int> distribution(0, totalsets - 1);
    static uniform_int_distribution<int> distributionLineup(0, LINEUPCOUNT - 1);
    while (currentIndex < TARGET_LINEUP_COUNT)
    {
        int i = distribution(generator);
        auto& targetLineups = allLineups[i];
        //int j = distributionLineup(generator);
        set[currentIndex] = targetLineups[0];
        //copy_n(targetLineups.begin() + j, 1, set.begin() + currentIndex);
        currentIndex += 1;
    }
    */

    
    for (int i = 0; i < NUM_ITERATIONS_OWNERSHIP && currentIndex < TARGET_LINEUP_COUNT; i++)
    {
        //auto & val = ownership[i];
        int currentTarget = (int)(TARGET_LINEUP_COUNT / NUM_ITERATIONS_OWNERSHIP);
        if (i == NUM_ITERATIONS_OWNERSHIP - 1)
        {
            currentTarget = TARGET_LINEUP_COUNT - currentIndex;
        }
        auto& targetLineups = allLineups[i];
        shuffle(targetLineups.begin(), targetLineups.end(), generator);
        // copy first n
        copy_n(targetLineups.begin(), currentTarget, set.begin() + currentIndex);
        currentIndex += currentTarget;
    }
    return set;
}

struct lineup_set
{
    vector<vector<uint8_t>> set;
    float ev;
    float stdev;
    float getSharpe()
    {
        return (ev - (10 * TARGET_LINEUP_COUNT)) / stdev;
    }
};

bool operator<(const lineup_set& lhs, const lineup_set& rhs)
{
    // backwards so highest value is "lowest" (best ranked lineup)
    float diff = lhs.ev - rhs.ev;
    if (diff == 0)
    {
        return lhs.stdev < rhs.stdev;
    }
    else
    {
        return diff > 0;
    }
}

void outputBestResults(vector<Player>& p, vector<lineup_set>& bestResults)
{
    // output bestset
    ofstream myfile;
    myfile.open("outputset.csv");

    //myfile << bestValue;
    //myfile << endl;
    for (auto& set : bestResults)
    {
        myfile << set.ev << "," << set.stdev << endl;
        for (auto& lineup : set.set)
        {
            for (auto& x : lineup)
            {
                myfile << p[x].name;
                myfile << ",";
            }
            myfile << endl;
        }
        myfile << endl;
    }

    myfile.close();
}

void outputSharpeResults(vector<Player>& p, vector<lineup_set>& bestSharpeResults)
{
    ofstream myfile;
    myfile.open("outputsetsharpe.csv");

    for (auto& set : bestSharpeResults)
    {
        myfile << set.ev << "," << set.stdev << endl;
        myfile << set.getSharpe() << endl;
        for (auto& lineup : set.set)
        {
            for (auto& x : lineup)
            {
                myfile << p[x].name;
                myfile << ",";
            }
            myfile << endl;
        }
        myfile << endl;
    }

    myfile.close();
}

float calcEV(vector<Player>& p, vector<uint8_t>& lineup)
{
    return accumulate(lineup.begin(), lineup.end(), 0.f, [&](float v, uint8_t i)
    {
        return v + p[i].proj;
    });
}

void setFromIndices(const vector<uint16_t>& setIndices, const vector<vector<vector<uint8_t>>>& allLineups, lineup_set& set)
{
    for (int i = 0; i < setIndices.size(); i++)
    {
        set.set[i] = allLineups[setIndices[i]][0];
    }
}

void getNextSet(vector<uint16_t>& setIndices, default_random_engine& generator)
{
    static uniform_int_distribution<int> distribution(0, TARGET_LINEUP_COUNT - 1);
    static uniform_int_distribution<int> distributionIndices(0, NUM_ITERATIONS_OWNERSHIP - 1);
    int i = distribution(generator);
    setIndices[i] = distributionIndices(generator);
}

lineup_set lineupSelectorOwnership(const string ownershipFile, const string playersFile)
{
    vector<Player> p = parsePlayers(playersFile);
    vector<PlayerSim> allPlayers;
    // create map of player -> index for lineup parser
    unordered_map<string, uint8_t> playerIndices;
    // for now just use same std dev
    for (auto& x : p)
    {
        allPlayers.emplace_back(x, 10.f);
        playerIndices.emplace(x.name, x.index);
    }

    //vector<pair<string, float>> ownership = parseOwnership(ownershipFile);

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);

    //int totalSets = STOCHASTIC_OPTIMIZER_RUNS;
    int totalSets = NUM_ITERATIONS_OWNERSHIP;// ownership.size();

    vector<vector<vector<uint8_t>>> allLineups(totalSets);
    for (int i = 0; i < totalSets; i++)
    {
        ostringstream stream;
        stream << "output-" << i << ".csv";
        allLineups[i] = parseLineups(stream.str(), playerIndices);
    }
    /*
    sort(allLineups.begin(), allLineups.end(), [&p](vector<vector<uint8_t>>& a, vector<vector<uint8_t>>& b)
    {
        return calcEV(p, a[0]) > calcEV(p, b[0]);
    });

    cout << calcEV(p, allLineups[0][0]) << endl;
    */
    // randomly pick a set -> if it will fit your requirements, pick a lineup
    // so we want 10% of some players, remaining % contains none of those players
    //= parseLineups(lineupsFile, playerIndices);

    // just shuffle and pull from sets
    // starterSet is just the first N lineups from all lineups which is the initialization of reservoir algorithm
   
    // get players
    // get all possible lineups
    // choose set
    // do random for now, randomly select set then run simulation
    float bestSharpe = 0.f;
    vector<lineup_set> bestResults;
    vector<lineup_set> bestSharpeResults;
    for (int i = 0; i < RANDOM_SET_COUNT; i++)
    {
        lineup_set set = { getTargetSet(allLineups, generator), 0, 0 };
        tie(set.ev, set.stdev) = runSimulation(set.set, allPlayers);

        if (set.getSharpe() > bestSharpe)
        {
            bestSharpe = set.getSharpe();
            cout << set.ev << ", " << set.getSharpe() << endl;
        }

        bestResults.push_back(set);
        sort(bestResults.begin(), bestResults.end());
        float lastRisk = 100000.f; // high so we keep first result
        // return / risk is sharpe ratio (its actually ev - 500) / stdev
        auto it = remove_if(bestResults.begin(), bestResults.end(), [&lastRisk](lineup_set& s)
        {
            if (s.stdev >= lastRisk)
            {
                return true;
            }
            else
            {
                lastRisk = s.stdev;
                return false;
            }
        });

        bestResults.resize(distance(bestResults.begin(), it));
        if (bestResults.size() > 10)
        {
            bestResults.resize(10);
        }

        bestSharpeResults.push_back(set);
        sort(bestSharpeResults.begin(), bestSharpeResults.end(), [](lineup_set& a, lineup_set& b)
        {
            return a.getSharpe() > b.getSharpe();
        });
        if (bestSharpeResults.size() > 10)
        {
            bestSharpeResults.resize(10);
        }
    }
    outputBestResults(p, bestResults);
    outputSharpeResults(p, bestSharpeResults);

    // analyze ownership on result
    vector<pair<string, float>> ownership = parseOwnership(ownershipFile);
    unordered_map<string, int> playerCounts;
    vector<string> targetPlayers(ownership.size());
    transform(ownership.begin(), ownership.end(), targetPlayers.begin(), [](const pair<const string, float>& it)
    {
        return it.first;
    });
    for (auto& i : ownership)
    {
        playerCounts.emplace(i.first, 0);
    }

    for (auto& l : (bestSharpeResults[0].set))
    {
        for (auto& x : l)
        {
            auto it = playerCounts.find(p[x].name);
            if (it != playerCounts.end())
            {
                playerCounts[p[x].name]++;
            }
        }
    }

    {
        ofstream myfile;
        myfile.open("ownershipResults.csv");
        for (auto& i : playerCounts)
        {
            myfile << i.first << ",";
            myfile << ((float)i.second / 50.f) << endl;
        }
    }

    return bestSharpeResults[0];
}

void lineupSelectorFilter(const string ownershipFile, const string playersFile)
{
    vector<Player> p = parsePlayers(playersFile);
    vector<PlayerSim> allPlayers;
    // create map of player -> index for lineup parser
    unordered_map<string, uint8_t> playerIndices;
    // for now just use same std dev
    for (auto& x : p)
    {
        allPlayers.emplace_back(x, 6.f);
        playerIndices.emplace(x.name, x.index);
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);

    //int totalSets = STOCHASTIC_OPTIMIZER_RUNS;
    int totalSets = NUM_ITERATIONS_OWNERSHIP;// ownership.size();

    vector<vector<vector<uint8_t>>> allLineups(totalSets);
    for (int i = 0; i < totalSets; i++)
    {
        ostringstream stream;
        stream << "output-" << i << ".csv";
        allLineups[i] = parseLineups(stream.str(), playerIndices);
    }
    float bestSharpe = 0.f;
    vector<lineup_set> bestResults;
    vector<lineup_set> bestSharpeResults;
    lineup_set set = { vector<vector<uint8_t>>(totalSets), 0, 0 };

    // all lineups
    for (int i = 0; i < totalSets; i++)
    {
        set.set[i] = allLineups[i][0];
    }

    for (int i = NUM_ITERATIONS_OWNERSHIP; i > 50; i--)
    {
        float worstSharpe = 10000.f;
        int worstIndex = 0;
        for (int j = 0; j < i; j++)
        {
            vector<uint8_t> removed = set.set[j];
            set.set.erase(set.set.begin() + j);
            tie(set.ev, set.stdev) = runSimulation(set.set, allPlayers);

            if (set.getSharpe() < worstSharpe)
            {
                worstIndex = j;
                worstSharpe = set.getSharpe();
                cout << set.ev << ", " << set.getSharpe() << endl;
            }
            set.set.push_back(removed);
        }
        // remove filtered lineup 
        set.set.erase(set.set.begin() + worstIndex);
    }
    bestSharpeResults.push_back(set);
    outputSharpeResults(p, bestSharpeResults);

    // analyze ownership on result
    vector<pair<string, float>> ownership = parseOwnership(ownershipFile);
    unordered_map<string, int> playerCounts;
    vector<string> targetPlayers(ownership.size());
    transform(ownership.begin(), ownership.end(), targetPlayers.begin(), [](const pair<const string, float>& it)
    {
        return it.first;
    });
    for (auto& i : ownership)
    {
        playerCounts.emplace(i.first, 0);
    }

    for (auto& l : (bestSharpeResults[0].set))
    {
        for (auto& x : l)
        {
            auto it = playerCounts.find(p[x].name);
            if (it != playerCounts.end())
            {
                playerCounts[p[x].name]++;
            }
        }
    }

    {
        ofstream myfile;
        myfile.open("ownershipResults.csv");
        for (auto& i : playerCounts)
        {
            myfile << i.first << ",";
            myfile << ((float)i.second / 50.f) << endl;
        }
        myfile.close();
    }
}

void splitLineups(const string lineups)
{
    vector<vector<string>> allLineups;
    allLineups = parseLineupString(lineups);

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);
    shuffle(allLineups.begin(), allLineups.end(), generator);
    ofstream myfile;
    myfile.open("outputsetSplit.csv");
    int i = 0;
    for (auto& lineup : allLineups)
    {
        for (auto& x : lineup)
        {
            myfile << x;
            myfile << ",";
        }
        myfile << endl;
        i++;
        if (i == 10)
        {
            myfile << endl;
        }
    }
    myfile << endl;

    myfile.close();

}

void lineupSelector(const string lineupsFile, const string playersFile)
{
    vector<Player> p = parsePlayers(playersFile);
    vector<PlayerSim> allPlayers;
    // create map of player -> index for lineup parser
    unordered_map<string, uint8_t> playerIndices;
    // for now just use same std dev
    for (auto& x : p)
    {
        allPlayers.emplace_back(x, 10.f);
        playerIndices.emplace(x.name, x.index);
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();

    default_random_engine generator(seed1);
    vector<vector<uint8_t>> allLineups = parseLineups(lineupsFile, playerIndices);

    // starterSet is just the first N lineups from all lineups which is the initialization of reservoir algorithm
    vector<vector<uint8_t>> starterSet;
    for (int i = 0; i < TARGET_LINEUP_COUNT; i++)
    {
        starterSet.push_back(allLineups[i]);
    }
    // get players
    // get all possible lineups
    // choose set
    // do random for now, randomly select set then run simulation
    float bestValue = 0.f;
    vector<vector<uint8_t>> bestSet;
    for (int i = 0; i < RANDOM_SET_COUNT; i++)
    {
        vector<vector<uint8_t>> set = getRandomSet(starterSet, allLineups, generator);
        float value;
        float ev;
        tie(value, ev) = runSimulation(set, allPlayers);
        if (value > bestValue)
        {
            bestValue = value;
            bestSet = set;
            cout << bestValue << endl;
        }
    }

    // output bestset
    ofstream myfile;
    myfile.open("outputset.csv");

    myfile << bestValue;
    myfile << endl;
    for (auto& lineup : bestSet)
    {
        for (auto& x : lineup)
        {
            myfile << p[x].name;
            myfile << ",";
        }
        myfile << endl;
    }

    myfile.close();
}

void determineOwnership()
{
    string playersFile = "players.csv";
    vector<Player> p = parsePlayers(playersFile);
    // run optimizer on raw data? then see highly valued players
    // what is highest ownership of a player we'd allow?

    // output 500 results
    // read results of top 500 total results

    unordered_map<string, uint8_t> playerIndices;
    // for now just use same std dev
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
    }

    //int totalSets = STOCHASTIC_OPTIMIZER_RUNS;
    int totalSets = NUM_ITERATIONS_OWNERSHIP;// ownership.size();

    string ownership = "ownershiptest.csv";
    vector<vector<uint8_t>> allLineups;
    vector<pair<string, float>> ownershipTarget;
    allLineups = parseLineups("output.csv", playerIndices);
    {
        unordered_map<string, int> playerCounts;
        for (auto& l : (allLineups))
        {
            for (auto& x : l)
            {
                auto it = playerCounts.find(p[x].name);
                if (it != playerCounts.end())
                {
                    playerCounts[p[x].name]++;
                }
                else
                {
                    playerCounts.emplace(p[x].name, 1);
                }
            }
        }

        vector<pair<string, int>> playerCountArr;
        copy(playerCounts.begin(), playerCounts.end(), back_inserter(playerCountArr));
        sort(playerCountArr.begin(), playerCountArr.end(), [](pair<string, int>& x, pair<string, int>& y) {
            return x.second > y.second;
        });

        for (auto& x : playerCountArr)
        {
            // assumes 500 lineups
            if (x.second < 180)
            {
                break;
            }
            // sqrt(x/5)*6
            // 100 -> 66?
            // 50 -> 33?
            // 30 -> 30?
            float valPercent = (float)x.second / 5.f;
            float targetPercent = sqrtf(valPercent) * 6 / 100.f;
            ownershipTarget.emplace_back(x.first, targetPercent);
        }

        ofstream myfile;
        myfile.open(ownership);
        for (auto& i : ownershipTarget)
        {
            myfile << i.first << ",";
            myfile << i.second << endl;
        }
        myfile.close();
    }

    // determine ownership/sharpe?

    // a player's sharpe score -> net return / players stdev
    // (proj - implied points)
    // 

    // iterations
    int iterations = 10;
    lineup_set bestset;
    bestset.stdev = 500.f;

    for (int i = 0; i < iterations; i++)
    {
        // create ownershipfile
        ownershipDriver(playersFile, ownership);
        lineup_set set = lineupSelectorOwnership(ownership, playersFile);
        if (set.getSharpe() > bestset.getSharpe())
        {
            bestset = set;
        }
        unordered_map<string, int> playerCounts;
        for (auto& l : (set.set))
        {
            for (auto& x : l)
            {
                auto it = playerCounts.find(p[x].name);
                if (it != playerCounts.end())
                {
                    playerCounts[p[x].name]++;
                }
                else
                {
                    playerCounts.emplace(p[x].name, 1);
                }
            }
        }

        vector<pair<string, int>> playerCountArr;
        copy(playerCounts.begin(), playerCounts.end(), back_inserter(playerCountArr));
        sort(playerCountArr.begin(), playerCountArr.end(), [](pair<string, int>& x, pair<string, int>& y) {
            return x.second < y.second;
        });
        // players that were previously specified use the resulting counts
        // new players exceeding threshold are rebalanced

        for (auto& x : playerCountArr)
        {
            auto it = find_if(ownershipTarget.begin(), ownershipTarget.end(), [&x](pair<string, float>& o)
            {
                return o.first == x.first;
            });
            if (it != ownershipTarget.end())
            {
                // update target ownership: (avg)
                it->second = (it->second + ((float)x.second / 50.f)) / 2.0f;
            }
            else
            {
                // assumes 500 lineups
                // how we generate % probably depends on iteration
                if (x.second > 50.f * (25.f /100.f))
                {
                    // sqrt(x/5)*6
                    // 100 -> 66?
                    // 50 -> 33?
                    // 30 -> 30?
                    float valPercent = (float)x.second * 2.f;
                    float targetPercent = sqrtf(valPercent) * 5 / 100.f;
                    ownershipTarget.emplace_back(x.first, targetPercent);
                }
            }
        }

        {

            ofstream myfile;
            myfile.open(ownership);
            for (auto& i : ownershipTarget)
            {
                myfile << i.first << ",";
                myfile << i.second << endl;
            }
            myfile.close();
        }
    }

    {
        ofstream myfile;
        myfile.open("outputsetsharpe-iter.csv");

            myfile << bestset.ev << "," << bestset.stdev << endl;
            myfile << bestset.getSharpe() << endl;
            for (auto& lineup : bestset.set)
            {
                for (auto& x : lineup)
                {
                    myfile << p[x].name;
                    myfile << ",";
                }
                myfile << endl;
            }
            myfile << endl;

        myfile.close();
    }
}

int main(int argc, char* argv[]) {
    // TODO:
    // make program properly usable via cmd line (optimize x y) (import x ) so we dont run from vs all the time
    // generate x lineups
    //
    //runPlayerOptimizer("players.csv", "output.csv");
    //importProjections("players.csv", false, false);


    // optimize [file] [output]
    // import [output] /*optional?*/
    if (argc > 1)
    {
        if (strcmp(argv[1], "optimize") == 0)
        {
            string filein, fileout;
            if (argc > 2)
            {
                filein = argv[2];
            }
            else
            {
                filein = "players.csv";
            }

            if (argc > 3)
            {
                fileout = argv[3];
            }
            else
            {
                fileout = "output.csv";
            }
            runPlayerOptimizer(filein, fileout);
        }

        if (strcmp(argv[1], "optimizen") == 0)
        {
            string filein, fileout, lineupstart;
            if (argc > 2)
            {
                filein = argv[2];
            }
            else
            {
                filein = "players.csv";
            }

            if (argc > 3)
            {
                fileout = argv[3];
            }
            else
            {
                fileout = "output.csv";
            }

            if (argc > 4)
            {
                lineupstart = argv[4];
            }
            else
            {
                lineupstart = "";
            }
            runPlayerOptimizerN(filein, fileout, lineupstart);
        }

        if (strcmp(argv[1], "import") == 0)
        {
            string fileout;
            if (argc > 2)
            {
                fileout = argv[2];
            }
            else
            {
                fileout = "players.csv";
            }
            importProjections(fileout, false, false);
        }

        if (strcmp(argv[1], "lineupselect") == 0)
        {
            string lineups, players;
            if (argc > 2)
            {
                lineups = argv[2];
            }
            else
            {
                lineups = "output.csv";
            }

            if (argc > 3)
            {
                players = argv[3];
            }
            else
            {
                players = "players.csv";
            }
            // file pathing is bad in many of these, need to get more files as args.
            lineupSelector(lineups, players);
        }

        if (strcmp(argv[1], "dominateplayers") == 0)
        {
            string filein, fileout;
            if (argc > 2)
            {
                filein = argv[2];
            }
            else
            {
                filein = "mergedPlayers.csv";
            }

            if (argc > 3)
            {
                fileout = argv[3];
            }
            else
            {
                fileout = "players.csv";
            }
            removeDominatedPlayers(filein, fileout);
        }

        if (strcmp(argv[1], "ownershipgen") == 0)
        {
            string playersFile, ownershipFile;
            if (argc > 2)
            {
                playersFile = argv[2];
            }
            else
            {
                playersFile = "players.csv";
            }

            if (argc > 3)
            {
                ownershipFile = argv[3];
            }
            else
            {
                ownershipFile = "ownership.csv";
            }
            ownershipDriver(playersFile, ownershipFile);
        }


        if (strcmp(argv[1], "lineupselectownership") == 0)
        {
            string playersFile, ownershipFile;
            if (argc > 2)
            {
                playersFile = argv[2];
            }
            else
            {
                playersFile = "players.csv";
            }

            if (argc > 3)
            {
                ownershipFile = argv[3];
            }
            else
            {
                ownershipFile = "ownership.csv";
            }
            lineupSelectorOwnership(ownershipFile, playersFile);
        }

        if (strcmp(argv[1], "lse") == 0)
        {
            string playersFile, ownershipFile;
            if (argc > 2)
            {
                playersFile = argv[2];
            }
            else
            {
                playersFile = "players.csv";
            }

            if (argc > 3)
            {
                ownershipFile = argv[3];
            }
            else
            {
                ownershipFile = "ownership.csv";
            }
            lineupSelectorFilter(ownershipFile, playersFile);
        }

        if (strcmp(argv[1], "splituplineups") == 0)
        {
            splitLineups("outputset.csv");
        }

        if (strcmp(argv[1], "stochasticoptimizer") == 0)
        {
            stochasticOptimizer();
        }

        if (strcmp(argv[1], "determineownership") == 0)
        {
            determineOwnership();
        }
    }
    return 0;
}