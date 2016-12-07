#include "parsing.h"
#include <algorithm>
#include <array>

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

vector<pair<string, string>> parseCorr(string filename)
{
    vector<pair<string, string>> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    int count = 0;
    while (tokens.size() == 2)
    {
        result.emplace_back(tokens[0], tokens[1]);
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
    while (tokens.size() >= 5)
    {
        int c = atoi(tokens[1].c_str()) - 10;
        float p = (float)atof(tokens[2].c_str());
        int pos = atoi(tokens[3].c_str());
        float sd = (float)atof(tokens[4].c_str());
        if (pos == 0)
        {
            c -= 10;
        }
        if (tokens.size() == 5)
        {
            result.emplace_back(tokens[0], c, p, pos, count++, sd);
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

    while (tokens.size() >= 1)
    {
        if (tokens[0].size() > 0)
        {
            if (isalpha(tokens[0][0]))
            {
                result.push_back(tokens);
            }
            else
            {
            }
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return result;
}

vector<vector<string>> parseLineupSet(const string filename)
{
    vector<vector<string>> result;
    ifstream       file(filename);
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    vector<string> current;

    while (tokens.size() >= 1)
    {
        if (tokens[0].size() > 0)
        {
            if (isalpha(tokens[0][0]))
            {
                result.push_back(tokens);
                current.clear();
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

    auto i = find_if(dsts.begin(), dsts.end(), [&name](string& n) {
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
    while (tokens.size() >= 2)
    {
        float proj;
        if (tokens.size() == 2)
        {
            proj = stof(tokens[1]);
        }
        else
        {
            proj = stof(tokens[2]);
        }
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