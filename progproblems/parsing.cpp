#include "parsing.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <numeric>

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
    // will fuller v but dont want all v names
    //static string v = " v";

    it = find_end(name.begin(), name.end(), sr.begin(), sr.end());
    name.resize(distance(name.begin(), it));

    it = find_end(name.begin(), name.end(), jr.begin(), jr.end());
    name.resize(distance(name.begin(), it));

    static array<string, 32> dsts = {
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
        if (*i == "washington" && name[0] == 'd')
        {
            // rb dwayne/deandre
        }
        else
        {
            name = *i;
        }
    }

    if (name == "robert kelley")
    {
        name = "rob kelley";
    }
    else if (name == "will fuller v")
    {
        name == "will fuller";
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

#define POSFILTER(pos) ((pos) == 0)
unordered_map<string, float> parseFantasyResults()
{
    ifstream file("playersHistorical.csv");
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);
    // skip first line
    tokens = getNextLineAndSplitIntoTokens(file);
    unordered_map<string, float> results;
    while (tokens.size() == 5)
    {
        normalizeName(tokens[0]);
        int pos = stoi(tokens[1]);

        if (POSFILTER(pos))
        {
            int wk = stoi(tokens[3]);
            int yr = stoi(tokens[2]);
            ostringstream id;
            id << tokens[0] << "," << pos << "," << setfill('0') << setw(2) << wk << "," << yr;
            results.emplace(id.str(), stof(tokens[4]));
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    return results;
}


void parseYahooProjFiles(unordered_map<string, float>& performanceProjections)
{
    string location = "..\\ffaprojs\\";
    // basically want, player, week, projection
    array<int, 4> weeks = { 2,6,8,10 };
    // pos data?
    // basically we want a "performance id" = (player, week, year) -> proj
    for (int i : weeks)
    {
        ostringstream stream;
        stream << location << "yahoo-wk" << setfill('0') << setw(2) << i;
        stream << ".csv";
        ifstream file(stream.str());
        vector<string> tokens = getNextLineAndSplitIntoTokens(file);
        tokens = getNextLineAndSplitIntoTokens(file); // skip first line
        while (tokens.size() > 8)
        {
            normalizeName(tokens[2]);
            int pos = 0;
            if (tokens[5] == "\"QB\"")
            {
                pos = 0;
            }
            else if (tokens[5] == "\"RB\"")
            {
                pos = 1;
            }
            else if (tokens[5] == "\"WR\"")
            {
                pos = 2;
            }
            else if (tokens[5] == "\"TE\"")
            {
                pos = 3;
            }
            if (POSFILTER(pos))
            {
                ostringstream id;
                id << tokens[2] << "," << pos << "," << setfill('0') << setw(2) << i << "," << "2016";
                auto it = copy_if(tokens[8].begin(), tokens[8].end(), tokens[8].begin(), [](char c)
                {
                    return c != '\"';
                });
                tokens[8].resize(distance(tokens[8].begin(), it));
                performanceProjections.emplace(id.str(), stof(tokens[8]));
            }
            tokens = getNextLineAndSplitIntoTokens(file);
        }
    }

    cout << performanceProjections.size() << endl;
}

void parseNFLProjFiles(unordered_map<string, float>& performanceProjections)
{
    string location = "..\\ffaprojs\\";
    // basically want, player, week, projection
    array<int, 1> weeks = { 10 };
    // pos data?
    // basically we want a "performance id" = (player, week, year) -> proj
    for (int i : weeks)
    {
        ostringstream stream;
        stream << location << "nfl-wk" << setfill('0') << setw(2) << i;
        stream << ".csv";
        ifstream file(stream.str());
        vector<string> tokens = getNextLineAndSplitIntoTokens(file);
        tokens = getNextLineAndSplitIntoTokens(file); // skip first line
        while (tokens.size() > 8)
        {
            normalizeName(tokens[2]);
            int pos = 0;
            if (tokens[5] == "\"QB\"")
            {
                pos = 0;
            }
            else if (tokens[5] == "\"RB\"")
            {
                pos = 1;
            }
            else if (tokens[5] == "\"WR\"")
            {
                pos = 2;
            }
            else if (tokens[5] == "\"TE\"")
            {
                pos = 3;
            }

            if (POSFILTER(pos))
            {
                ostringstream id;
                id << tokens[2] << "," << pos << "," << setfill('0') << setw(2) << i << "," << "2016";
                auto it = copy_if(tokens[8].begin(), tokens[8].end(), tokens[8].begin(), [](char c)
                {
                    return c != '\"';
                });
                tokens[8].resize(distance(tokens[8].begin(), it));
                performanceProjections.emplace(id.str(), stof(tokens[8]));
            }
            tokens = getNextLineAndSplitIntoTokens(file);
        }
    }

    cout << performanceProjections.size() << endl;
}

void parseNumberFireProjFiles(unordered_map<string, float>& performanceProjections)
{
    string location = "..\\numberfireprojs\\";
    // basically want, player, week, projection
    const int weeks = 13;
    // pos data?
    // basically we want a "performance id" = (player, week, year) -> proj
    for (int i = 1; i <= weeks; i++)
    {
        ostringstream stream;
        stream << location << "w" << setfill('0') << setw(2) << i;
        stream << ".csv";
        ifstream file(stream.str());
        vector<string> tokens = getNextLineAndSplitIntoTokens(file);
        while (tokens.size() == 3)
        {
            normalizeName(tokens[0]);
            int pos = stoi(tokens[1]);

            if (POSFILTER(pos))
            {
                ostringstream id;
                id << tokens[0] << "," << pos << "," << setfill('0') << setw(2) << i << "," << "2016";
                performanceProjections.emplace(id.str(), stof(tokens[2]));
            }
            tokens = getNextLineAndSplitIntoTokens(file);
        }
    }

    cout << performanceProjections.size() << endl;
}

int getSourceId(array<string, 8>& sources, string source)
{
    auto i = find_if(sources.begin(), sources.end(), [&source](string& n) {
        return source.find(n) != string::npos;
    });

    if (i == sources.end())
    {
        //cout << source;
        return -1;
    }
    else
    {
        return distance(sources.begin(), i);
    }
}

void parseHistoricalProjFiles()
{
    // fantasypros = .25 * yahoo + .25 * espn + .25 * cbs + .25 * STATS
    // just input STATS into linear model
    static array<string, 8> sources = {
        "espn",
        //"fftoday",
        "yahoo",
        "numberfire",
        //"nfl",
        //"sharks",
        "cbs",
        "fantasypros",
        //"nerd"
    };
    ifstream file("playerProjections.csv");
    vector<string> tokens = getNextLineAndSplitIntoTokens(file);

    vector<unordered_map<string, float>> histProjections(sources.size());
    parseYahooProjFiles(histProjections[1]);
    parseNFLProjFiles(histProjections[3]);
    while (tokens.size() == 6)
    {
        normalizeName(tokens[0]);
        int pos = stoi(tokens[1]);
        int wk = stoi(tokens[3]);
        // dont have real data for wk 14 yet
        // skip TE for now
        if (wk < 14 && POSFILTER(pos))
        {
            int yr = stoi(tokens[2]);
            ostringstream id;
            id << tokens[0] << "," << pos << "," << setfill('0') << setw(2) << wk << "," << yr;
            int source = getSourceId(sources, tokens[5]);
            if (source >= 0)
            {
                // may also want to "prune" projections here
                histProjections[source].emplace(id.str(), stof(tokens[4]));
            }
        }
        tokens = getNextLineAndSplitIntoTokens(file);
    }

    // nerd size so small, probably unusable
    int numberfireTable = 2;
    int ESPNTable = 0;
    int yahooTable = 1;
    int cbsTable = 3;
    parseNumberFireProjFiles(histProjections[numberfireTable]);
    cout << histProjections.size() << endl;

    unordered_map<string, float> results = parseFantasyResults();

    // yahoo missing week 8 2016?
    // create tables for those who have entry in each source
    int startingTable = 4;//7; // fpros has lower entries
    int datasetSize = startingTable + 1 + 1; // tables including startingtable and results
    unordered_map<string, float>& prosTable = histProjections[startingTable];
    vector<pair<string, vector<float>>> dataTable;
    for (auto & entry : prosTable)
    {
        vector<float> allProjs;
        for (int i = 0; i < startingTable; i++)
        {
            auto it = histProjections[i].find(entry.first);
            if (it == histProjections[i].end())
            {
                // can't use this data point
                if (entry.second >= 5)
                {
                    cout << entry.first << ": " << sources[i] << endl;
                }
                break;
            }
            else
            {
                allProjs.push_back(it->second);
            }
        }

        if (allProjs.size() == datasetSize - 2)
        {
            // apparently fpros is actually: cbs, espn, numberfire, stats, fftoday
            // need fftoday to split out separate
            float STATSval = entry.second;
            allProjs.push_back(STATSval);
            float avg = accumulate(allProjs.begin(), allProjs.end(), 0.f) / (float)allProjs.size();
            auto it = results.find(entry.first);
            if (it != results.end() && avg >= 4.5f)
            {
                allProjs.push_back(it->second);
                // Normalize data for svr:

                dataTable.emplace_back(entry.first, allProjs);
            }
            else
            {
                cout << entry.first << ": no fantasy result data" << endl;
            }
        }
        
    }

    cout << dataTable.size() << endl;

    // output user friendly file and data training file? also we want to split by position

    {
        // for now since we dont have enough data, just output projection data
        // format:  espn, yahoo, numberfire, nfl, sharks, cbs, pros, real
        ofstream myfile;
        myfile.open("dataset.csv");


        ofstream names;
        names.open("dataset-index.csv");

        for (auto& i : dataTable)
        {
            names << i.first << endl;
            bool first = true;
            for (auto val : i.second)
            {
                if (!first)
                {
                    myfile << ",";
                }
                else
                {
                    first = false;
                }
                myfile << val;
            }
            myfile << endl;
        }

        names.close();
        myfile.close();
    }
}

// general model for now: 70% yahoo, 10% espn, 10% cbs, 10% stats
// basically 60% yahoo, 40% fpros
// for QB: probably use close to even average of yahoo, cbs, stats, numberfire
// def probably the same for now?

unordered_map<string, float> parseProsStats()
{
    unordered_map<string, float> results;
    for (int i = 0; i <= 4; i++)
    {
        ostringstream stream;
        stream << "fpproj-" << i;
        stream << ".csv";
        ifstream file(stream.str());
        vector<string> tokens = getNextLineAndSplitIntoTokens(file);
        while (tokens.size() >= 6)
        {
            normalizeName(tokens[0]);
            int pos = i;
            ostringstream id;
            float proj = stof(tokens[tokens.size() - 1]);
            float rec = 0.f;
            if (i == 0)
            {
            }
            else if (i == 1 || i == 2)
            {
                rec = stof(tokens[4]);
            }
            else if (i == 3)
            {
                rec = stof(tokens[1]);
            }

            proj += rec * .5;
            /*
            float payds = 0.f;
            float patds = 0.f;
            float ints = 0.f;
            float ruyds = 0.f;
            float rutds = 0.f;
            float fls = 0.f;
            float rec = 0.f;
            float reyds = 0.f;
            float retds = 0.f;
            if (i == 0)
            {
                payds = stof(tokens[3]);
                patds = stof(tokens[4]);
                ints = stof(tokens[5]);
                ruyds = stof(tokens[7]);
                rutds = stof(tokens[8]);
                fls = stof(tokens[9]);
            }
            else if (i == 1 || i == 2)
            {
                ruyds = stof(tokens[2]);
                rutds = stof(tokens[3]);
                rec = stof(tokens[4]);
                reyds = stof(tokens[5]);
                retds = stof(tokens[6]);
                fls = stof(tokens[7]);
            }
            else if (i == 3)
            {
                rec = stof(tokens[1]);
                reyds = stof(tokens[2]);
                retds = stof(tokens[3]);
                fls = stof(tokens[4]);
            }
            float proj = payds * .04 + patds * 4 + ints * -1 + ruyds * .1 + rutds * 6 + fls * -2 + rec * .5 + reyds * .1 + retds * 6;
            // may also want to "prune" projections here
            */
            if (tokens[0] != "david johnson" || pos == 1)
            {
                if (results.find(tokens[0]) != results.end())
                {
                    cout << tokens[0] << endl;
                    if (tokens[0] == "cleveland")
                    {
                        // player named cleveland
                        results[tokens[0]] = proj;
                    }
                    //if (tokens[0] != "ty montgomery" && tokens[0] != "daniel brown" && tokens[0] != "neal sterling")
                    else if (tokens[0] != "ty montgomery" && proj > 5)
                    {
                        throw invalid_argument("Invalid player: " + tokens[0]);
                    }
                }
                else
                {
                    results.emplace(tokens[0], proj);
                }
            }
            tokens = getNextLineAndSplitIntoTokens(file);
        }
    }

    //cout << results.size() << endl;
    return results;
}

unordered_map<string, float> parseYahooStats()
{
    unordered_map<string, float> results;
    for (int i = 0; i <= 4; i++)
    {
        ostringstream stream;
        stream << "yahooproj-" << i;
        stream << ".csv";
        ifstream file(stream.str());
        vector<string> tokens = getNextLineAndSplitIntoTokens(file);
        while (tokens.size() >= 10)
        {
            normalizeName(tokens[0]);
            int pos = i;
            ostringstream id;
            float proj = stof(tokens[1]);
            float rec = pos < 4 ? stof(tokens[9]) : 0.f;
            proj += rec * .5;
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
            float proj = payds * .04 + patds * 4 + ints * -1 + ruyds * .1 + rutds * 6 + fls * -2 + rec * .5 + reyds * .1 + retds * 6 + rettds *6 + tpconv * 2;*/
            // may also want to "prune" projections here

            if (tokens[0] != "david johnson" || pos == 1)
            {
                if (results.find(tokens[0]) != results.end())
                {
                    cout << tokens[0] << endl;;
                    //if (tokens[0] != "ty montgomery" && tokens[0] != "daniel brown" && tokens[0] != "neal sterling")
                    if (tokens[0] != "ty montgomery" && proj > 5 && results.find(tokens[0])->second != proj)
                    {
                        throw invalid_argument("Invalid player: " + tokens[0]);
                    }
                }
                else
                {
                    results.emplace(tokens[0], proj);
                }
            }
            tokens = getNextLineAndSplitIntoTokens(file);
        }
    }

    //cout << results.size() << endl;
    return results;
}