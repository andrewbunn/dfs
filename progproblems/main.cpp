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
#include "xorshift.h"
#include "test.h"
#include "parsing.h"
#include "Player.h"
#include "Players.h"

#include "lcg.h"
#include <cassert>
#include <cmath>

__m256 xorshift128plus_avx2(__m256i &state0, __m256i &state1)
{
    __m256i s1 = state0;
    const __m256i s0 = state1;
    state0 = s0;
    s1 = _mm256_xor_si256(s1, _mm256_slli_epi64(s1, 23));
    state1 = _mm256_xor_si256(_mm256_xor_si256(_mm256_xor_si256(s1, s0),
        _mm256_srli_epi64(s1, 18)),
        _mm256_srli_epi64(s0, 5));

    __m256i u = _mm256_add_epi64(state1, s0);
    __m256 f = _mm256_sub_ps(_mm256_castsi256_ps(u), *(__m256*)_ps256_1);
    return f;
    //return _mm256_add_epi64(state1, s0);
}

static void normaldistf_boxmuller_avx(float* data, size_t count, LCG<__m256>& r) {
    assert(count % 16 == 0);
    const __m256 twopi = _mm256_set1_ps(2.0f * 3.14159265358979323846f);
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 minustwo = _mm256_set1_ps(-2.0f);

    for (size_t i = 0; i < count; i += 16) {
        __m256 u1 = _mm256_sub_ps(one, r()); // [0, 1) -> (0, 1]
        __m256 u2 = r();
        __m256 radius = _mm256_sqrt_ps(_mm256_mul_ps(minustwo, log256_ps(u1)));
        __m256 theta = _mm256_mul_ps(twopi, u2);
        __m256 sintheta, costheta;
        sincos256_ps(theta, &sintheta, &costheta);
        _mm256_store_ps(&data[i], _mm256_mul_ps(radius, costheta));
        _mm256_store_ps(&data[i + 8], _mm256_mul_ps(radius, sintheta));
    }
}

// number of lineups to generate in optimizen - TODO make parameter
#define LINEUPCOUNT 300000
// number of simulations to run of a set of lineups to determine expected value
#define SIMULATION_COUNT 20000
// number of random lineup sets to select
#define RANDOM_SET_COUNT 10000
// number of lineups we want to select from total pool
#define TARGET_LINEUP_COUNT 50
// number of pools to generate
#define NUM_ITERATIONS_OWNERSHIP 100
#define STOCHASTIC_OPTIMIZER_RUNS 50

using namespace concurrency;
using namespace std;

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
            merged.erase(unique(merged.begin(), merged.end()), merged.end());
            if (merged.size() > LINEUPCOUNT)
            {
                merged.resize(LINEUPCOUNT);
            }
        }

        return merged;
    }
    else
    {
        lineup_list bestLineups(1, oldLineup);
        for (int i = startPos; i < players[pos].size(); i++)
        {
            const Player& p = players[pos][i];
            Players2 currentLineup = oldLineup;
            if (p.cost <= budget)
            {
                if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
                {
                    const int originalLen = bestLineups.size();
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
                    // in place merge is much faster for larger sets
#if LINEUPCOUNT > 10
                    inplace_merge(bestLineups.begin(), bestLineups.begin() + originalLen, bestLineups.end());
#else
                    sort(bestLineups.begin(), bestLineups.end());
#endif
                    bestLineups.erase(unique(bestLineups.begin(), bestLineups.end()), bestLineups.end());
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
    // if we partition players by position, 

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
                // output which set we're processing
                cout << pl << ",";
                if (!startingLineup.tryAddPlayer(it->pos, it->proj, it->index))
                {
                    valid = false;
                    i--;
                    break;
                }
                budgetUsed += it->cost;
                if (budgetUsed >= 100)
                {
                    valid = false;
                    i--;
                    break;
                }
            }
        }
        if (!valid)
        {
            cout << "invalid";
            cout << endl;
            continue;
        }

        {
            double msTime = 0;
            lineup_list lineups = generateLineupN(p, excludedPlayers, startingLineup, budgetUsed, msTime);

            // skip over lineups where
            if (lineups[0].totalCount < 9 || lineups[0].value < 121)
            {
                cout << "low value: " << lineups[0].value << "," << (int)lineups[0].totalCount;
                cout << endl;
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

float mixPlayerProjections(Player& p, float numberfire, float fpros, float yahoo)
{
    if (p.pos == 0)
    {
        // for QB: probably use close to even average of yahoo, cbs, stats, numberfire
        // could ignore espn when dling data?
        return fpros * .8333 + yahoo * .1666;
    }
    else
    {
        // for now just 70% yahoo, 30% fpros (fpros includes numberfire)
        return yahoo * .7 + fpros * .3;
    }
}

void removeDominatedPlayers(string filein, string fileout)
{
    vector<Player> players = parsePlayers(filein);

    unordered_map<string, float> yahoo = parseYahooStats();
    unordered_map<string, float> fpros = parseProsStats();
    cout << "Mixing projections" << endl;
    for (auto& p : players)
    {
        auto itpros = fpros.find(p.name);
        auto ity = yahoo.find(p.name);

        // linear reg models here:

        // players we dont want to include can just have large negative diff
        if (itpros != fpros.end() && ity != yahoo.end())
        {
            if (p.name == "david johnson" && p.pos == 3)
            {
                p.proj = 0.f;
            }
            else
            {
                p.proj = mixPlayerProjections(p, p.proj, itpros->second, ity->second);
            }
        }
        else
        {
            cout << p.name << endl;
            if (p.pos != 4)
            {
                // ignore player if it wasnt high on yahoo/pros?
                p.proj = 0.f; 
            }
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
                }) >= PositionCount[i]);
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
            myfile << static_cast<float>(p.stdDev);
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
template <typename T>
inline float generateScore(uint8_t player, const vector<Player>& allPlayers, T& generator)
{
    const Player& p = allPlayers[player];
    normal_distribution<float> distribution(p.proj, p.stdDev);

    // .4z1 + 0.91651513899 * z2 = correlated standard normal
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
// customized deviations - non-normal distributions
// account for contest types/costs/prizes
// estimate thresholds specific to a given week? (how much obvious value is out there?)
//
// So we generate large chunk of lineups using one set of player limitations/projections
// then generate other sets, how do we incorporate those together?
// we could enforce % of lineups selected from each of those sets but how they would get selected/weighted ultimately depends
// on projections we use in simulation
// then we need more data to drive our "asset mix"
//
// then in simulation, we can track things like variance of a set as well and select "best mix" based on more than just EV.
//
// other ways to improve: estimate cutoffs more accurately for a given week based on ownership? and then the random values we generate for a simulation affect those lines based on ownership
// perf: lineups.size() as template arg?
// template optimize fn
pair<float, float> runSimulation(const vector<vector<uint8_t>>& lineups, const vector<Player>& allPlayers, const int corrIdx)
{
    float winningsTotal = 0.f;
    float runningAvg = 0.f;
    bool converged = false;
    static vector<float> simulationResults(SIMULATION_COUNT);
    parallel_transform(begin(simulationResults), end(simulationResults), begin(simulationResults), [&lineups, &allPlayers, corrIdx](const float&)
    {
        static thread_local unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
        static thread_local random_device rdev;
        static thread_local LCG<__m256> lcg(seed1, rdev(), rdev(), rdev(), rdev(), rdev(), rdev(), rdev());

        alignas(256) array<float, 64> playerStandardNormals;
        normaldistf_boxmuller_avx(&playerStandardNormals[0], 64, lcg);
        array<float, 64> playerScores;
        int i;
        for (i = 0; i < corrIdx; i++)
        {
            const Player& p = allPlayers[i];
            // .4z1 + 0.91651513899 * z2 = correlated standard normal
            if (i % 2 == 0)
            {
                playerScores[i] = p.proj + (p.stdDev * playerStandardNormals[i]);
            }
            else
            {
                float corrZ = .4 * playerStandardNormals[i- 1] + 0.91651513899 * playerStandardNormals[i];
                playerScores[i] = p.proj + (p.stdDev * corrZ);
            }
            
            if (playerScores[i] < 0)
            {
                playerScores[i] = 0.f;
            }
        }
        for (; i < allPlayers.size(); i++)
        {
            const Player& p = allPlayers[i];

            // playerscore should not go below 0? will that up winrate too high? probably favors cheap players
            playerScores[i] = p.proj + (p.stdDev * playerStandardNormals[i]);
            if (playerScores[i] < 0)
            {
                playerScores[i] = 0.f;
            }
        }
        // keep track of times we win high placing since that excludes additional same placements
        // the problem with this is that generally we enter multiple contests, need to factor that in

        array<uint8_t, CONTENDED_PLACEMENT_SLOTS> placementCount = {};
        // create map of player -> generated score
        // for each lineup -> calculate score
        float winnings = 0.f;
        for (auto& lineup : lineups)
        {
            float lineupScore = 0.f;
            for (auto player : lineup)
            {
                lineupScore += playerScores[player];
            }
            // lookup score -> winnings
            // need to account for my own entries for winnings
            winnings += determineWinnings(lineupScore, placementCount);
        }
        return winnings;
    });

    // we can calculate std dev per lineup and calculate risk of whole set
    winningsTotal = accumulate(simulationResults.begin(), simulationResults.end(), 0.f);

    // calculate std dev here:
    // sqrt (1/n * [(val - mean)^2 + ]
    float expectedValue = winningsTotal / SIMULATION_COUNT;
    transform(simulationResults.begin(), simulationResults.end(), simulationResults.begin(), [&expectedValue, &lineups](float& val)
    {
        // variation below target:
        float diff = val - 10 * lineups.size();  //(val - expectedValue);
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

pair<float, float> runSimulationMaxWin(const vector<vector<uint8_t>>& lineups, const vector<Player>& allPlayers, const int corrIdx)
{
    float winningsTotal = 0.f;
    float runningAvg = 0.f;
    bool converged = false;
    static vector<float> simulationResults(SIMULATION_COUNT);
    parallel_transform(begin(simulationResults), end(simulationResults), begin(simulationResults), [&lineups, &allPlayers, corrIdx](const float&)
    {
        static thread_local unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
        static thread_local random_device rdev;
        static thread_local LCG<__m256> lcg(seed1, rdev(), rdev(), rdev(), rdev(), rdev(), rdev(), rdev());

        alignas(256) array<float, 64> playerStandardNormals;
        normaldistf_boxmuller_avx(&playerStandardNormals[0], 64, lcg);
        array<float, 64> playerScores;
        int i;
        for (i = 0; i < corrIdx; i++)
        {
            const Player& p = allPlayers[i];
            // .4z1 + 0.91651513899 * z2 = correlated standard normal
            if (i % 2 == 0)
            {
                playerScores[i] = p.proj + (p.stdDev * playerStandardNormals[i]);
            }
            else
            {
                float corrZ = .4 * playerStandardNormals[i - 1] + 0.91651513899 * playerStandardNormals[i];
                playerScores[i] = p.proj + (p.stdDev * corrZ);
            }

            if (playerScores[i] < 0)
            {
                playerScores[i] = 0.f;
            }
        }
        for (; i < allPlayers.size(); i++)
        {
            const Player& p = allPlayers[i];

            // playerscore should not go below 0? will that up winrate too high? probably favors cheap players
            playerScores[i] = p.proj + (p.stdDev * playerStandardNormals[i]);
            if (playerScores[i] < 0)
            {
                playerScores[i] = 0.f;
            }
        }
        // keep track of times we win high placing since that excludes additional same placements
        // the problem with this is that generally we enter multiple contests, need to factor that in

        array<uint8_t, CONTENDED_PLACEMENT_SLOTS> placementCount = {};
        // create map of player -> generated score
        // for each lineup -> calculate score
        float winnings = 0.f;
        for (auto& lineup : lineups)
        {
            float lineupScore = 0.f;
            for (auto player : lineup)
            {
                lineupScore += playerScores[player];
            }
            // only count > threshold
            if (lineupScore >= 171.14)
            {
                winnings = 1;
            }
        }
        return winnings;
    });

    // we can calculate std dev per lineup and calculate risk of whole set
    winningsTotal = accumulate(simulationResults.begin(), simulationResults.end(), 0.f);

    // is variation even useful here?
    float expectedValue = winningsTotal / SIMULATION_COUNT;

    float stdDev = 1.f;

    return make_pair(expectedValue, stdDev);
}

pair<float, float> runSimulationSlow(const vector<vector<uint8_t>>& lineups, const vector<Player>& allPlayers)
{
    float winningsTotal = 0.f;
    float runningAvg = 0.f;
    bool converged = false;
    static vector<float> simulationResults(SIMULATION_COUNT);
    parallel_transform(begin(simulationResults), end(simulationResults), begin(simulationResults), [&lineups, &allPlayers](const float&)
    {
        static thread_local unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
        static thread_local mt19937 generatorTh(seed1);
        array<float, 64> playerScores;
        for (int i = 0; i < allPlayers.size(); i++)
        {
            playerScores[i] = generateScore(i, allPlayers, generatorTh);
        }

        array<uint8_t, CONTENDED_PLACEMENT_SLOTS> placementCount = {};
        float winnings = 0.f;
        for (auto& lineup : lineups)
        {
            float lineupScore = 0.f;
            for (auto& player : lineup)
            {
                lineupScore += playerScores[player];
            }
            winnings += determineWinnings(lineupScore, placementCount);
        }
        return winnings;
    });
    winningsTotal = accumulate(simulationResults.begin(), simulationResults.end(), 0.f);
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
    
    // satisfy ownership first then pull from last set
    // so for marshall pull from marshal sets at random?
    // easier is just have sets specified in ownership
    // marshall set, 
    // for now i can manually do it with output files or something
    // 
    //int totalsets = NUM_ITERATIONS_OWNERSHIP;
    ////int totalsets = STOCHASTIC_OPTIMIZER_RUNS;
    //static uniform_int_distribution<int> distribution(0, totalsets - 1);
    ////static uniform_int_distribution<int> distributionLineup(0, LINEUPCOUNT - 1);
    //while (currentIndex < TARGET_LINEUP_COUNT)
    //{
    //    int i = distribution(generator);
    //    auto& targetLineups = allLineups[i];
    //    uniform_int_distribution<int> distributionLineup(0, targetLineups.size() - 1);
    //    int j = distributionLineup(generator);
    //    set[currentIndex] = targetLineups[j];
    //    //copy_n(targetLineups.begin() + j, 1, set.begin() + currentIndex);
    //    currentIndex += 1;
    //}

    static uniform_real_distribution<float> distribution(0.f, 1.f);
    for (int i = 0; i < NUM_ITERATIONS_OWNERSHIP && currentIndex < TARGET_LINEUP_COUNT; i++)
    {
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
        return (ev - (10 * set.size())) / stdev;
    }
    lineup_set() : ev(0.f), stdev(1.f) {}
    lineup_set(vector<vector<uint8_t>>& s) : ev(0.f), stdev(1.f), set(s) {}
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

lineup_set lineupSelectorOwnership(const string ownershipFile, const string playersFile)
{
    vector<Player> p = parsePlayers(playersFile);
    // create map of player -> index for lineup parser
    unordered_map<string, uint8_t> playerIndices;
    // for now just use same std dev
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);

    int totalSets = NUM_ITERATIONS_OWNERSHIP;// ownership.size();

    vector<vector<vector<uint8_t>>> allLineups(totalSets);
    for (int i = 0; i < totalSets; i++)
    {
        ostringstream stream;
        stream << "output-" << i << ".csv";
        allLineups[i] = parseLineups(stream.str(), playerIndices);
    }

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
        lineup_set set(getTargetSet(allLineups, generator));
        tie(set.ev, set.stdev) = runSimulation(set.set, p, 0);

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
            else
            {
                playerCounts.emplace(p[x].name, 1);
            }
        }
    }

    {
        ofstream myfile;
        myfile.open("ownershipResults.csv");
        for (auto& i : playerCounts)
        {
            myfile << i.first << ",";
            myfile << ((float)i.second / (float)TARGET_LINEUP_COUNT) << endl;
        }
    }

    return bestSharpeResults[0];
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
        if (i == 14)
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
    // create map of player -> index for lineup parser
    unordered_map<string, uint8_t> playerIndices;
    // for now just use same std dev
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();

    auto start = chrono::steady_clock::now();

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
    lineup_set bestSet;
    bestSet.ev = -100000.f;
    for (int i = 0; i < RANDOM_SET_COUNT; i++)
    {
        lineup_set set(getRandomSet(starterSet, allLineups, generator));
        tie(set.ev, set.stdev) = runSimulation(set.set, p, 0);
        if (set.getSharpe() > bestSet.getSharpe())
        {
            bestSet = set;
            cout << set.ev << ", " << set.getSharpe() << endl;
        }
    }

    auto end = chrono::steady_clock::now();
    auto diff = end - start;
    double msTime = chrono::duration <double, milli>(diff).count();

    // output bestset
    ofstream myfile;
    myfile.open("outputset.csv");
    myfile << msTime << "ms" << endl;
    myfile << bestSet.ev << "," << bestSet.getSharpe();
    myfile << endl;
    for (auto& lineup : bestSet.set)
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

void greedyLineupSelector()
{
    vector<Player> p = parsePlayers("players.csv");
    vector<pair<string, string>> corr = parseCorr("corr.csv");

    vector<pair<string, float>> ownership = parseOwnership("ownership.csv");

    int corrIdx = 0;
    for (auto & s : corr)
    {
        // move those entries to the start of the array
        // only when we have the pair
        auto it = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == s.first;
        });
        auto itC = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == s.second;
        });
        if (it != p.end() && itC != p.end())
        {
            swap(p[corrIdx].index, it->index);
            iter_swap(it, p.begin() + corrIdx++);
            swap(p[corrIdx].index, itC->index);
            iter_swap(itC, p.begin() + corrIdx++);
        }
    }

    unordered_map<string, uint8_t> playerIndices;
    unordered_map<uint8_t, int> playerCounts;
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
    }
    vector<pair<uint8_t, float>> ownershipLimits;
    for (auto& x : ownership)
    {
        ownershipLimits.emplace_back(playerIndices.find(x.first)->second, x.second);
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    auto start = chrono::steady_clock::now();

    int totalSets = 2;// NUM_ITERATIONS_OWNERSHIP;// ownership.size();
    vector<vector<uint8_t>> allLineups = parseLineups("output.csv", playerIndices);

    /*vector<vector<vector<uint8_t>>> allLineups(totalSets);
    for (int i = 0; i < totalSets; i++)
    {
        ostringstream stream;
        stream << "output-" << i << ".csv";
        allLineups[i] = parseLineups(stream.str(), playerIndices);
    }*/
    

    ofstream myfile;
    myfile.open("outputset.csv");
    // choose lineup that maximizes objective
    // iteratively add next lineup that maximizes objective.
    lineup_set bestset;
    int z = 0;
    for (int i = 0; i < TARGET_LINEUP_COUNT; i++)
    {
        lineup_set set = bestset;
        bestset.ev = 0.f; // reset so we accept new lineup
        //for (auto & lineupbucket : allLineups)
        {
            //for (auto & lineup : lineupbucket)
            for (auto & lineup : allLineups)
            {
                set.set.push_back(lineup);
                tie(set.ev, set.stdev) = runSimulationMaxWin(set.set, p, corrIdx);
                //tie(set.ev, set.stdev) = runSimulation(set.set, p, corrIdx);
                if (set.ev > bestset.ev)
                //if (set.getSharpe() > bestset.getSharpe())
                {
                    bestset = set;
                }
                set.set.erase(set.set.end() - 1);
            }
        }
        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        double msTime = chrono::duration <double, milli>(diff).count();
        cout << "Lineups: "<< (i+1) << " EV: " << bestset.ev << ", sortino: " << bestset.getSharpe() << " elapsed time: " << msTime << endl;

        // rather than "enforced ownership" we should just have ownership caps
        // eg. DJ @ 60%, after player exceeds threshold, we can rerun optimizen, and work with new player set
        for (auto x : bestset.set[bestset.set.size() - 1])
        {
            auto it = playerCounts.find(x);
            if (it == playerCounts.end())
            {
                playerCounts.emplace(x, 1);
            }
            else
            {
                it->second++;
            }
        }

        vector<string> playersToRemove;
        for (auto & x : ownershipLimits)
        {
            auto it = playerCounts.find(x.first);
            if (it != playerCounts.end())
            {
                float percentOwned = (float)it->second / (float)TARGET_LINEUP_COUNT;
                if (percentOwned >= x.second)
                {
                    playersToRemove.push_back(p[x.first].name);
                }
            }
        }

        if (playersToRemove.size() > 0)
        {
            double msTime = 0;
            lineup_list lineups = generateLineupN(p, playersToRemove, Players2(), 0, msTime);
            // faster to parse lineup_list to allLineups
            allLineups.clear();
            for (auto& lineup : lineups)
            {
                uint64_t bitset = lineup.bitset;
                int count = 0;
                vector<uint8_t> currentLineup;
                for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
                {
                    if (bitset & 1 == 1)
                    {
                        count++;
                        currentLineup.push_back((uint8_t)i);
                    }
                    bitset = bitset >> 1;
                }
                allLineups.push_back(currentLineup);
            }
        }

        for (auto& x : bestset.set[bestset.set.size() - 1])
        {
            cout << p[x].name;
            cout << ",";
        }
        cout << endl;
    }

    cout << endl;

    auto end = chrono::steady_clock::now();
    auto diff = end - start;
    double msTime = chrono::duration <double, milli>(diff).count();

    // output bestset
    myfile << msTime << "ms" << endl;
    myfile << bestset.ev << "," << bestset.getSharpe();
    myfile << endl;
    for (auto& lineup : bestset.set)
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

lineup_set determineOwnership()
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
            if (x.second < allLineups.size() * .36)
            //if (x.second < .25 * allLineups.size())
            {
                break;
            }
            // sqrt(x/5)*6
            // 100 -> 66?
            // 50 -> 33?
            // 30 -> 30?
            float valPercent = (float)x.second * 100.f / (float)allLineups.size();
            float targetPercent = sqrtf(valPercent) * 6 / 100.f;
            //float targetPercent = .66f * (float)x.second / (float)allLineups.size();
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
    int iterations = 5;
    lineup_set bestset;
    bestset.ev = 0;
    bestset.stdev = 500000.f;

    for (int i = 0; i < iterations; i++)
    {
        // create ownershipfile
        ownershipDriver(playersFile, ownership);
        lineup_set set = lineupSelectorOwnership(ownership, playersFile);
        if (set.getSharpe() > bestset.getSharpe())
        {
            bestset = set;
        }
        else
        {
            break;
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
                it->second = (it->second + ((float)x.second / (float)TARGET_LINEUP_COUNT)) / 2.0f;
            }
            else
            {
                // how we generate % probably depends on iteration
                if (x.second > (float)TARGET_LINEUP_COUNT * (25.f / 100.f))
                {
                    // sqrt(x/5)*6
                    // 100 -> 66?
                    // 50 -> 33?
                    // 30 -> 30?
                    float valPercent = (float)x.second * 100.f / (float)TARGET_LINEUP_COUNT;
                    float targetPercent = sqrtf(valPercent) * 5 / 100.f;

                    //float targetPercent = .66f * (float)x.second / (float)TARGET_LINEUP_COUNT;
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
    return bestset;
}

void superDriver()
{
    string playersFile = "players.csv";
    vector<Player> p = parsePlayers(playersFile);

    lineup_set bestset;
    bestset.ev = 0;
    bestset.stdev = 50000.f;
    for (int i = 0; i < 5; i++)
    {
        lineup_set set = determineOwnership();
        if (set.getSharpe() > bestset.getSharpe())
        {
            bestset = set;
        }
    }
    {
        ofstream myfile;
        myfile.open("outputsetsharpe-final.csv");

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

void evaluateScore(string filename)
{
    vector<vector<string>> allLineups;
    //allLineups = parseLineupSet("outputsetsharpe-final.csv");
    allLineups = parseLineupSet(filename);
    unordered_map<string, float> results = parseProjections("playerResults.csv");
    vector<float> scores;
    for (auto & lineup : allLineups)
    {
        float score = 0.f;
        for (auto & name : lineup)
        {
            score += results.find(name)->second;
        }
        scores.push_back(score);
    }

    array<uint8_t, CONTENDED_PLACEMENT_SLOTS> placementCount = {};
    float winnings = 0.f;
    for (auto &score : scores)
    {
        // need actual winnings tables
        winnings += determineWinnings(score, placementCount);
    }

    cout << winnings << endl;
}

int main(int argc, char* argv[]) {
    if (argc > 1)
    {
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

        if (strcmp(argv[1], "splituplineups") == 0)
        {
            splitLineups("outputset.csv");
        }

        if (strcmp(argv[1], "determineownership") == 0)
        {
            determineOwnership();
        }

        if (strcmp(argv[1], "sd") == 0)
        {
            superDriver();
        }

        if (strcmp(argv[1], "eval") == 0)
        {
            string playersFile;
            if (argc > 2)
            {
                playersFile = argv[2];
            }
            else
            {
                playersFile = "outputsetsharpe.csv";
            }
            evaluateScore(playersFile);
        }

        if (strcmp(argv[1], "greedyselect") == 0)
        {
            greedyLineupSelector();
        }

        if (strcmp(argv[1], "parsehistproj") == 0)
        {
            parseHistoricalProjFiles();
        }

        if (strcmp(argv[1], "parsefpros") == 0)
        {
            parseProsStats();
        }

    }
    return 0;
}