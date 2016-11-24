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
        //float val;
        uint8_t cost;
        uint8_t pos;
        uint8_t index;
        Player(string s, uint8_t c, float p, uint8_t pos, uint8_t idx) : name(s), cost(c), proj(p), pos(pos), index(idx)
        {
            //val = proj / (float)cost;
        }
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

    void parsePlayerInfo()
    {
        vector<vector<Player>> allPlayers;
        allPlayers.reserve(numPositions);
        for (int i = 0; i < numPositions; i++)
        {
            vector<Player> players;
            allPlayers.push_back(players);
        }
        // 5 positions
    }

    

    // parse csv of player, cost, proj?
    // budget = 200
    // restraints = 1 qb, 2rb, 3wr, 1te, 1flx, 1def
    // for def could just do budget/2 for loose estimate
    // dp array of costs?
    // when we find a "max" need to find a "next" -> disallow a player?
    // iterate through original lineup - toggle players
    // playerid -> pos, index
    
    Players knapsack(int budget, vector<Player>& players)
    {
        // probably want arrays sorted by value
        // pick player, adjust remaining budget, current value
        // recurse
        // keep array of cost?
        // "current player list" could be array<int, 9>
        // keep "index" of each player, know how many of each player
        vector<vector<Players>> m(players.size() + 1);
        //m.reserve(players.size());
        for (int i = 0; i <= players.size(); i++)
        {
            m[i].reserve(budget + 1);
        }
        // TODO initialize vectors
        for (int j = 0; j <= budget; j++)
        {
            m[0].emplace_back();
        }
        //m[0].assign(budget + 1, Players());
        
        // iterate through all players, when we "add player" verify its valid
        for (int i = 1; i <= players.size(); i++)
        {
            Player& p = players[i - 1];
            // 0 -> 200
            for (int j = 0; j <= budget; j++)
            {
                // m [i, j] -> value, player list (best value using first i players, under j weight)
                if (p.cost > j)
                {
                    // player is too expensive so we can't add him
                    m[i].push_back(m[i - 1][j]);
                }
                else
                {
                    auto prev = m[i - 1][j - p.cost];
                    bool useNewVal = false;
                    // prev.second needs to describe list of players
                    if (prev.tryAddPlayer(p.pos, p.proj, i - 1))
                    {
                        // check player list of m[i - 1, j] if we can add
                        // check m[i - 1, j - i.cost] to see if we can add our player
                        // could make that part of tryAddPlayer
                        //prev.value = m[i - 1][j - p.cost].value + p.proj;
                        if (prev.value > m[i - 1][j].value)
                        {
                            m[i].push_back(prev);
                            //m[i][j] = prev;
                            useNewVal = true;
                        }
                    }
                    
                    if (!useNewVal)
                    {
                        m[i].push_back(m[i - 1][j]);
                        //m[i][j] = m[i - 1][j];
                    }
                }
            }
        }

        return m[players.size()][budget];
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

#define LINEUPCOUNT 10


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


int main(int argc, char* argv[]) {
    // for toggling lineups, we can assign "risk" to lower value plays
    // ie. quiz/gil are highest value, so less likely to toggle
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
    }
    return 0;
}