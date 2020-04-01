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
#include <functional>
#include <random>
#include <cstdint>
#include <chrono>
#include <ppl.h>
#include <numeric>
#include <bitset>
#include <future>
#define ASIO_MSVC _MSC_VER
#include "test.h"
#include "parsing.h"
#include "Player.h"
#include "Players.h"

#include <cassert>
#include <cmath>
#include <mkl.h>
#include "asio.hpp"
#include "asio/use_future.hpp"
#include "server.h"

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


float mixPlayerProjections(Player& p, float numberfire, float fpros, float yahoo)
{
    if (p.pos == 0)
    {
        // for QB: probably use close to even average of yahoo, cbs, stats, numberfire
        // could ignore espn when dling data?
        return fpros;
    }
    else if (p.pos == 4)
    {
        return fpros;
        //return fpros * 2 - numberfire;
    }
    else
    {
        // for now just 70% yahoo, 30% fpros (fpros includes numberfire)
        //return yahoo * .3 + fpros * 1 + numberfire * -.3;
        return fpros * 2 - numberfire;
    }
}

void removeDominatedPlayersProjFile()
{
    unordered_map<string, float> stds;

    for (int i = 0; i <= 4; i++)
    {
        ostringstream stream;
        stream << "std" << i;
        stream << ".csv";
        unordered_map<string, float> std = parseProjections(stream.str());
        for (auto &x : std)
        {
            stds.emplace(x.first, x.second);
        }
    }
    vector<tuple<string, int, int>> costs = parseCosts("costs.csv");
    //unordered_map<string, float> numfire = parseProjections("projs.csv");

    unordered_map<string, float> fpros = parseProsStats();

    vector<tuple<string, int, float, int, float>> playersResult;
    vector<float> sdevs;
    for (auto& p : fpros)
    {
        if (p.first == "cleveland")
        {
            continue;
        }

        {
            auto it = find_if(costs.begin(), costs.end(), [&](tuple<string, int, int> &x)
            {
                return get<0>(x) == p.first;
            });
            if (it != costs.end())
            {
                int pos = get<1>(*it);
                float proj = p.second;
                
                float sdev = 0.f;
                auto itnfstd = stds.find(p.first);
                if (itnfstd != stds.end())
                {
                    sdev = itnfstd->second;
                }
                playersResult.emplace_back(p.first, pos, proj, get<2>(*it), sdev);
                //sdevs.push_back(sdev);
            }
        }
    }

    ofstream myfile;
    myfile.open("players.csv");
    // for a slot, if there is a player cheaper cost but > epsilon score, ignore
    // no def for now?
    for (int i = 0; i <= 4; i++)
    {
        vector<tuple<string, int, float, int, float>> positionPlayers;
        copy_if(playersResult.begin(), playersResult.end(), back_inserter(positionPlayers), [i](tuple<string, int, float, int, float>& p) {
            return (get<3>(p) == i);
        });

        // sort by value, descending
        sort(positionPlayers.begin(), positionPlayers.end(), [](tuple<string, int, float, int, float>& a, tuple<string, int, float, int, float>& b) { return get<2>(a) > get<2>(b); });


        // biggest issue is for rb/wr we dont account for how many we can use.
        for (int j = 0; j < positionPlayers.size(); j++)
        {
            // remove all players with cost > player
            //auto& p = positionPlayers[j];
            auto it = remove_if(positionPlayers.begin() + j + 1, positionPlayers.end(), [&](tuple<string, int, float, int, float>& a) {

                // PositionCount
                static float minValue[5] = { 8, 8, 8, 5, 5 };
                return get<2>(a) < minValue[i] ||
                    (count_if(positionPlayers.begin(), positionPlayers.begin() + j + 1, [&](tuple<string, int, float, int, float>& p) {
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
            myfile << get<4>(p);
            myfile << ",";

            myfile << endl;
        }
    }
    myfile.close();
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
        //auto ity = yahoo.find(p.name);

        // linear reg models here:

        // players we dont want to include can just have large negative diff
        if (itpros != fpros.end() /*&& ity != yahoo.end() || (itpros != fpros.end() && (p.pos != 4 && p.pos != 0))*/)
        {
            if (p.name == "david johnson" && p.pos == 3)
            {
                p.proj = 0.f;
            }
            else
            {
                p.proj = mixPlayerProjections(p, p.proj, itpros->second, /*ity != yahoo.end() ? ity->second :*/ 0);
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

float runSimulationMaxWin(
    const float* standardNormals,
    const vector<vector<uint8_t>>& lineups,
    const vector<Player>& allPlayers,
    const float* projs, const float*  stdevs,
    const vector<uint8_t>& corrPairs,
    const vector<float>& corrCoeffs)
{
    int winningThresholdsHit = 0;


    // vector math:
    // calc corr normals
    // do stddev * entire std normals
    const int len = SIMULATION_COUNT * allPlayers.size();


    for (int index = 0; index < SIMULATION_COUNT; index++)
    {
        const float* playerStandardNormals = &standardNormals[allPlayers.size() * index];
        array<float, 128> playerScores;
        vsMul(allPlayers.size(), stdevs, playerStandardNormals, &playerScores[0]);

        for (int i = 1; i < corrPairs.size(); i += 2)
        {
            float z1 = playerStandardNormals[corrPairs[i - 1]] * corrCoeffs[i - 1];
            playerScores[corrPairs[i]] = stdevs[i] * (playerStandardNormals[corrPairs[i]] * corrCoeffs[i] + z1);
        }

        vsAdd(allPlayers.size(), projs, &playerScores[0], &playerScores[0]);

        int winnings = 0;
        for (auto& lineup : lineups)
        {
            float lineupScore = 0.f;
            for (auto player : lineup)
            {
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

    // is variation even useful here?
    float expectedValue = (float)winningThresholdsHit / (float)SIMULATION_COUNT;

    float stdDev = 1.f;
    return expectedValue;
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
        bool setA = i++ < 7;
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

vector<string> enforceOwnershipLimits(vector<Player>& p, array<int, 256>& playerCounts, 
    vector<pair<uint8_t, float>>& ownershipLimits, int numLineups, uint64_t& disallowedSet1, uint64_t& disallowedSet2)
{
    vector<string> playersToRemove;
    for (auto & x : ownershipLimits)
    {
        float percentOwned = (float)playerCounts[x.first] / (float)numLineups;
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
    for (uint16_t i = 0; i < playerCounts.size(); i++)
    {
        if (playerCounts[i]) {
            float percentOwned = (float)playerCounts[i] / (float)numLineups;
            if (percentOwned > 0.6 && find_if(ownershipLimits.begin(), ownershipLimits.end(), [&x, i](pair<uint8_t, float>& z)
                {
                    return z.first == i;
                }) == ownershipLimits.end())
            {
                if (i > 63)
                {
                    disallowedSet2 |= (uint64_t)1 << (i - 64);
                }
                else
                {
                    disallowedSet1 |= (uint64_t)1 << i;
                }
                playersToRemove.push_back(p[i].name);
            }
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
    array<int, 256> playerCounts{};

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
    bestsetIndex.reserve(TARGET_LINEUP_COUNT);

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
            playerCounts[x]++;
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
    array<int, 256> playerCounts{};

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
            playerCounts[x]++;
        }

        if (i > 0)
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
                    )//&& processedDistributedOptimizer)
                {
                    cout << "Multiple qbs to distribute" << endl;
                    //int distIndexStart = (int)(qbs.size() * .5);
                    //int distIndexEnd = qbs.size();
                    for (int q = 0; q < qbs.size(); q++)
                    {
                        // alternate removing qbs since top end qbs process faster
                        // if we want to change distribution from 50/50, may need probability
                        if (q % 2 == 1 && q > 1)
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
                        vector<Players2> distributedLineups = parseLineupsData("\\\\ANBUNN5\\Users\\andrewbunn\\dfs\\progproblems\\sharedlineups.csv");
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
            removeDominatedPlayersProjFile();
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