#pragma once
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
#include <future>
#define ASIO_MSVC _MSC_VER
#include "xorshift.h"
#include "test.h"
#include "parsing.h"
#include "Player.h"
#include "Players.h"

#include "lcg.h"
#include <cassert>
#include <cmath>
#include <mkl.h>
#include "asio.hpp"
#include "asio/use_future.hpp"
#include "server.h"

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

using namespace concurrency;
using namespace std;

static unordered_map<int, const vector<Player>> filtedFlex;

// can transpose players of same type
vector<Players2> knapsackPositionsN(int budget, int pos, const Players2 oldLineup, const vector<vector<Player>>& players, int rbStartPos, int wrStartPos, int skipPositionSet)
{
    while (skipPositionSet != 0 && (skipPositionSet & (1 << pos)) != 0)
    {
        skipPositionSet &= ~(1 << pos);
        pos++;
    }
    if (pos >= 9)
    {
        return vector<Players2>(1, oldLineup);
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
                return knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players, isRB ? (&p - &players[pos][0]) + 1 : rbStartPos, isWR ? (&p - &players[pos][0]) + 1 : wrStartPos, skipPositionSet);
            }
        }

        return vector<Players2>(1, currentLineup);
    };
    if (pos <= 2)
    {
        vector<vector<Players2>> lineupResults(players[pos].size() - startPos);
        parallel_transform(begin(players[pos]) + startPos, end(players[pos]), begin(lineupResults), loop);

        vector<Players2> merged;
        merged.reserve(LINEUPCOUNT * 2);
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
        vector<Players2> bestLineups;// (1, oldLineup);
        size_t targetSize;
        if (budget == 0)
        {
            targetSize = 3;
            bestLineups.reserve(targetSize);
        }
        else if (pos == 8)
        {
            targetSize = players[pos].size() + 1;
            bestLineups.reserve(targetSize);
        }

        //bestLineups.push_back(oldLineup);
        const vector<Player>* playersArray = &players[pos];
        if (pos == 7)
        {
            int index = rbStartPos * 256 + wrStartPos;
            auto it = filtedFlex.find(index);
            if (it != filtedFlex.end())
            {
                playersArray = &it->second;
            }
            else
            {
                index++;
            }
        }

        for (int i = startPos; i < playersArray->size(); i++)
        {
            const Player& p = (*playersArray)[i];
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
                        vector<Players2> lineups = knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players, isRB ? i + 1 : rbStartPos, isWR ? i + 1 : wrStartPos, skipPositionSet);
                        bestLineups.insert(bestLineups.end(), lineups.begin(), lineups.end());
                    }
                    // in place merge is much faster for larger sets
#if LINEUPCOUNT > 10
                    inplace_merge(bestLineups.begin(), bestLineups.begin() + originalLen, bestLineups.end());
#else
                    sort(bestLineups.begin(), bestLineups.end());
#endif
                    // pos == 8 should never exceed lineup count, and if it does thats fine
                    if (pos < 8)
                    {
                        bestLineups.erase(unique(bestLineups.begin(), bestLineups.end()), bestLineups.end());
                        if (bestLineups.size() > LINEUPCOUNT)
                        {
                            bestLineups.resize(LINEUPCOUNT);
                        }
                    }
                }
            }
        }
        return bestLineups;
    }
}

vector<Players2> generateLineupN(const vector<Player>& p, vector<string>& disallowedPlayers, Players2 currentPlayers, int budgetUsed, double& msTime)
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

    // for each starting rb and wr pos pair, create players table
    for (int i = 0; i <= playersByPos[1].size(); i++)
    {
        for (int j = 0; j <= playersByPos[3].size(); j++)
        {
            int index = i * 256 + j;
            vector<Player> flexPlayers;
            // add rbs from rb start pos i
            for (int z = i; z < playersByPos[1].size(); z++)
            {
                flexPlayers.push_back(playersByPos[1][z]);
            }
            // add wrs from wr start pos j
            for (int z = j; z < playersByPos[3].size(); z++)
            {
                flexPlayers.push_back(playersByPos[3][z]);
            }
            flexPlayers.insert(flexPlayers.end(), playersByPos[6].begin(), playersByPos[6].end());

            filtedFlex.emplace(index, flexPlayers);
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
    vector<Players2> output = knapsackPositionsN(100 - budgetUsed, 0, currentPlayers, playersByPos, 0, 0, skipPositionsSet);

    auto end = chrono::steady_clock::now();
    auto diff = end - start;
    msTime = chrono::duration <double, milli>(diff).count();
    return output;
}

void saveLineupList(vector<Player>& p, vector<Players2>& lineups, string fileout, double msTime)
{
    ofstream myfile;
    myfile.open(fileout);
    myfile << msTime << " ms" << endl;

    for (auto lineup : lineups)
    {
        int totalcost = 0;
        uint64_t bitset = lineup.bitset1;
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

        bitset = lineup.bitset2;
        for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
        {
            if (bitset & 1 == 1)
            {
                count++;
                myfile << p[64 + i].name;
                totalcost += p[64 + i].cost;
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
    vector<Players2> lineups = generateLineupN(p, vector<string>(), startingLineup, budgetUsed, msTime);
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
            vector<Players2> lineups = generateLineupN(p, excludedPlayers, startingLineup, budgetUsed, msTime);

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
        return fpros * .77 + yahoo * .13 + numberfire * .1;
    }
    else if (p.pos == 4)
    {
        return fpros * .4 + yahoo * .2 + numberfire * .4;
    }
    else
    {
        // for now just 70% yahoo, 30% fpros (fpros includes numberfire)
        return yahoo * .5 + fpros * .5;
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
            // gillislee
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

    ofstream allPlayers;
    allPlayers.open("allplayers.csv");
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

        for (auto& p : positionPlayers)
        {
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
        for (int j = 0; j < positionPlayers.size(); j++)
        {
            // remove all players with cost > player
            //auto& p = positionPlayers[j];
            auto it = remove_if(positionPlayers.begin() + j + 1, positionPlayers.end(), [&](Player& a) {

                // PositionCount
                static float minValue[5] = { 8, 7, 7, 5, 5 };
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
    allPlayers.close();
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

        alignas(256) array<float, 128> playerStandardNormals;
        normaldistf_boxmuller_avx(&playerStandardNormals[0], allPlayers.size(), lcg);
        array<float, 128> playerScores;
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

float runSimulationMaxWin(
    const float* standardNormals,
    const vector<vector<uint8_t>>& lineups,
    const vector<Player>& allPlayers,
    const float* projs, const float*  stdevs,
    //const float* corrCoefs, const array<float, SIMULATION_COUNT * 64>& corrOnes, const float* corrForty,
    const vector<uint8_t>& corrPairs,
    const vector<float>& corrCoeffs)
{
    int winningThresholdsHit = 0;
    /*
    unsigned seed2 = std::chrono::system_clock::now().time_since_epoch().count();
    const int n = 64* SIMULATION_COUNT;
    alignas(256) unique_ptr<float[]> standardNormals(new float[n]);
    VSLStreamStatePtr stream;
    vslNewStream(&stream, VSL_BRNG_SFMT19937, seed2);
    vsRngGaussian(VSL_RNG_METHOD_GAUSSIAN_BOXMULLER2,
        stream, n, &standardNormals[0], 0.0, 1.0);
    vslDeleteStream(&stream);*/

    // can just accumulate now
    //static vector<float> simulationResults(SIMULATION_COUNT);

    // vector math:
    // calc corr normals
    // do stddev * entire std normals
    const int len = SIMULATION_COUNT * allPlayers.size();
    //const array<float, len> corrCoefs; // 0, .916, 0, .916 ...
    //const array<float, len> corrOnes;
    //const array<float, len> corrForty; // init to .4

    //vector<uint8_t> corrPairs;
    //vector<float> corrCoeffs;
    // use corrPairs indices to create index array:
    // max qb len (32? 10?)
    //thread_local unique_ptr<float[]> zs(new float[len]);
    //memcpy(&zs[0], standardNormals, len * sizeof(float));

    for (int index = 0; index < SIMULATION_COUNT; index++)
    {
        const float* playerStandardNormals = &standardNormals[allPlayers.size() * index];
        /*for (int i = 1; i < corrPairs.size(); i += 2)
        {
            float z1 = playerStandardNormals[corrPairs[i - 1]] * corrCoeffs[i - 1];
            playerStandardNormals[corrPairs[i]] = playerStandardNormals[corrPairs[i]] * corrCoeffs[i] + z1;
        }*/
    //}
        array<float, 128> playerScores;
        vsMul(allPlayers.size(), stdevs, playerStandardNormals, &playerScores[0]);

        for (int i = 1; i < corrPairs.size(); i += 2)
        {
            float z1 = playerStandardNormals[corrPairs[i - 1]] * corrCoeffs[i - 1];
            playerScores[corrPairs[i]] = stdevs[i] * (playerStandardNormals[corrPairs[i]] * corrCoeffs[i] + z1);
        }

        vsAdd(allPlayers.size(), projs, &playerScores[0], &playerScores[0]);

    //vsMul(len, stdevs, &standardNormals[0], &standardNormals[0]);
    //vsAdd(len, projs, &standardNormals[0], &standardNormals[0]);

    //for (int index = 0; index < SIMULATION_COUNT; index++)
    //{
        //float* scores = &standardNormals[allPlayers.size() * index];
        int winnings = 0;
        for (auto& lineup : lineups)
        {
            float lineupScore = 0.f;
            for (auto player : lineup)
            {
                //lineupScore += playerScoresV[index * 64 + player];
                lineupScore += playerScores[player];
            }
            // only count > threshold
            if (lineupScore >= 170)
            {
                winnings = 1;
            }
        }
        winningThresholdsHit += winnings;
    }

    /*
    thread_local array<float, len> playerScoresV;
    vsMul(len, stdevs, standardNormals, &playerScoresV[0]);
    

    // array of .916 for each corr entry but 1 for non corr target entrys (unchanged)
    vsMul(len, &corrCoefs[0], &playerScoresV[0], &playerScoresV[0]);
    // mask standardNormals shifted - 1 with corr indexes results in corrScalars
    thread_local array<float, len> corrScalars = corrOnes; // init to 1 for each corr target
    vsMul(len - 1, standardNormals, &corrScalars[1], &corrScalars[1]); // results in normals shifted to their corr target

    // 0.4 * z1:
    vsMul(len, &corrForty[0], &corrScalars[0], &corrScalars[0]);
    // sd * result
    vsMul(len, stdevs, &corrScalars[0], &corrScalars[0]);
    // add values to original array
    vsAdd(len, &playerScoresV[0], &corrScalars[0], &playerScoresV[0]);

    vsAdd(len, projs, &playerScoresV[0], &playerScoresV[0]);*/


    // qb1 -> wr1, wr2, te, opp def
    // could have (pairs) (idx1, idx2) -> corr idx1 always qb's
    // corr40Arr = (qb, wr1), ...
    // corr30Arr = (qb, wr2), (qb, te1)..
    // this way we dont need to do the corr/swapping stuff either
    // better to just be adjacent array entries, pack unpack for vector?


    //for (int index = 0; index < SIMULATION_COUNT; index++)
    //{
    //    unique_ptr<float[]> playerStandardNormals(new float[allPlayers.size()]);
    //    memcpy(&playerStandardNormals[0], &standardNormals[allPlayers.size() * index], allPlayers.size() * sizeof(float));
    //    //const float* playerStandardNormals = &zs[allPlayers.size() * index];
    //    //alignas(256) array<float, 64> playerStandardNormals;
    //    //normaldistf_boxmuller_avx(&playerStandardNormals[0], 64, lcg);
    //    array<float, 128> playerScores;
    //    int i = 0;
    //    //for (i = 0; i < corrIdx; i++)
    //    //{
    //    //    const Player& p = allPlayers[i];
    //    //    // .4z1 + 0.91651513899 * z2 = correlated standard normal
    //    //    if (i % 2 == 0)
    //    //    {
    //    //        playerScores[i] = p.proj + (p.stdDev * playerStandardNormals[i]);
    //    //    }
    //    //    else
    //    //    {
    //    //        // p.proj + sd * .4 * other normal + sd * .9 * normal
    //    //        float corrZ = .4 * playerStandardNormals[i - 1] + 0.91651513899 * playerStandardNormals[i];
    //    //        playerScores[i] = p.proj + (p.stdDev * corrZ);
    //    //        //if (playerScores[i] != playerScoresV[index * 64 + i])
    //    //        //{
    //    //            //cout << playerScores[i] << "," << playerScoresV[index * 64 + i] << endl;
    //    //        //}
    //    //    }
    //    //}

    //    for (int i = 1; i < corrPairs.size(); i += 2)
    //    {
    //        float z1 = playerStandardNormals[corrPairs[i - 1]] * corrCoeffs[i - 1];
    //        playerScores[corrPairs[i]] = playerStandardNormals[corrPairs[i]] * corrCoeffs[i] + z1;
    //    }

    //    // assumes corrIdx < size
    //    //int len = allPlayers.size() - corrIdx;
    //    //vsMul(len, stdevs + corrIdx, playerStandardNormals + corrIdx, &playerScores[corrIdx]);
    //    //vsAdd(len, projs + corrIdx, &playerScores[corrIdx], &playerScores[corrIdx]);
    //    for (; i < allPlayers.size(); i++)
    //    {
    //        const Player& p = allPlayers[i];

    //        // playerscore should not go below 0? will that up winrate too high? probably favors cheap players
    //        playerScores[i] = p.proj + (p.stdDev * playerStandardNormals[i]);
    //        if (abs(playerScores[i] - zs[allPlayers.size() * index + i]) > 0.01)
    //        {
    //            cout << p.name << playerScores[i] << zs[allPlayers.size() * index + i] << endl;
    //        }
    //    }
    //    

    //    // keep track of times we win high placing since that excludes additional same placements
    //    // the problem with this is that generally we enter multiple contests, need to factor that in

    //    // create map of player -> generated score
    //    // for each lineup -> calculate score
    //    int winnings = 0;
    //    for (auto& lineup : lineups)
    //    {
    //        float lineupScore = 0.f;
    //        for (auto player : lineup)
    //        {
    //            //lineupScore += playerScoresV[index * 64 + player];
    //            lineupScore += playerScores[player];
    //        }
    //        // only count > threshold
    //        if (lineupScore >= 170)
    //        {
    //            winnings = 1;
    //        }
    //    }

    //    winningThresholdsHit += winnings;
    //}

    // we can calculate std dev per lineup and calculate risk of whole set
    //winningsTotal = accumulate(simulationResults.begin(), simulationResults.end(), 0.f);

    // is variation even useful here?
    float expectedValue = (float)winningThresholdsHit / (float)SIMULATION_COUNT;

    float stdDev = 1.f;
    return expectedValue;
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
        array<float, 128> playerScores;
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
    vector<vector<string>> originalOrder = allLineups;
    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed1);
    shuffle(allLineups.begin(), allLineups.end(), generator);

    vector<vector<string>> setA;
    vector<vector<string>> setB;

    int i = 0;
    partition_copy(allLineups.begin(), allLineups.end(), back_inserter(setA), back_inserter(setB), [&i](vector<string>& l)
    {
        bool setA = i++ < 10;
        return setA;
    });
    sort(setA.begin(), setA.end(), [&originalOrder](vector<string>& la, vector<string>& lb)
    {
        return find(originalOrder.begin(), originalOrder.end(), la) < find(originalOrder.begin(), originalOrder.end(), lb);
    });
    sort(setB.begin(), setB.end(), [&originalOrder](vector<string>& la, vector<string>& lb)
    {
        return find(originalOrder.begin(), originalOrder.end(), la) < find(originalOrder.begin(), originalOrder.end(), lb);
    });

    ofstream myfile;
    myfile.open("outputsetSplit.csv");
    for (auto& lineup : setA)
    {
        for (auto& x : lineup)
        {
            myfile << x;
            myfile << ",";
        }
        myfile << endl;
    }
    myfile << endl;
    myfile << endl;

    for (auto& lineup : setB)
    {
        for (auto& x : lineup)
        {
            myfile << x;
            myfile << ",";
        }
        myfile << endl;
    }

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

vector<string> enforceOwnershipLimits(vector<Player>& p, unordered_map<uint8_t, int>& playerCounts, 
    vector<pair<uint8_t, float>>& ownershipLimits, int numLineups, uint64_t& disallowedSet1, uint64_t& disallowedSet2)
{
    vector<string> playersToRemove;
    for (auto & x : ownershipLimits)
    {
        auto it = playerCounts.find(x.first);
        if (it != playerCounts.end())
        {
            float percentOwned = (float)it->second / (float)numLineups;
            if (percentOwned >= x.second)
            {
                if (x.first > 63)
                {
                    disallowedSet2 |= (uint64_t)1 << (x.first - 64);
                }
                else
                {
                    disallowedSet1 |= (uint64_t)1 << x.first;
                }
                playersToRemove.push_back(p[x.first].name);
            }
        }
    }

    for (auto & x : playerCounts)
    {
        float percentOwned = (float)x.second / (float)TARGET_LINEUP_COUNT;
        if (percentOwned > 0.25 && find_if(ownershipLimits.begin(), ownershipLimits.end(), [&x](pair<uint8_t, float>& z)
        {
            return z.first == x.first;
        }) == ownershipLimits.end())
        {
            if (x.first > 63)
            {
                disallowedSet2 |= (uint64_t)1 << (x.first - 64);
            }
            else
            {
                disallowedSet1 |= (uint64_t)1 << x.first;
            }
            playersToRemove.push_back(p[x.first].name);
        }
    }
    return playersToRemove;
}

constexpr int lineupChunkSize = 64;
int selectorCore(
    const vector<Player>& p,
    const vector<vector<uint8_t>>& allLineups,
    const vector<uint8_t>& corrPairs,
    const vector<float>& corrCoeffs,
    const array<float, 128>& projs, const array<float, 128>& stdevs,
    int lineupsIndexStart, int lineupsIndexEnd, // request data
    lineup_set& bestset    // request data
    )
{
    bestset.ev = 0.f;
    vector<int> lineupChunkStarts;
    for (int j = lineupsIndexStart; j < lineupsIndexEnd; j += lineupChunkSize)
    {
        lineupChunkStarts.push_back(j);
    }

    vector<lineup_set> chunkResults(lineupChunkStarts.size());
    parallel_transform(lineupChunkStarts.begin(), lineupChunkStarts.end(), chunkResults.begin(),
        [&allLineups, &p, &bestset, &projs, &stdevs, &corrPairs, &corrCoeffs](int lineupChunkStart)
    {
        static thread_local unique_ptr<float[]> standardNormals(new float[(size_t)p.size() * (size_t)SIMULATION_COUNT * (size_t)min((size_t)lineupChunkSize, allLineups.size())]);
        unsigned seed2 = std::chrono::system_clock::now().time_since_epoch().count();
        int currentLineupCount = min(lineupChunkSize, (int)(allLineups.size()) - lineupChunkStart);
        const size_t n = (size_t)p.size() * (size_t)SIMULATION_COUNT * (size_t)currentLineupCount;
        VSLStreamStatePtr stream;
        vslNewStream(&stream, VSL_BRNG_SFMT19937, seed2);
        int status = vsRngGaussian(VSL_RNG_METHOD_GAUSSIAN_BOXMULLER2,
            stream, n, &standardNormals[0], 0.0, 1.0);
        vslDeleteStream(&stream);

        int indexBegin = lineupChunkStart;
        int indexEnd = indexBegin + currentLineupCount;

        vector<lineup_set> results(currentLineupCount);
        transform(allLineups.begin() + indexBegin, allLineups.begin() + indexEnd, results.begin(),
            [&allLineups, &p, &bestset, &projs, &stdevs, &corrPairs, &corrCoeffs, indexBegin] (const vector<uint8_t>& lineup)
        {
            size_t index = &lineup - &allLineups[indexBegin];
            const int chunk = p.size() * SIMULATION_COUNT;
            lineup_set currentSet = bestset;
            currentSet.set.push_back(lineup);
            currentSet.ev = runSimulationMaxWin(
                &standardNormals[chunk * index],
                currentSet.set, p, &projs[0], &stdevs[0],
                corrPairs, corrCoeffs);
            return currentSet;
        });
        // comparator is backwards currently, can fix
        auto it = min_element(results.begin(), results.end());
        return *it;
    });
    auto itResult = min_element(chunkResults.begin(), chunkResults.end());
    bestset = *itResult;

    auto itLineups = find(allLineups.begin(), allLineups.end(), bestset.set[bestset.set.size() - 1]);
    return distance(allLineups.begin(), itLineups);
}

void greedyLineupSelector()
{
    vector<Player> p = parsePlayers("players.csv");
    vector<tuple<string, string, float>> corr = parseCorr("corr.csv");

    vector<pair<string, float>> ownership = parseOwnership("ownership.csv");

    vector<uint8_t> corrPairs;
    vector<float> corrCoeffs;
    //int corrIdx = 0;
    for (auto & s : corr)
    {
        // move those entries to the start of the array
        // only when we have the pair
        auto it = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == get<0>(s);
        });
        auto itC = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == get<1>(s);
        });
        if (it != p.end() && itC != p.end())
        {
            float r = get<2>(s);
            float zr = sqrt(1 - r*r);
            corrPairs.push_back(it->index);
            corrPairs.push_back(itC->index);
            corrCoeffs.push_back(r);
            corrCoeffs.push_back(zr);
        }
    }

    unordered_map<string, uint8_t> playerIndices;
    unordered_map<uint8_t, int> playerCounts;

    // vectorized projection and stddev data
    static array<float, 128> projs;
    static array<float, 128> stdevs;
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
        projs[x.index] = (x.proj);
        stdevs[x.index] = (x.stdDev);
    }

   /* for (int i = 1; i < SIMULATION_COUNT; i++)
    {
        for (int j = 0; j < p.size(); j++)
        {
            projs[i * p.size() + j] = projs[j];
            stdevs[i * p.size() + j] = stdevs[j];
        }
    }*/
    vector<pair<uint8_t, float>> ownershipLimits;
    for (auto& x : ownership)
    {
        auto it = playerIndices.find(x.first);
        if (it != playerIndices.end())
        {

            ownershipLimits.emplace_back(it->second, x.second);
        }
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    auto start = chrono::steady_clock::now();

    vector<vector<uint8_t>> allLineups = parseLineups("output.csv", playerIndices);


    uint64_t currentDisallowedSet1 = 0;
    uint64_t currentDisallowedSet2 = 0;
    vector<string> currentPlayersRemoved;

    vmlSetMode(VML_EP);

    ofstream myfile;
    myfile.open("outputset.csv");
    // choose lineup that maximizes objective
    // iteratively add next lineup that maximizes objective.
    lineup_set bestset;
    vector<int> bestsetIndex;

    int z = 0;
    for (int i = 0; i < TARGET_LINEUP_COUNT; i++)
    {
        int lineupsIndexStart = 0;
        int lineupsIndexEnd = allLineups.size();

        bestsetIndex.push_back(selectorCore(
            p,
            allLineups,
            corrPairs,
            corrCoeffs,
            projs, stdevs,
            lineupsIndexStart, lineupsIndexEnd, // request data
            bestset    // request data
        ));


        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        double msTime = chrono::duration <double, milli>(diff).count();
        cout << "Lineups: " << (i + 1) << " EV: " << bestset.ev << " elapsed time: " << msTime << endl;

        for (auto& x : bestset.set[bestset.set.size() - 1])
        {
            cout << p[x].name;
            cout << ",";
        }
        cout << endl;
        cout << endl;

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

        if (i > 1)
        {
            uint64_t disallowedSet1 = 0;
            uint64_t disallowedSet2 = 0;
            vector<string> playersToRemove = enforceOwnershipLimits(p, playerCounts, ownershipLimits, bestset.set.size(), disallowedSet1, disallowedSet2);

            if (disallowedSet1 != currentDisallowedSet1 || disallowedSet2 != currentDisallowedSet2)
            {
                cout << "Removing players: ";
                for (auto &s : playersToRemove)
                {
                    cout << s << ",";
                }
                cout << endl;
                cout << endl;
                array<uint64_t, 2> disSets = { disallowedSet1 , disallowedSet2 };
                //disallowedPlayersToLineupSet.emplace(disSets, allLineups);
                currentPlayersRemoved = playersToRemove;
                currentDisallowedSet1 = disallowedSet1;
                currentDisallowedSet2 = disallowedSet2;
                double msTime = 0;

                {
                    vector<Players2> lineups = generateLineupN(p, playersToRemove, Players2(), 0, msTime);
                    // faster to parse vector<Players2> to allLineups
                    allLineups.clear();
                    for (auto& lineup : lineups)
                    {
                        uint64_t bitset = lineup.bitset1;
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
                        bitset = lineup.bitset2;
                        for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
                        {
                            if (bitset & 1 == 1)
                            {
                                count++;
                                currentLineup.push_back((uint8_t)i + 64);
                            }
                            bitset = bitset >> 1;
                        }
                        allLineups.push_back(currentLineup);
                    }
                }
            }
        }
    }

    cout << endl;

    auto end = chrono::steady_clock::now();
    auto diff = end - start;
    double msTime = chrono::duration <double, milli>(diff).count();

    // output bestset
    myfile << msTime << "ms" << endl;
    myfile << bestset.ev;
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

void distributedLineupSelector()
{
    // names are probably wrong
    // master runs base algo and calls out to "worker"
    // for now, master is BUNN, worker is ANBUNN5
    using namespace asio;

    io_service io_service;
    io_service::work work(io_service);
    std::thread thread([&io_service]() { io_service.run(); });

    //tcp::socket s(io_service);
    //tcp::resolver resolver(io_service);
    udp::resolver resolver(io_service);
    udp::socket socket(io_service, udp::v4());

    std::future<udp::resolver::iterator> iterUdp =
            resolver.async_resolve(
        { udp::v4(), "ANBUNN5", "9000" },
                asio::use_future);

    vector<Player> p = parsePlayers("players.csv");
    vector<tuple<string, string, float>> corr = parseCorr("corr.csv");

    vector<pair<string, float>> ownership = parseOwnership("ownership.csv");

    vector<uint8_t> corrPairs;
    vector<float> corrCoeffs;
    for (auto & s : corr)
    {
        // move those entries to the start of the array
        // only when we have the pair
        auto it = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == get<0>(s);
        });
        auto itC = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == get<1>(s);
        });
        if (it != p.end() && itC != p.end())
        {
            float r = get<2>(s);
            float zr = sqrt(1 - r*r);
            corrPairs.push_back(it->index);
            corrPairs.push_back(itC->index);
            corrCoeffs.push_back(r);
            corrCoeffs.push_back(zr);
        }
    }

    unordered_map<string, uint8_t> playerIndices;
    unordered_map<uint8_t, int> playerCounts;

    // vectorized projection and stddev data
    static array<float, 128> projs;
    static array<float, 128> stdevs;
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
        projs[x.index] = (x.proj);
        stdevs[x.index] = (x.stdDev);
    }
    vector<pair<uint8_t, float>> ownershipLimits;
    for (auto& x : ownership)
    {
        auto it = playerIndices.find(x.first);
        if (it != playerIndices.end())
        {

            ownershipLimits.emplace_back(it->second, x.second);
        }
    }

    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    auto start = chrono::steady_clock::now();

    int totalSets = 2;// NUM_ITERATIONS_OWNERSHIP;// ownership.size();
    vector<vector<uint8_t>> allLineups = parseLineups("output.csv", playerIndices);
    // copy output to sharedoutput for initial runs
    {
        ifstream source("output.csv", ios::binary);
        ofstream dest("\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio 2013\\Projects\\dfs\\progproblems\\sharedoutput.csv", ios::binary);

        dest << source.rdbuf();

        source.close();
        dest.close();
    }

    /*vector<vector<vector<uint8_t>>> allLineups(totalSets);
    for (int i = 0; i < totalSets; i++)
    {
        ostringstream stream;
        stream << "output-" << i << ".csv";
        allLineups[i] = parseLineups(stream.str(), playerIndices);
    }*/

   
    uint64_t currentDisallowedSet1 = 0;
    uint64_t currentDisallowedSet2 = 0;
    vector<string> currentPlayersRemoved;
    //unordered_map<array<uint64_t, 2>, vector<vector<uint8_t>>, set128_hash> disallowedPlayersToLineupSet;
    
    vmlSetMode(VML_EP);
    /*
    const float corrCoef = 0.91651513899f;
    const float corrForty = 0.4f;
    static array<float, len> corrCoefs; // 0, .916, 0, .916 ...
    static array<float, len> corrOnes;
    static array<float, len> corrFortys; // init to .4
    for (int i = 0; i < len; i++)
    {
        if (i % 64 < corrIdx && i % 2 == 1)
        {
            corrCoefs[i] = corrCoef;
            corrOnes[i] = 1.f;
            corrFortys[i] = 0.4f;
        }
        else
        {
            corrCoefs[i] = 1.f;
        }
        projs[i] = projs[i % 64];
        stdevs[i] = stdevs[i % 64];
    }*/

    ofstream myfile;
    myfile.open("outputset.csv");
    // choose lineup that maximizes objective
    // iteratively add next lineup that maximizes objective.
    lineup_set bestset;
    vector<int> bestsetIndex;

    udp::endpoint endpoint_ = *iterUdp.get();

    int z = 0;
    for (int i = 0; i < TARGET_LINEUP_COUNT; i++)
    {
        int lineupsIndexStart = 0;
        int lineupsIndexEnd = allLineups.size();

        array<char, max_length> recv_buf = {};
        // just support 2 for now
        // ANBUNN5 is faster so if that's server give it bigger load:
        int distributedLineupStart = lineupsIndexEnd = (int)((double)allLineups.size() * .45);
        int distributedLineupEnd = allLineups.size();
        // convert start, end, bestset to request
        // write async, get result?
        array<char, max_length> bestsetIndices = {};
        char* currI = &bestsetIndices[0];
        int setLen = 0;
        if (bestset.set.size() > 0)
        {
            for (auto& x : bestset.set[bestset.set.size() - 1])
            {
                sprintf(currI, "%d,", x);
                currI = strchr(currI, ',') + 1;
                setLen++;
            }
        }

        array<char, max_length> request_buf;
        // just send last lineup, keep state on server, tell server to "reset"
        snprintf(&request_buf[0], request_buf.size(), "select %d %d %d: %s", distributedLineupStart, distributedLineupEnd, setLen, &bestsetIndices[0]);
            
        std::future<std::size_t> send_length =
            socket.async_send_to(asio::buffer(request_buf),
                endpoint_,
                asio::use_future);

        send_length.get();

        udp::endpoint sender_endpoint;
        future<size_t> recv_length =
            socket.async_receive_from(
                asio::buffer(recv_buf),
                sender_endpoint,
                asio::use_future);


        bestsetIndex.push_back(selectorCore(
            p,
            allLineups,
            corrPairs,
            corrCoeffs,
            projs, stdevs,
            lineupsIndexStart, lineupsIndexEnd, // request data
            bestset    // request data
        ));

        size_t result = recv_length.get();
        if (result > 0)
        {
            int resultIndex;
            float resultEV;
            sscanf(&recv_buf[0], "%d %f", &resultIndex, &resultEV);

            // update bestset
            if (resultEV > bestset.ev)
            {
                bestsetIndex[bestsetIndex.size() - 1] = resultIndex;
                bestset.set[bestset.set.size() - 1] = allLineups[resultIndex];
                bestset.ev = resultEV;
            }
        }

        auto end = chrono::steady_clock::now();
        auto diff = end - start;
        double msTime = chrono::duration <double, milli>(diff).count();
        cout << "Lineups: "<< (i+1) << " EV: " << bestset.ev << " elapsed time: " << msTime << endl;

        for (auto& x : bestset.set[bestset.set.size() - 1])
        {
            cout << p[x].name;
            cout << ",";
        }
        cout << endl;
        cout << endl;

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

        if (i > 1)
        {
            uint64_t disallowedSet1 = 0;
            uint64_t disallowedSet2 = 0;
            vector<string> playersToRemove = enforceOwnershipLimits(p, playerCounts, ownershipLimits, bestset.set.size(), disallowedSet1, disallowedSet2);

            if (disallowedSet1 != currentDisallowedSet1 || disallowedSet2 != currentDisallowedSet2)
            {
                cout << "Removing players: ";
                for (auto &s : playersToRemove)
                {
                    cout << s << ",";
                }
                cout << endl;
                cout << endl;
                array<uint64_t, 2> disSets = { disallowedSet1 , disallowedSet2 };
                //disallowedPlayersToLineupSet.emplace(disSets, allLineups);
                currentPlayersRemoved = playersToRemove;
                currentDisallowedSet1 = disallowedSet1;
                currentDisallowedSet2 = disallowedSet2;
                double msTime = 0;
                bool processedDistributedOptimizer = false;
                // distributed:

                // can "remove" half of qbs for each machine for now
                vector<string> playersToRemoveDistributed = playersToRemove;
                vector<string> qbs;
                for (auto& pl : p)
                {
                    auto it = find(playersToRemove.begin(), playersToRemove.end(), pl.name);
                    if (it == playersToRemove.end())
                    {
                        if (pl.pos == 0)
                        {
                            qbs.push_back(pl.name);
                        }
                    }
                }

                // for now, dont bother distributed if we cant split qbs
                if (qbs.size() > 1
                    && processedDistributedOptimizer)
                {
                    cout << "Multiple qbs to distribute" << endl;
                    int distIndexStart = (int)(qbs.size() * .45);
                    int distIndexEnd = qbs.size();
                    for (int q = 0; q < qbs.size(); q++)
                    {
                        // distributed removes first half of array, we remove second half
                        if (q < distIndexStart)
                        {
                            playersToRemoveDistributed.push_back(qbs[q]);
                        }
                        else
                        {
                            playersToRemove.push_back(qbs[q]);
                        }
                    }

                    array<char, max_length> playersToRemoveIndicies = {};
                    currI = &playersToRemoveIndicies[0];
                    for (auto& s : playersToRemoveDistributed)
                    {
                        sprintf(currI, "%d,", playerIndices[s]);
                        currI = strchr(currI, ',') + 1;
                    }
                    snprintf(&request_buf[0], request_buf.size(), "optimize %d: %s", playersToRemoveDistributed.size(), &playersToRemoveIndicies[0]);


                    future<std::size_t> sendOptimizeLength =
                        socket.async_send_to(asio::buffer(request_buf),
                            endpoint_,
                            asio::use_future);

                    sendOptimizeLength.get();

                    udp::endpoint senderOptimize_endpoint;
                    future<size_t> recvOptimizelength =
                        socket.async_receive_from(
                            asio::buffer(recv_buf),
                            senderOptimize_endpoint,
                            asio::use_future);

                    vector<Players2> lineups = generateLineupN(p, playersToRemove, Players2(), 0, msTime);
                    if (recvOptimizelength.get() > 0)
                    {
                        cout << "Got response from server." << endl;
                        vector<Players2> distributedLineups = parseLineupsData("\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio 2013\\Projects\\dfs\\progproblems\\sharedlineups.csv");
                        // both lists are sorted
                        // merge lineups
                        lineups.insert(lineups.end(), distributedLineups.begin(), distributedLineups.end());
                        sort(lineups.begin(), lineups.end());
                        lineups.resize(LINEUPCOUNT);
                        saveLineupList(p, lineups, "\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio 2013\\Projects\\dfs\\progproblems\\sharedoutput.csv", msTime);


                        allLineups.clear();
                        for (auto& lineup : lineups)
                        {
                            uint64_t bitset = lineup.bitset1;
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
                            bitset = lineup.bitset2;
                            for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
                            {
                                if (bitset & 1 == 1)
                                {
                                    count++;
                                    currentLineup.push_back((uint8_t)i + 64);
                                }
                                bitset = bitset >> 1;
                            }
                            allLineups.push_back(currentLineup);
                        }

                        processedDistributedOptimizer = true;
                    }
                }
                
                if (!processedDistributedOptimizer)
                {
                    vector<Players2> lineups = generateLineupN(p, playersToRemove, Players2(), 0, msTime);
                    // faster to parse vector<Players2> to allLineups
                    allLineups.clear();
                    for (auto& lineup : lineups)
                    {
                        uint64_t bitset = lineup.bitset1;
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
                        bitset = lineup.bitset2;
                        for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
                        {
                            if (bitset & 1 == 1)
                            {
                                count++;
                                currentLineup.push_back((uint8_t)i + 64);
                            }
                            bitset = bitset >> 1;
                        }
                        allLineups.push_back(currentLineup);
                    }
                    saveLineupList(p, lineups, "\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio 2013\\Projects\\dfs\\progproblems\\sharedoutput.csv", msTime);
                }
            }
        }
    }

    cout << endl;

    auto end = chrono::steady_clock::now();
    auto diff = end - start;
    double msTime = chrono::duration <double, milli>(diff).count();

    // output bestset
    myfile << msTime << "ms" << endl;
    myfile << bestset.ev;
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

    io_service.stop();
    thread.join();
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
    allLineups = parseLineupString("outputset.csv");
    //vector<vector<string>> allLineups;
    //allLineups = parseLineupSet("outputsetsharpe-final.csv");
    //allLineups = parseLineupSet(filename);
    unordered_map<string, float> results = parseProjections("playerResults.csv");
    vector<float> scores;
    for (auto & lineup : allLineups)
    {
        float score = 0.f;
        for (auto & name : lineup)
        {
            auto it = results.find(name);
            if (it != results.end())
            {
                score += it->second;
            }
        }
        scores.push_back(score);
    }

    
    {
        ofstream myfile;
        myfile.open("outputset-scores.csv");
        int i = 0;
        for (auto& lineup : allLineups)
        {
            for (auto & name : lineup)
            {
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

void distributedSelectWorker()
{
    vector<Player> p = parsePlayers("players.csv");
    vector<tuple<string, string, float>> corr = parseCorr("corr.csv");

    vector<pair<string, float>> ownership = parseOwnership("ownership.csv");

    vector<uint8_t> corrPairs;
    vector<float> corrCoeffs;
    for (auto & s : corr)
    {
        // move those entries to the start of the array
        // only when we have the pair
        auto it = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == get<0>(s);
        });
        auto itC = find_if(p.begin(), p.end(), [&s](Player& p)
        {
            return p.name == get<1>(s);
        });
        if (it != p.end() && itC != p.end())
        {
            float r = get<2>(s);
            float zr = sqrt(1 - r*r);
            corrPairs.push_back(it->index);
            corrPairs.push_back(itC->index);
            corrCoeffs.push_back(r);
            corrCoeffs.push_back(zr);
        }
    }

    unordered_map<string, uint8_t> playerIndices;

    // vectorized projection and stddev data
    static array<float, 128> projs;
    static array<float, 128> stdevs;
    for (auto& x : p)
    {
        playerIndices.emplace(x.name, x.index);
        projs[x.index] = (x.proj);
        stdevs[x.index] = (x.stdDev);
    }

    

    using namespace asio;
    io_service io_service;

    static server s(io_service, 9000,
        p,
        playerIndices,
        corrPairs,
        corrCoeffs,
        projs,
        stdevs
        );

    io_service.run();
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

        if (strcmp(argv[1], "runmaster") == 0)
        {
            distributedLineupSelector();
        }

        if (strcmp(argv[1], "runchild") == 0)
        {
            distributedSelectWorker();
        }
    }
    return 0;
}