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

// number of lineups to generate in optimizen - TODO make parameter
#define LINEUPCOUNT 2000
// number of simulations to run of a set of lineups to determine expected value
#define SIMULATION_COUNT 500
// number of random lineup sets to select
#define RANDOM_SET_COUNT 10000
// number of lineups we want to select from total pool
#define TARGET_LINEUP_COUNT 50

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
lineup_list knapsackPositionsN(int budget, int pos, const Players2 oldLineup, const vector<vector<Player>>& players, int rbStartPos, int wrStartPos)
{
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
                return knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players, isRB ? (&p - &players[pos][0]) + 1 : 0, isWR ? (&p - &players[pos][0]) + 1 : 0);
            }
        }

        return lineup_list(1, currentLineup);
    };
    if (pos <= 2)
    {
        vector<lineup_list> lineupResults(players[pos].size());
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
                        lineup_list lineups = knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players, isRB ? i + 1 : 0, isWR ? i + 1 : 0);
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

lineup_list generateLineupN(vector<Player>& p)
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

    return knapsackPositionsN(100, 0, Players2(), playersByPos, 0, 0);
}

vector<int> toggleLowValuePlayers(vector<Player>& p, Players2 lineup)
{
    vector<int> result;
    int totalcost = 0;
    uint64_t bitset = lineup.bitset;
    int count = 0;
    float worstValue = 100.f;
    int worstValueIndex = 0;
    for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
    {
        if (bitset & 1 == 1)
        {
            count++;
            //result.push_back(p[i]);
            if (p[i].cost > 0)
            {
                float value = p[i].proj / p[i].cost;
                if (value < worstValue)
                {
                    worstValue = value;
                    worstValueIndex = i;
                }
            }
        }
        bitset = bitset >> 1;
    }

    result.push_back(worstValueIndex);
    return result;
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

void runPlayerOptimizerN(string filein, string fileout)
{
    vector<Player> p = parsePlayers(filein);
    auto start = chrono::steady_clock::now();

    //Players2 lineup = generateLineup(p);
    lineup_list lineups = generateLineupN(p);
    auto end = chrono::steady_clock::now();
    auto diff = end - start;

        ofstream myfile;
        myfile.open(fileout);
        myfile << chrono::duration <double, milli>(diff).count() << " ms" << endl;

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

    array<string, 28> dsts = {
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
            "Pittsburgh"
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
                    //swap(get<2>(positionPlayers[j], get<2>(positionPlayers[j + 1]);
                }

                //swap(positionPlayers[j], positionPlayers[j + 1]);
                //rotate(positionPlayers.begin() + i, it, it + 1);
                //get<2>(positionPlayers[i]) = proj;
            }
        }
    }

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
                }) >= PositionCount[i]);
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

inline float generateScore(uint8_t player, vector<PlayerSim>& allPlayers, default_random_engine& generator)
{
    PlayerSim& p = allPlayers[player];
    // need to research what std dev should be
    return p.distribution(generator);
}

// cutoffs is sorted array of the average score for each prize cutoff
// alternatively we can do regression to have function of value -> winnings
inline float determineWinnings(float score/*, vector<float>& winningsCutoffs, vector<float>& winningsValues*/)
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
    // binary search array of cutoffs
    auto it = lower_bound(cutoffs.begin(), cutoffs.end(), score, greater<float>());
    float value;
    if (it != cutoffs.end())
    {
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
float runSimulation(vector<vector<uint8_t>>& lineups, vector<PlayerSim>& allPlayers,
    default_random_engine& generator
    /*, vector<float>& winningsCutoffs, vector<float>& winningsValues*/
)
{
    float winningsTotal = 0.f;
    float runningAvg = 0.f;
    bool converged = false;
    // vector of all players in all lineups
    // is it faster to create vector here or just create map of scores lazily from full map of players?
    for (int i = 0; i < SIMULATION_COUNT; i++)
    {
        // only 64 players considered? need universal index
        // map of index -> score
        array<float, 64> playerScores = {};
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
                    playerScores[player] = generateScore(player, allPlayers, generator);
                }
                lineupScore += playerScores[player];
            }
            // lookup score -> winnings
            winnings += determineWinnings(lineupScore/*, winningsCutoffs, winningsValues*/);
        }
        winningsTotal += winnings;

        // dbg:
        /*
        float currentAvg = winningsTotal / (i + 1);
        if (abs(currentAvg - runningAvg) < 20)
        {
            if (!converged)
            {
                cout << i;
            }
            //break;
            converged = true;
        }
        else
        {
            if (converged)
            {
                cout << "not ready";
                cout << i;
            }
            converged = false;
        }
        runningAvg = currentAvg;
        */
    }

    float expectedValue = winningsTotal / SIMULATION_COUNT;
    return expectedValue;
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

void lineupSelector(const string lineupsFile, const string playersFile)
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
        float value = runSimulation(set, allPlayers, generator);
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
            runPlayerOptimizerN(filein, fileout);
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
            string filein, fileout;
            if (argc > 2)
            {
                filein = argv[2];
            }
            else
            {
                filein = "output.csv";
            }

            if (argc > 3)
            {
                fileout = argv[3];
            }
            else
            {
                fileout = "players.csv";
            }
            lineupSelector(filein, fileout);
        }
    }
    return 0;
}